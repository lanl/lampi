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


#ifndef _BUCKET
#define _BUCKET

#include "util/Lock.h"
#include "mem/ULMStack.h"

// Class managing the memory buckets used by the allocator.

template <long MaxStackElements>
struct Bucket
{
    // element size for this bucket
    int segmentSize_m;

    // bucket lock
    Locks lock_m;

    // the stack.
    Stack<MaxStackElements> stack_m;

    // Number of elements, that is, number of ALLOCREGIONS for this bucket
    size_t nElements_m;

    // the TOTAL number of bytes in the bucket available for allocation
    size_t totalBytesInBucket_m;

    // the TOTAL number of bytes currently allocated from the bucket
    size_t totalBytesAllocated_m;

    // the TOTAL number of bytes available - e.g. total amount "granted" the stack
    size_t totalBytesAvailable_m;

    // minimum memory pushed onto the stack (in bytes)
    size_t minBytesPushedOnStack_m;

    // maximum memory pushed onto the stack (in bytes)
    size_t maxBytesPushedOnStack_m;

    // maximum number of times that a request for memory can fail
    long maxConsecReqFail_m;

    // consecutive number of times a request for memory has failed
    long consecReqFail_m;
};

#endif /* _BUCKET */
