#ifndef IB_MEMORY_H
#define IB_MEMORY_H

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <vapi.h>
}

struct ib_reg_info {
    bool registered;
    bool can_evict;
    int hca_index;
    VAPI_mr_hndl_t handle;
    VAPI_mr_t request;
    VAPI_mr_t reply;
};

/* this should be no smaller than the max. number of 
 * IB HCAs supported, set by LAMPI_MAX_IB_HCAS
 */
#define LAMPI_MIN_IB_REG_ENTRIES 2

struct ib_mem_info {
    void *start;
    void *end;
    int entries_used;
    unsigned int lu_count;
    unsigned int ref_count[LAMPI_MIN_IB_REG_ENTRIES];
    struct ib_reg_info reg_array[LAMPI_MIN_IB_REG_ENTRIES];
};

/* protected by thread lock in ib_state */

/* this class is designed to handle non-overlapping
 * memory regions that are registered a single time
 * for each active HCA present...
 */
class ib_memory_registration_cache {

    private:

        int size_m;
        int used_m;
        int growby_m;
        int shrinkby_m;
        struct ib_mem_info *array_m;
        struct ib_mem_info *hint_m;
        unsigned long long thrash_count_m;

        static int compare(const void *a, const void *b) 
        {
            struct ib_mem_info *first = (struct ib_mem_info *)a;
            struct ib_mem_info *second = (struct ib_mem_info *)b;

            if (first->start >= second->start) {
                if (first->end <= second->end) {
                    return 0;
                }
                else {
                    return 1;
                }
            }
            else {
                return -1;
            }
        }

    public:

        ib_memory_registration_cache(int size = 0, 
            int growby = 100, 
            int shrinkby = 200) 
        {
            size_m = size;
            growby_m = growby;
            shrinkby_m = shrinkby;
            used_m = 0;
            array_m = 0;
            hint_m = 0;
            thrash_count_m = 0;

            if (size_m) {
                array_m = (struct ib_mem_info *)malloc(size_m * 
                    sizeof(struct ib_mem_info));
                if (!array_m) { size_m = 0; }
            }
        }

        ~ib_memory_registration_cache()
        {
            if (array_m) { free(array_m); array_m = 0; size_m = 0; }
        }

        bool register_mr(void *addr, size_t size, VAPI_mrw_acl_t acl, 
            int hca_index = -1, bool can_evict = true);

        bool reregister_mr(struct ib_mem_info *meminfo, int hca_index);

        void deregister_mr(void *addr, size_t size, int hca_index = -1,
            bool leave_cache_entry = false);

        void deregister_mr(struct ib_mem_info *meminfo, int hca_index = -1, 
            bool leave_cache_entry = false);
        
        void deregister_all_mr(void);

        unsigned long long get_thrash_count(void) { return thrash_count_m; }

        void clear_thrash_count(void) { thrash_count_m = 0; }

        bool is_registered(void *addr, size_t size, 
            int hca_index, struct ib_reg_info *reginfo)
        {
            bool result = false;
            struct ib_mem_info *infop;
            int i;

            if (is_cached(addr, size, 0, &infop)) {
                for (i = 0; i < infop->entries_used; i++) {
                    if (infop->reg_array[i].hca_index == hca_index) {
                        result = true;
                        if (!infop->reg_array[i].registered && 
                            !reregister_mr(infop, hca_index)) {
                            result = false; 
                        }
                        if (result) {
                            (infop->ref_count[i])++;
                            memcpy(reginfo, &(infop->reg_array[i]), 
                                sizeof(struct ib_reg_info));
                        }
                    }
                } 
            }
        
            return result;
        }

        bool increment_ref_count(void *addr, size_t size, int hca_index)
        {
            bool result = false;
            struct ib_mem_info *infop;
            int i;

            if (is_cached(addr, size, 0, &infop)) {
                for (i = 0; i < infop->entries_used; i++) {
                    if (infop->reg_array[i].hca_index == hca_index) {
                        result = true;
                        (infop->ref_count[i])++;
                        break;
                    }
                } 
            }
        
            return result;
        }

