#ifndef _PrivateMPool_h_
#define _PrivateMPool_h_

#include "MPool.h"

class PrivateMPool : public MPool
{
    public:
        // Methods
        PrivateMPool(void *p, size_t sz, void *q = 0,
                size_t q_sz = 0);
        PrivateMPool(size_t sz);
        ~PrivateMPool();
        int grow(size_t sz, size_t *act_sz = 0);

    private:
        bool ownPool;
};

#endif
