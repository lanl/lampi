#ifndef _FirstFitAlloc_h_
#define _FirstFitAlloc_h_

#include "Allocator.h"
#include "MPool.h"
#include "util/Links.h"
#include "util/DblLinkList.h"

class Block : public Links_t
{
    public:
        void   *pool_loc; // starting address of block in pool
        void   *usr_p;    // pointer passed to user
        size_t size;      // size of this block
        int    locality;  // locality tag
        bool   inUse;     // flag indicating if user has block
};

class FirstFitAllocator : public Allocator
{
    public:
        // Methods
        FirstFitAllocator(MPool *p, size_t da);
        ~FirstFitAllocator();
        int allocate(size_t sz, void **p, size_t align = 0,
                int locality = -1, size_t *act_sz = 0);
        int free(void *p, int locality = -1);
        int check();

    private:
        // Methods
        // getBlock attepts to get a block of size sz
        int getBlock(size_t sz, void **p, size_t align, 
                int locality, size_t *act_sz);
        
        // coalesce steps through the blocks and merges 
        // adjacent free ones, returning the size of the
        // largest block.
        size_t coalesce();

        // growPool attemts to actually increase the size
        // of the memoryPool, returning the increase in size.
        size_t growPool();

        // State
        DoubleLinkList blocks;
};

#endif
