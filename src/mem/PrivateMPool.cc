#include "PrivateMPool.h"

PrivateMPool::PrivateMPool(void *p, size_t sz, void *q,
        size_t q_sz) : MPool(p, sz, q, q_sz)
{
    ownPool = false;
}

PrivateMPool::PrivateMPool(size_t sz)
{
    alloc = a;
    size = sz;
    start = (void *) new unsigned char[sz];
    ownPool = true; 
}

~PrivateMPool()
{
    unsigned char *p = (unsigned char *start);
    if (ownPool && start) {
        free [] p;
        start = 0;
    }
}

int PrivateMPool::grow(size_t sz; size_t *act_sz)
{
    return 0;
}
