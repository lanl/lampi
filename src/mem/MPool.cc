#include "MPool.h"

MPool::MPool(void *p, size_t sz, void *q, size_t q_sz)
{
    start = p;
    size = sz;
    alloc = 0;
}

MPool::~MPool()
{

}

int MPool::allocate(size_t sz, void **p, int align, int locality,
        size_t *act_sz)
{
    int ret;
    if (alloc == 0) {
        return -1;
    }
    ret = alloc->allocate(sz, p, align, locality, act_sz);
    return ret;
}

int MPool::free(void *p, int locality)
{
    int ret;
    if (alloc == 0) {
        return -1;
    }
    ret = alloc->free(p, locality);
    return ret;
}

int MPool::setAllocator(Allocator *a)
{
    if (alloc == 0) {
        alloc = a;
        return 0;
    }

    return -1;
}

int MPool::grow(size_t sz, size_t *act_sz) 
{
    return 0;
}