        bool decrement_ref_count(void *addr, size_t size, int hca_index)
        {
            bool result = false;
            struct ib_mem_info *infop;
            int i;

            if (is_cached(addr, size, 0, &infop)) {
                for (i = 0; i < infop->entries_used; i++) {
                    if (infop->reg_array[i].hca_index == hca_index) {
                        result = true;
                        (infop->ref_count[i])--;
                        break;
                    }
                } 
            }
        
            return result;
        }

        unsigned int get_ref_count(void *addr, size_t size, int hca_index)
        {
            struct ib_mem_info *infop;
            int i;

            if (is_cached(addr, size, 0, &infop)) {
                for (i = 0; i < infop->entries_used; i++) {
                    if (infop->reg_array[i].hca_index == hca_index) {
                        return infop->ref_count[i];
                    }
                } 
            }
        
            return 0;
        }

        bool is_cached(void *addr, size_t size)
        {
            return is_cached(addr, size, 0, 0);
        }

        bool is_cached(void *addr, 
            size_t size, 
            struct ib_mem_info *info,
            struct ib_mem_info **infop)
        {
            struct ib_mem_info key;

            key.start = addr;
            key.end = ((char *)key.start + size);

            return is_cached(&key, info, infop);
        }

        bool is_cached(struct ib_mem_info *key, 
            struct ib_mem_info *info, 
            struct ib_mem_info **infop)
        {
            struct ib_mem_info *p;

            if (used_m == 0) { return false; }

            if (hint_m) {
                if (compare(key, hint_m) == 0) {
                    if (info) {
                        memcpy(info, hint_m, sizeof(struct ib_mem_info));
                    }
                    if (infop) {
                        *infop = hint_m;
                    }
                    (hint_m->lu_count)++;
                    return true;
                }
            }

            p = (struct ib_mem_info *)bsearch(key, array_m, 
                (size_t)used_m, sizeof(struct ib_mem_info), 
                &compare);

            if (p) {
                if (info) {
                    memcpy(info, p, sizeof(struct ib_mem_info));
                }
                if (infop) {
                    *infop = p;
                }
                hint_m = p;
                (p->lu_count)++;
            }

            return (p) ? true : false;
        }

        bool cache(void *addr, size_t size, struct ib_reg_info *reginfo)
        {
            struct ib_mem_info *entry, *tmp_array;
            int i;

            if (is_cached(addr, size, 0, &entry)) {
                if (reginfo) {
                    if (entry->entries_used < LAMPI_MIN_IB_REG_ENTRIES) {
                        memcpy(&(entry->reg_array[entry->entries_used]), 
                            reginfo, sizeof(struct ib_reg_info));
                        (entry->entries_used)++;
                    }
                    else {
                        return false;
                    }
                }
            }
            else {
                if (used_m == size_m) {
                    tmp_array = (struct ib_mem_info *)realloc(array_m, 
                        sizeof(struct ib_mem_info) * (size_m + growby_m));
                    if (!tmp_array) { return false; }
                    array_m = tmp_array;
                    size_m += growby_m;
                }
                entry = &(array_m[used_m++]);
                entry->start = addr;
                entry->end = ((char *)entry->start + ((size > 0) ? size : 0));
                entry->entries_used = 0;
                entry->lu_count = 0;
                for (i = 0; i < LAMPI_MIN_IB_REG_ENTRIES; i++) { 
                    entry->ref_count[i] = 0; 
                }
                if (reginfo) {
                    (entry->entries_used)++;
                    memcpy(&(entry->reg_array[0]), reginfo, 
                        sizeof(struct ib_reg_info));
                }
                qsort(array_m, (size_t)used_m, sizeof(struct ib_mem_info), 
                    &compare);
                hint_m = 0;
            }

            return true;
        }

