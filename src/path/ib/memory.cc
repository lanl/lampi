#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include <vapi_common.h>
}

#include "state.h"

bool ib_memory_registration_cache::register_mr(void *addr, size_t size,
    VAPI_mrw_acl_t acl, int hca_index, bool can_evict)
{
    bool result = false;
    VAPI_ret_t vapi_result;
    struct ib_reg_info reginfo;
    struct ib_mem_info *victim;

    if (ib_state.hca[hca_index].cap.max_mr_size < (u_int64_t)size) {
        ulm_warn(("register_mr(addr %p hca_index %d) region too big"
            " (%ld bytes), max allowed is %ld bytes!\n", addr, hca_index,
            (long)size, (long)ib_state.hca[hca_index].cap.max_mr_size));
        return result;
    }
        
    reginfo.registered = true;
    reginfo.can_evict = can_evict;
    reginfo.hca_index = hca_index;
    reginfo.request.type = VAPI_MR;
    reginfo.request.start = (VAPI_virt_addr_t)((unsigned long)addr);
    reginfo.request.size = size;
    reginfo.request.pd_hndl = ib_state.hca[hca_index].pd;
    reginfo.request.acl = acl;
    
    vapi_result = VAPI_register_mr(ib_state.hca[hca_index].handle,
        &reginfo.request, &reginfo.handle, &reginfo.reply);

    if (vapi_result != VAPI_OK) {
        if (vapi_result == VAPI_EAGAIN) {
            /* find another MR to victimize, deregister it, and try again... */
            victim = find_lu(hca_index);
            if (victim) {
                deregister_mr(victim, hca_index, true);
                vapi_result = VAPI_register_mr(ib_state.hca[hca_index].handle,
                    &reginfo.request, &reginfo.handle, &reginfo.reply);
                if (vapi_result != VAPI_OK) {
                    if (vapi_result == VAPI_EAGAIN) {
                        return result;
                    }
                    else {
                        ulm_exit((-1, "VAPI_register_mr() for HCA %d returned %s\n",
                            hca_index, VAPI_strerror(vapi_result)));
                    }
                }
                thrash_count_m++;
            }
            else {
                return result;
            }
        }
        else {
            ulm_exit((-1, "VAPI_register_mr() for HCA %d returned %s\n",
                hca_index, VAPI_strerror(vapi_result)));
        }
    }

    /* now cache this MR data */
    result = cache(addr, size, &reginfo);
    if (!result) {
        vapi_result = VAPI_deregister_mr(ib_state.hca[hca_index].handle,
            reginfo.handle);
        if (vapi_result != VAPI_OK) {
            ulm_exit((-1, "VAPI_deregister_mr() for HCA %d returned %s\n",
                hca_index, VAPI_strerror(vapi_result)));
        }
    }

    return result;
}

bool ib_memory_registration_cache::reregister_mr(struct ib_mem_info *meminfo,
    int hca_index)
{
    VAPI_ret_t vapi_result;
    bool result = false;
    struct ib_mem_info *infop;
    struct ib_reg_info *p;
    int i;

    if (is_cached(meminfo, 0, &infop)) {
        for (i = 0; i < infop->entries_used; i++) {
            if (infop->reg_array[i].hca_index == hca_index) {
                p = &(infop->reg_array[i]);
                if (p->registered) {
                    result = true;
                    break;
                }
                else {
                    vapi_result = VAPI_register_mr(ib_state.hca[hca_index].handle,
                        &(p->request), &(p->handle), &(p->reply));
                    if (vapi_result == VAPI_OK) {
                        p->registered = true;
                        result = true;
                        break;
                    }
                    else if (vapi_result != VAPI_EAGAIN) {
                        ulm_exit((-1, "VAPI_register_mr() for HCA %d returned %s\n",
                            hca_index, VAPI_strerror(vapi_result)));
                    }
                }
            }
        } 
    }

    return result;
}

