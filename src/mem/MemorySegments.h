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


#ifndef _MEMORYSEGMENTS
#define _MEMORYSEGMENTS

#include "util/Lock.h"

/*
 * This is the base structure used to describe a "pool" of
 *  memory segments.  This structure specifies generic
 *  quantities (such as upper limit on the amount of memory
 *  that can be stored), but does not specify storage mode.
 */
template <class LinkListType>
class ListOfMemorySegments_t 
{
    public:
        // minimum memory pushed onto the free list (in bytes)
        ssize_t minBytesPushedOnFreeList_m;

        // maximum memory pushed onto the free list (in bytes)
        ssize_t maxBytesPushedOnFreeList_m;

        // amount of memory pushed onto the free list (in bytes)
        ssize_t bytesPushedOnFreeList_m;

        // maximum number of times in a row that a request for
        //   memory can fail
        long maxConsecReqFail_m;

        // consecutive number of times a request for memory has
        //   failed
        long consecReqFail_m;

        // lock
        Locks lock_m;

        // link list of free pointers
        LinkListType freeList_m;
};

#endif /* !_MEMORYSEGMENTS */