        void uncache(struct ib_mem_info *meminfo)
        {
            struct ib_mem_info *infop, *tmp_array;
            unsigned long index;
            int num_to_move;

            if (is_cached(meminfo, 0, &infop)) {
                index = ((unsigned long)infop - (unsigned long)array_m) / 
                    sizeof(struct ib_mem_info);
                num_to_move = used_m - index - 1;
                if (num_to_move > 0) {
                    memmove(infop, infop + 1, 
                        sizeof(struct ib_mem_info) * num_to_move);
                }
                used_m--;
                if (hint_m >= infop) {
                    hint_m = (hint_m == infop) ? 0 : (infop - 1);
                }
                if ((size_m - used_m) >= shrinkby_m) {
                    tmp_array = (struct ib_mem_info *)realloc(array_m,
                        sizeof(struct ib_mem_info) * (size_m - shrinkby_m));
                    if (tmp_array) {
                        if (tmp_array != array_m) { hint_m = 0; }
                        array_m = tmp_array;
                        size_m -= shrinkby_m;
                    }
                }
            }
            
            return;
        }

        void uncache(struct ib_mem_info *meminfo, struct ib_reg_info *reginfo)
        {
            struct ib_mem_info *infop;
            int i, num_to_move;

            if (is_cached(meminfo, 0, &infop)) {
                for (i = 0; i < infop->entries_used; i++) {
                    if (memcmp(reginfo, 
                        &(infop->reg_array[i]), 
                        sizeof(struct ib_reg_info)) == 0) {
                        num_to_move = infop->entries_used - i - 1;
                        if (num_to_move > 0) {
                            memmove(&(infop->reg_array[i]), &(infop->reg_array[i+1]), 
                                sizeof(struct ib_reg_info) * num_to_move);
                        }
                        (infop->entries_used)--;
                        break;
                    }
                } 
                if (infop->entries_used == 0) {
                    uncache(meminfo);
                }
            }

            return;
        }

        struct ib_mem_info *find_lu(int hca_index = -1, bool *try_again = 0)
        {
            struct ib_mem_info *result = 0;
            unsigned int min = (unsigned int)(-1);
            int i, j;
            bool match, ok_to_evict, result_ok = false;

            if (try_again) {
                *try_again = false;
            }

            for (i = 0; i < used_m; i++) {
                struct ib_mem_info *p = &(array_m[i]);
                if (p->lu_count <= min) { 
                    match = false;
                    ok_to_evict = true;
                    for (j = 0; j < p->entries_used; j++) {
                        if (((hca_index < 0) || 
                            (p->reg_array[j].hca_index == hca_index)) && 
                            p->reg_array[j].registered && 
                            p->reg_array[j].can_evict) {
                            match = true;
                            ok_to_evict = (p->ref_count[j] != 0) ? 
                                false : ok_to_evict;
                            if (hca_index >= 0) {
                                break;
                            }
                        }
                    }
                    if (match) {
                        if (ok_to_evict) {
                            result_ok = true;
                            result = p;
                            min = p->lu_count;
                        }
                        else if (!result || !result_ok ||   
                            (p->lu_count < min)) {
                            result_ok = false;
                            result = p;
                            min = p->lu_count;
                        }
                    }
                }
            }

            if (result && !result_ok) { 
                result = 0; 
                if (try_again) {
                    *try_again = true; 
                }
            }
            return result;
        }

        void dump(FILE *stream = stdout)
        {
            int i, j;

            fprintf(stream, "ib_memory_registration_cache at %p: array_m %p, size_m %d,"
                " used_m %d, growby_m %d, shrinkby_m %d\n", this, array_m, size_m, 
                used_m, growby_m, shrinkby_m);
            for (i = 0; i < used_m; i++) {
                fprintf(stream, "entry[%d]: start %p end %p (size %ld) entries_used %d lu_count %u\n",
                    i, array_m[i].start, array_m[i].end, ((unsigned long)array_m[i].end -
                    (unsigned long)array_m[i].start), array_m[i].entries_used,
                    array_m[i].lu_count);
                for (j = 0; j < array_m[i].entries_used; j++) {
                    /* print each registration info structure */
                }
            }

            return;
        }
};