void ib_memory_registration_cache::deregister_mr(void *addr, size_t size,
    int hca_index, bool leave_cache_entry)
{
    struct ib_mem_info *infop;
    
    if (is_cached(addr, size, 0, &infop)) {
        deregister_mr(infop, hca_index, leave_cache_entry);
    }
}

void ib_memory_registration_cache::deregister_mr(struct ib_mem_info *meminfo,
    int hca_index, bool leave_cache_entry)
{
    VAPI_ret_t vapi_result;
    struct ib_reg_info *infop = 0;
    int i;

    for (i = 0; i < meminfo->entries_used; i++) {
        if (meminfo->reg_array[i].hca_index == hca_index) {
            infop = &(meminfo->reg_array[i]);
            break;
        }
    }

    if (infop && infop->registered) {
        vapi_result = VAPI_deregister_mr(ib_state.hca[hca_index].handle,
            infop->handle);    
        if (vapi_result != VAPI_OK) {
            ulm_exit((-1, "VAPI_deregister_mr() for HCA %d returned %s\n",
                hca_index, VAPI_strerror(vapi_result)));
        }
        infop->registered = false;
    }

    if (!leave_cache_entry && infop) {
        uncache(meminfo, infop);
    }
}

/* called at MPI_Finalize time... */
void ib_memory_registration_cache::deregister_all_mr(void)
{
    VAPI_ret_t vapi_result;
    struct ib_mem_info *p;
    struct ib_reg_info *q;
    int i, j;

    for (i = 0; i < used_m; i++) {
        p = &(array_m[i]);
        for (j = 0; j < p->entries_used; j++) {
            q = &(p->reg_array[j]);
            if (q->registered) {
                vapi_result = VAPI_deregister_mr(ib_state.hca[q->hca_index].handle,
                    q->handle);
                if (vapi_result != VAPI_OK) {
                    ulm_warn(("VAPI_deregister_mr() for HCA %d returned %s\n",
                        q->hca_index, VAPI_strerror(vapi_result)));
                }
                q->registered = false;
            }
        }
    }
}

struct ib_memory_allocator::alloced_mem_head ib_memory_allocator::allocations_template_m[] = {
    { 64, 0 },
    { 256, 0 },
    { 1024, 0 },
    { 4096, 0 },
    { 32768, 0},
    { 131072, 0 },
    { 524288, 0 },
    { 2097152, 0 },
    { 8388608, 0 },
    { 16777216, 0 }
};

void ib_memory_allocator::init(unsigned long long csize)
{
    int i;

#ifndef ENABLE_UNIT_TEST
    for (i = 0; i < ib_state.num_active_hcas; i++) {
        if ((i == 0) || 
            (ib_state.hca[ib_state.active_hcas[i]].cap.max_mr_size < max_chunksize_m)) {
            max_chunksize_m = ib_state.hca[ib_state.active_hcas[i]].cap.max_mr_size;
        }
    }
#else
    max_chunksize_m = 0x10000000;
#endif

    current_chunksize_m = csize;
    if (current_chunksize_m > max_chunksize_m) { current_chunksize_m = max_chunksize_m; }

    buckets_m = (struct alloced_mem_head *)::malloc(sizeof(allocations_template_m));
    memcpy(buckets_m, allocations_template_m, sizeof(allocations_template_m));
    num_buckets_m = sizeof(allocations_template_m) / sizeof(struct alloced_mem_head);

    for (i = 0, largest_bucket_size_m = 0; i < num_buckets_m; i++) {
        if (buckets_m[i].bucket_size > current_chunksize_m) {
            num_buckets_m = i;
            break;
        }
        else if (buckets_m[i].bucket_size > largest_bucket_size_m) {
            largest_bucket_size_m = buckets_m[i].bucket_size;
        }
    }

    inited_m = true;
    return;
}

