#ifndef _Allocator_h
#define _Allocator_h

#include <stddef.h>

class MPool;

/* This is the base class that must be used for any LAMPI-compliant 
 * memory allocator 
 * 
 * Note that, as concrete classes, one first creates a MPool object,
 * then an Allocator object, passing a pointer to the MPool in via
 * the c'tor.  Finally, one calls MPool::setAllocator() with a pointer
 * to the Allocator as argument.  In this way, both the  MPool and
 * the Allocator are coupled.  This coupling is necessary as the
 * Allocator may need to grow the MPool to satisfy allocation
 * requests.
 */
class Allocator
{
public:
    /* Methods */
    /* -- Allocator C'tor
     *    p  - pointer to the Mpool that this allocator will manage
     *    da - default alignment for allocated memory, in bytes
     */
    Allocator(MPool *p, size_t da) 
        { 
            pool = p;
            default_align = da;
        }
    virtual ~Allocator() {}

    /* -- allocate
     *    sz       - number of bytes requested in allocated region
     *    p        - pointer to allocated region
     *    align    - alignment in bytes of region, if 0, default
     *               alignment will be used
     *    locality - memory locality of region
     *    act_sz   - acutal size of region in bytes
     */
    virtual int allocate(size_t sz, void **p, size_t align = 0,
                         int locality = 0, size_t *act_sz = 0) = 0;

    /* -- free
     *    p        - pointer to unused region
     *    locality - memory locality of unused region
     */
    virtual int free(void *p, int locality = 0) = 0;

    /* -- check
     *    Debugging method.  When the code is built with -DMEM_DEBUG,
     *    guard regions are added to the beginning and end of each 
     *    block of allocated memory.  This method checks all allocated
     *    blocks to ensure that the guard words remain intact, and 
     *    returns an error (as well as other information) if some are
     *    corrupted
     */
    virtual int check() = 0;

    /* State */
    MPool *pool;
    size_t default_align;
};

#endif