/* must be greater than a pointer's worth of memory */
#define LAMPI_IB_MEM_ALLOC_HIDDEN   8
/* see template of bucket_sizes array in memory.cc before changing */
#define LAMPI_IB_MEM_ALLOC_CHUNKSIZE (16*1024*1024)
/* the max number of buffers of a given bucket size to allocate at one time */
#define LAMPI_IB_MEM_ALLOC_MAX_BUFS_PER_ALLOC 40

class ib_memory_allocator {
    
    private:

        /* forward declaration */
        struct alloced_mem_desc;

        struct chunk_desc {
            void *addr;
            unsigned long long size;
            unsigned long free_count;
            unsigned long total_count;
            struct alloced_mem_desc *alloced_mem;
            struct chunk_desc *next;
        };

        struct alloced_mem_desc {
            size_t offset;
            size_t length;
            size_t bucket_size;
            int bucket_index;
            struct chunk_desc *chunk;
            struct alloced_mem_desc *next;
        };

        struct free_mem_desc {
            struct alloced_mem_desc *descp;
            struct free_mem_desc *next;
        };

        struct alloced_mem_head {
            size_t bucket_size;
            struct free_mem_desc *free_ptr;
        };

        bool inited_m;
        unsigned long long max_chunksize_m;
        unsigned long long current_chunksize_m;
        size_t largest_bucket_size_m;
        int num_buckets_m;
        struct chunk_desc *chunks_m;
        struct alloced_mem_head *buckets_m;
        static struct alloced_mem_head allocations_template_m[];

        struct free_mem_desc *get_chunk(size_t size);

        void free_chunk(struct chunk_desc *chunk);

        bool get_more_memory(int bucket_index);

    public:

        ib_memory_allocator() 
        { 
            inited_m = false; 
            max_chunksize_m = 0;
            current_chunksize_m = 0;
            largest_bucket_size_m = 0;
            num_buckets_m = 0;
            chunks_m = 0;
            buckets_m = 0;
        }

        ~ib_memory_allocator() {}

        /* called after post-fork IB initialization */
        void init(unsigned long long csize = LAMPI_IB_MEM_ALLOC_CHUNKSIZE); 

        void *malloc(size_t size);

        void free(void *addr);

        void dump(FILE *stream = stdout)
        {
            struct chunk_desc *p = chunks_m;
            struct alloced_mem_desc *q;
            int i, j;

            fprintf(stream, "ib_memory_allocation at %p: inited_m %s max_chunksize_m %lld\n",
                this, inited_m ? "true" : "false", max_chunksize_m);
            fprintf(stream, "\tcurrent_chunksize_m %lld largest_bucket_size_m %ld num_buckets_m %d\n",
                current_chunksize_m, (long)largest_bucket_size_m, num_buckets_m);
            
            if (!inited_m) { return; }

            for (i = 0; i < num_buckets_m; i++) {
                fprintf(stream, "bucket[%d]: bucket_size %ld free ptrs avail: %s\n",
                    i, (long)buckets_m[i].bucket_size, 
                    (buckets_m[i].free_ptr != 0) ? "yes" : "no");
            }
            fprintf(stream,"\n");

            i = 0;
            while (p) {
                fprintf(stream, "chunk[%d]: addr %p size %lld free %ld total %ld\n",
                    i, p->addr, p->size, p->free_count, p->total_count);
                q = p->alloced_mem;
                j = 0;
                while (q) {
                    fprintf(stream, "\talloc[%d]: offset %ld length %ld bucket_size "
                        "%ld bucket_index %d\n", j, (long)q->offset, (long)q->length, 
                        (long)q->bucket_size, q->bucket_index);
                    q = q->next;
                    j++;
                }
                p = p->next;
                i++;
            }
        }
};

#endif
