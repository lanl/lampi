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
