/*
 * This file is part of LA-MPI
 *
 * Copyright 2002 Los Alamos National Laboratory
 *
 * This software and ancillary information (herein called "LA-MPI") is
 * made available under the terms described here.  LA-MPI has been
 * approved for release with associated LA-CC Number LA-CC-02-41.
 * 
 * Unless otherwise indicated, LA-MPI has been authored by an employee
 * or employees of the University of California, operator of the Los
 * Alamos National Laboratory under Contract No.W-7405-ENG-36 with the
 * U.S. Department of Energy.  The U.S. Government has rights to use,
 * reproduce, and distribute LA-MPI. The public may copy, distribute,
 * prepare derivative works and publicly display LA-MPI without
 * charge, provided that this Notice and any statement of authorship
 * are reproduced on all copies.  Neither the Government nor the
 * University makes any warranty, express or implied, or assumes any
 * liability or responsibility for the use of LA-MPI.
 * 
 * If LA-MPI is modified to produce derivative works, such modified
 * LA-MPI should be clearly marked, so as not to confuse it with the
 * version available from LANL.
 * 
 * LA-MPI is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * LA-MPI is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 */

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef _BUCKET
#define _BUCKET

#include "util/Lock.h"
#include "mem/ULMStack.h"

/*
 * This class is used to manage the memory buckets
 *   used by the allocator.
 */

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
    unsigned long totalBytesInBucket_m;

    // the TOTAL number of bytes currently allocated from the bucket
    unsigned long totalBytesAllocated_m;

    // the TOTAL number of bytes available - e.g. total amount "granted" the stack
    unsigned long totalBytesAvailable_m;

    // minimum memory pushed onto the stack (in bytes)
    size_t minBytesPushedOnStack_m;

    // maximum memory pushed onto the stack (in bytes)
    size_t maxBytesPushedOnStack_m;

    // maximum number of times in a row that a request for
    //   memory can fail
    long maxConsecReqFail_m;

    // consecutive number of times a request for memory has
    //   failed
    long consecReqFail_m;
};

#endif /* _BUCKET */
