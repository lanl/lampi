/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "FirstFitAlloc.h"

FirstFitAllocator::FirstFitAllocator(MPool *p, size_t da)
        : Allocator(p, da)
{
    Block *b;
    b = new Block();
    b->size = pool->size;
    b->inUse = false;
    b->pool_loc = pool->start;
    b->usr_p = b->pool_loc;
    b->locality = -1;
    blocks.Append(b);
}

FirstFitAllocator::~FirstFitAllocator()
{
    Block *b, *c;

    b = (Block *) blocks.begin();
    while (b != (Block *) blocks.end()) {
        c = (Block *) b->next;
        delete b;
        b = c;
    }
}

int FirstFitAllocator::allocate(size_t sz, void **p, size_t align,
        int locality, size_t *act_sz)
{
    size_t a, nbytes, block_sz;

    if (getBlock(sz, p, align, locality, act_sz) == 0) {
        return 0;
    }

    // not enough blocks... try to coalesce 
    // set up alignment, number of bytes wrt alignment
    a = (align == 0) ? default_align : align;
    nbytes = ((sz + a - 1) / a) * a;
    block_sz = ((sizeof(Block) + a - 1) / a) * a; 
    nbytes += block_sz;
    if (coalesce() >= nbytes) {
        return (getBlock(sz, p, align, locality, act_sz));
    }

    // coalescing didn't work, try growing the pool
    if (growPool() >= nbytes) {
        return getBlock(sz, p, align, locality, act_sz);
    }

    return -1;
}

int FirstFitAllocator::getBlock(size_t sz, void **p, size_t align, 
        int locality, size_t *act_sz)
{
    size_t a, nbytes, block_sz, u, v;
    Block *b, *c;

    // set up alignment, number of bytes wrt alignment
    a = (align == 0) ? default_align : align;
    nbytes = ((sz + a - 1) / a) * a;
    block_sz = ((sizeof(Block) + a - 1) / a) * a; // header 
    nbytes += block_sz;

    // run through list, checking size, and locality if 
    // specified
    for (b = (Block *) blocks.begin(); 
            b != (Block *) blocks.end(); 
            b = (Block *) b->next) {
        if (b->inUse) {
            continue;
        }

        if (b->size >= nbytes && b->locality == locality) {
            u = (size_t) b->pool_loc;
            v = ((u + a - 1) / a) * a;
            if (b->size == nbytes + v - u) {
                b->usr_p = (void *) (v + block_sz);
                b->inUse = true;
                *p = b->usr_p;
                if (act_sz) {
                    *act_sz = nbytes - block_sz;
                }
                return 0;
            }
            else if (b->size > nbytes + v - u) {
                // block is larger than necessary, split in two
                c = new Block;
                c->size = b->size - nbytes - v + u;
                c->locality = locality;
                c->pool_loc = (void *) (v + nbytes);
                c->usr_p = c->pool_loc;
                //blocks.insert(b, c);
                b->size = nbytes + v - u;
                b->usr_p = (void *) (v + block_sz);
                b->locality = locality;
                b->inUse = true;
                *p = b->usr_p;
                if (act_sz) {
                    *act_sz = nbytes - block_sz;
                }
                return 0;
            }
        }
    }

    // No blocks match... 
    return -1;
}

int FirstFitAllocator::free(void *p, int locality)
{
    // Note that we may want to perform coalescing here
    Block *b;

    for (b = (Block *) blocks.begin();
            b != (Block *) blocks.end();
            b = (Block *) b->next) {
        if (p == b->usr_p) {
            b->inUse = false;
            b->locality = locality;
            return 0;
        }
    }

    // Didn't find the block...
    return -1;
}

int FirstFitAllocator::check()
{
    return 0;
}

size_t FirstFitAllocator::coalesce() 
{
    Block *b, *c;
    unsigned char *u, *v;
    size_t ret = 0;

    // march through list and join adjacent free blocks
    for (b = (Block *) blocks.begin(); 
            b != (Block *) blocks.end(); 
            b = (Block *) b->next) {
        u = (unsigned char *) b;
        v = (unsigned char *) b->next;
        if (b->inUse 
                || ((Block *) b->next)->inUse
                || b->next == blocks.end() 
                || b->locality != ((Block *) b->next)->locality
                || u + b->size != v) {
            continue;
        }

        // merge blocks
        b->size += ((Block *) b->next)->size;
        if (ret < b->size) {
            ret = b->size;
        }
        c = (Block *) b->next;
        blocks.RemoveLink((Links_t *) b->next);
        if (c) {
            delete c;
            c = 0;
        }
    }

    return ret;
}

size_t FirstFitAllocator::growPool()
{ 
    return 0;
}