void *ib_memory_allocator::malloc(size_t requested_size)
{
    void *result = 0;
    size_t size;
    struct free_mem_desc *p;
    int i;

    if (0 == requested_size) { return result; }

    if (!inited_m) { init(); }

#ifndef ENABLE_UNIT_TEST
    if (0 == ib_state.num_active_hcas) { return ::malloc(requested_size); }
#endif
    
    size = requested_size + LAMPI_IB_MEM_ALLOC_HIDDEN;

    if (size <= largest_bucket_size_m) {
        for (i = 0; i < num_buckets_m; i++) {
            if (buckets_m[i].bucket_size >= size) {
                if ((buckets_m[i].free_ptr != 0) || get_more_memory(i)) {
                    p = buckets_m[i].free_ptr;
                    buckets_m[i].free_ptr = p->next;
                    result = ((char *)p + LAMPI_IB_MEM_ALLOC_HIDDEN);
                    (p->descp->chunk->free_count)--;
                }
                break;
            }
        }
    }
    else {
        p = get_chunk(size);
        result = ((char *)p + LAMPI_IB_MEM_ALLOC_HIDDEN);
        (p->descp->chunk->free_count)--;
    }

    return result;
}

void ib_memory_allocator::free(void *addr)
{
    struct free_mem_desc *p, *tmp;
    struct alloced_mem_desc *q;
    struct chunk_desc *r;
    
    if (!addr) { return; }

    if (!inited_m) { init(); }

#ifndef ENABLE_UNIT_TEST
    if (0 == ib_state.num_active_hcas) { return ::free(addr); }
#endif

    p = (struct free_mem_desc *)((char *)addr - LAMPI_IB_MEM_ALLOC_HIDDEN); 
    q = p->descp;
    r = q->chunk;

    if (q->bucket_index >= 0) {
        tmp = buckets_m[q->bucket_index].free_ptr;
        buckets_m[q->bucket_index].free_ptr = p;
        p->next = tmp;
        (r->free_count)++;
    }
    else {
        (r->free_count)++;
        if (r->free_count == r->total_count) {
            free_chunk(r);
        }
    }
    return;
}

struct ib_memory_allocator::free_mem_desc *ib_memory_allocator::get_chunk(size_t size)
{
    struct free_mem_desc *result = 0;
#ifndef ENABLE_UNIT_TEST
    int i;
#endif

    struct chunk_desc *chunk_desc = 
        (struct chunk_desc *)::malloc(sizeof(struct chunk_desc));
    struct alloced_mem_desc *mem_desc = 
        (struct alloced_mem_desc *)::malloc(sizeof(struct alloced_mem_desc));
    void *chunk = ::malloc(size); 

    if (!chunk_desc || !mem_desc || !chunk) {
        if (chunk_desc) { ::free(chunk_desc); }
        if (mem_desc) { ::free(mem_desc); }
        if (chunk) { ::free(chunk); }
        return result;
    }

#ifndef ENABLE_UNIT_TEST
    /* register memory */
    for (i = 0; i < ib_state.num_active_hcas; i++) {
        ib_state.mr_cache.register_mr(chunk, size, 
            VAPI_EN_LOCAL_WRITE | VAPI_EN_REMOTE_WRITE | VAPI_EN_REMOTE_READ,
            ib_state.active_hcas[i], true);
    }
#endif

    /* fill out record */
    mem_desc->offset = 0;
    mem_desc->length = size;
    mem_desc->bucket_size = 0;
    mem_desc->bucket_index = -1;
    mem_desc->chunk = chunk_desc;
    mem_desc->next = 0;

    chunk_desc->addr = chunk;
    chunk_desc->size = size;
    chunk_desc->free_count = 1;
    chunk_desc->total_count = 1;
    chunk_desc->alloced_mem = mem_desc;

    /* add chunk descriptor to list */
    chunk_desc->next = chunks_m;
    chunks_m = chunk_desc;

