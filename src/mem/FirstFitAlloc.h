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
