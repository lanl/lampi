#ifndef _MPool_h_
#define _MPool_h_

#include "Allocator.h"

class MPool 
{
public:
    // Methods
    // C'tor:
    //   p    starting address of region to be managed by pool
    //   sz   size of region
    //   a    Allocator object to manage pool
    //   q    optional pointer to separate region that the control
    //        structure will reside in
    //   q_sz size of region for control structure
    MPool(void *p, size_t sz, void *q = 0, size_t q_sz = 0);
    virtual ~MPool();

    // allocate:
    //   sz       size of region to allocate
    //   p        pointer to region allocated
    //   align    alignment of region
    //   locality memory locality of region
    //   act_sz   actual size of region allocated, may be larger 
    //            than sz
    int allocate(size_t sz, void **p, int align = 0, 
                 int locality = 0, size_t *act_sz = 0);
    int free(void *p, int locality = 0);
    int setAllocator(Allocator *a);

    // grow
    //   sz     size to grow pool by
    //   act_sz actual size pool has grown
    virtual int grow(size_t sz, size_t *act_sz = 0) = 0;

    // State
    Allocator *alloc;
    void *start;
    size_t size;
};

#endif