    /* store free record info. in memory */
    result = (struct free_mem_desc *)chunk;
    result->descp = mem_desc;
    result->next = 0;
    
    return result;
}

/* this chunk has not been sub-allocated - no need to remove free list entries */
void ib_memory_allocator::free_chunk(struct ib_memory_allocator::chunk_desc *chunk)
{
    struct chunk_desc *p = chunks_m;
    struct alloced_mem_desc *q, *r;
#ifndef ENABLE_UNIT_TEST
    int i;
#endif

    /* remove chunk from chunks_m list */
    while (p && (p->next != chunk)) {
        p = p->next;
    }
    if (!p) { return; }

    p->next = chunk->next;
 
    /* free all alloced_mem structures */
    q = chunk->alloced_mem;
    while (q) {
        r = q->next;
        ::free(q);
        q = r;
    }
    
#ifndef ENABLE_UNIT_TEST
    /* unregister memory */
    for (i = 0; i < ib_state.num_active_hcas; i++) {
        ib_state.mr_cache.deregister_mr(chunk->addr, (size_t)chunk->size,  
            ib_state.active_hcas[i], false);
    }
#endif

    /* free memory and chunk descriptor */
    ::free(chunk->addr);
    ::free(chunk);

    return;
}

bool ib_memory_allocator::get_more_memory(int bucket_index)
{
    bool result = false;
    struct chunk_desc *p = chunks_m;
    struct alloced_mem_desc *q, *r, *s;
    struct free_mem_desc *t;
    int num_needed;
    size_t size_needed;

    s = (struct alloced_mem_desc *)::malloc(sizeof(struct alloced_mem_desc));
    if (!s) { return result; }

    s->bucket_size = buckets_m[bucket_index].bucket_size;
    s->bucket_index = bucket_index;
    
    num_needed  = current_chunksize_m / buckets_m[bucket_index].bucket_size; 
    if (num_needed > LAMPI_IB_MEM_ALLOC_MAX_BUFS_PER_ALLOC) {
        num_needed = LAMPI_IB_MEM_ALLOC_MAX_BUFS_PER_ALLOC;
    }
    size_needed = num_needed * buckets_m[bucket_index].bucket_size;

    /* search the chunks to find size_needed bytes */
    while (p) {
        unsigned long long this_chunk_size = p->size;
        q = r = p->alloced_mem;
        while (q) {
            if  ((r != q) && 
                ((q->offset - (r->offset + r->length)) >= size_needed)) {
                r->next = s;
                s->next = q;
                s->offset = r->offset + r->length;
                s->length = size_needed;
                s->chunk = p;
                result = true;
                break;
            }
            this_chunk_size -= q->length;
            r = q;
            q = q->next;
        }
        if (result) { break; }
        if (this_chunk_size >= size_needed) {
            r->next = s;
            s->next = q;
            s->offset = r->offset + r->length;
            s->length = size_needed;
            s->chunk = p;
            result = true;
            break;
        }
        p = p->next;
    }

    /* try to get a new chunk */
    if (!result) {
        t = get_chunk(current_chunksize_m);
        if (t) {
            ::free(s);
            s = t->descp;
            s->offset = 0;
            s->length = size_needed;
            s->bucket_size = buckets_m[bucket_index].bucket_size;
            s->bucket_index = bucket_index;
            p = s->chunk;
            p->total_count = 0;
            p->free_count = 0;
            result = true;
        }
    }

    /* now create the free list structures for each buffer */
    if (result) {
        char *tmp = (char *)p->addr + s->offset;
        while (num_needed--) {
            t = (struct free_mem_desc *)tmp;
            t->descp = s;
            t->next = buckets_m[bucket_index].free_ptr;
            buckets_m[bucket_index].free_ptr = t;
            (p->total_count)++;
            (p->free_count)++;
            tmp += s->bucket_size;
        }
    }

    return result;
}
