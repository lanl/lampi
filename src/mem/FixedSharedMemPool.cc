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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/mmap_params.h"
#include "mem/FixedSharedMemPool.h"
#include "mem/ZeroAlloc.h"
#include "ulm/errors.h"
#include "os/numa.h"

// initialize the memory pools

void FixedSharedMemPool::init(ssize_t initialAllocation,
                              ssize_t minAllocationSize,
                              int numberPools,
                              int nArrayElementsToAdd,
                              bool applyMemAffinity)
{
    // set pools as ok to use
    poolOkToUse_m = true;

    // set memory affinity flag
    applyMemoryAffinity = applyMemAffinity;

    // set number of pools to use
    assert(numberPools > 0);
    nPools_m = numberPools;

    // set minimum allocation size
    minAllocationSize_m = minAllocationSize;
    if (minAllocationSize_m < getpagesize())
        minAllocationSize_m = getpagesize();

    // set nArrayElementsToAdd_m
    assert(nArrayElementsToAdd > 0);
    nArrayElementsToAdd_m = nArrayElementsToAdd;

    // allocate *memSegments_m
    memSegments_m = (MemorySegment_t **)
        ulm_malloc(sizeof(MemorySegment_t *) * nPools_m);
    if (!memSegments_m) {
        ulm_exit((-1,
                  "FixedSharedMemPool::init Unable to allocate memory for "
                  "memSegments_m, requested %ld bytes, errno %d\n",
                  sizeof(MemorySegment_t *) * nPools_m, errno));
    }
    // allocate nMemorySegments_m
    nMemorySegments_m = (int *) ulm_malloc(sizeof(int) * nPools_m);
    if (!nMemorySegments_m) {
        ulm_exit((-1,
                  "FixedSharedMemPool::init Unable to allocate memory for "
                  "nMemorySegments_m, requested %ld bytes, errno %d\n",
                  sizeof(int) * nPools_m, errno));
    }
    for (int pool = 0; pool < nPools_m; pool++)
        nMemorySegments_m[pool] = 0;

    // allocate nMemorySegmentsInArray_m
    nMemorySegmentsInArray_m = (int *) ulm_malloc(sizeof(int) * nPools_m);
    if (!nMemorySegmentsInArray_m) {
        ulm_exit((-1,
                  "FixedSharedMemPool::init Unable to allocate memory for "
                  "nMemorySegmentsInArray, requested %ld bytes, errno %d\n",
                  sizeof(int) * nPools_m, errno));
    }
    for (int pool = 0; pool < nPools_m; pool++)
        nMemorySegmentsInArray_m[pool] = 0;

    // allocate memSegments_m
    for (int pool = 0; pool < nPools_m; pool++) {
        memSegments_m[pool] = (MemorySegment_t *)
            ulm_malloc(sizeof(MemorySegment_t) * nArrayElementsToAdd_m);
        if (!memSegments_m[pool]) {
            ulm_exit((-1,
                      "FixedSharedMemPool::init Unable to allocate memory "
                      "for memSegments_m[], requested %ld bytes, errno %d\n",
                      sizeof(MemorySegment_t) * nArrayElementsToAdd_m,
                      errno));
        }
        nMemorySegmentsInArray_m[pool] = nArrayElementsToAdd_m;
    }

    // generate initial allocation
    if (initialAllocation > 0) {
        // make sure allocation is large enough
        if (initialAllocation < minAllocationSize_m)
            initialAllocation = minAllocationSize_m;

        // allocate memSegments_m

        // loop over pools
        for (int pool = 0; pool < nPools_m; pool++) {

            // allocate memory
            void *tmpPtr = ZeroAlloc(initialAllocation, MMAP_SHARED_PROT,
                                     MMAP_SHARED_FLAGS);
            if (!tmpPtr) {
                ulm_exit((-1,
                          "FixedSharedMemPool::init Unable to allocate "
                          "memory pool , requested %ld, errno %d\n",
                          initialAllocation, errno));
            }
            // apply memory affinity
            if (applyMemoryAffinity) {
                if (!setAffinity(tmpPtr, initialAllocation, pool)) {
                    ulm_exit((-1, "Error: setting memory affinity\n"));
                }
            }
            // set MemorySegment_t data
            memSegments_m[pool][0].basePtr_m = tmpPtr;
            memSegments_m[pool][0].currentPtr_m = tmpPtr;
            memSegments_m[pool][0].segmentLength_m = initialAllocation;
            memSegments_m[pool][0].memAvailable_m = initialAllocation;

            // update the number of elements in use
            nMemorySegments_m[pool] = 1;

        }                       // end pool loop

    }                           // end initial allocation
}


void *FixedSharedMemPool::getMemorySegment(size_t length,
                                           size_t alignment,
					   int poolIndex)
{
    // return if pool can't be used
    if (!poolOkToUse_m)
        return NULL;

    // get the appropriate mask for the alignment
    size_t mask = ~(alignment - 1);

    // sanity check
    assert(poolIndex < nPools_m);

    // initizliaze return pointer to NULL
    void *returnPtr = NULL;

    // loop over memSegments_m elements to see if available memory
    //   exists
    int segmentIndex = -1;
    ssize_t lengthToAllocate = length;
    for (int segment = 0; segment < nMemorySegments_m[poolIndex];
         segment++) {

        char *ptr =
            (char *) memSegments_m[poolIndex][segment].currentPtr_m;

        // check to see if pointer is aligned correctly
        if ((((size_t) ptr) & mask) == ((size_t) ptr)) {
            returnPtr = ptr;
            lengthToAllocate = length;
        } else {

            // align the pointer
            ptr = (char *) ((size_t) ptr + alignment);
            ptr = (char *) ((size_t) ptr & mask);

            lengthToAllocate = length +
                (ptr - (char *) memSegments_m[poolIndex][segment].currentPtr_m);

            // continue if not enough memory in this segment
            if (lengthToAllocate >
                memSegments_m[poolIndex][segment].memAvailable_m) {
                continue;
            }
            returnPtr = ptr;
        }

        if (memSegments_m[poolIndex][segment].memAvailable_m >=
            lengthToAllocate) {
            segmentIndex = segment;
            break;
        }
    }

    // if no available memory exists - get more memory
    if (segmentIndex < 0) {

        // if need be, increase the size of memSegments_m[]
        if (nMemorySegments_m[poolIndex] ==
            nMemorySegmentsInArray_m[poolIndex]) {
            // create a temp version of memSegments_m[]
            MemorySegment_t *tmpMemSeg = (MemorySegment_t *)
                ulm_malloc(sizeof(MemorySegment_t) *
                           (nMemorySegments_m[poolIndex] +
                            nArrayElementsToAdd_m));
            if (!tmpMemSeg) {
                ulm_exit((-1,
                          "FixedSharedMemPool::getMemorySegment Unable to "
                          "allocate memory for tmpMemSeg, errno %d\n",
                          errno));
            }
            // copy old version of memSegments_m to tmp copy
            for (int seg = 0; seg < nMemorySegments_m[poolIndex]; seg++) {
                tmpMemSeg[seg].basePtr_m =
                    memSegments_m[poolIndex][seg].basePtr_m;
                tmpMemSeg[seg].currentPtr_m =
                    memSegments_m[poolIndex][seg].currentPtr_m;
                tmpMemSeg[seg].segmentLength_m =
                    memSegments_m[poolIndex][seg].segmentLength_m;
                tmpMemSeg[seg].memAvailable_m =
                    memSegments_m[poolIndex][seg].memAvailable_m;
            }

            // free memSegments_m[]
            ulm_free(memSegments_m[poolIndex]);

            // assigne the tmp copies of memSegments_m[] to memSegments_m[]
            memSegments_m[poolIndex] = tmpMemSeg;

            // set the element of memSegments_m to grab
            nMemorySegmentsInArray_m[poolIndex] += nArrayElementsToAdd_m;

        }                       // end increase size of memSegments_m[]

        segmentIndex = nMemorySegments_m[poolIndex];

        // allocate more memory
        ssize_t lenToAllocate = 4 * (length + alignment);
        if (lenToAllocate < minAllocationSize_m)
            lenToAllocate = 2 * minAllocationSize_m;
        void *tmpPtr =
            ZeroAlloc(lenToAllocate, MMAP_SHARED_PROT, MMAP_SHARED_FLAGS);
        if (!tmpPtr) {
            ulm_exit((-1, "FixedSharedMemPool::getMemorySegment Unable to "
                      "allocate memory pool\n"));
        }
        // apply memory affinity
        if (applyMemoryAffinity) {
            if (!setAffinity(tmpPtr, lenToAllocate, poolIndex)) {
                ulm_exit((-1, "Error: setting memory affinity\n"));
            }
        }
        // fill in memSegments_m
        memSegments_m[poolIndex][segmentIndex].basePtr_m = tmpPtr;
        memSegments_m[poolIndex][segmentIndex].currentPtr_m = tmpPtr;
        memSegments_m[poolIndex][segmentIndex].segmentLength_m =
            lenToAllocate;
        memSegments_m[poolIndex][segmentIndex].memAvailable_m =
            lenToAllocate;

        //
        nMemorySegments_m[poolIndex]++;

        // set pointer and length
        char *ptr =
            (char *) memSegments_m[poolIndex][segmentIndex].currentPtr_m;
        // check to see if pointer is aligned correctly
        if ((((size_t) ptr) & mask) == ((size_t) ptr)) {
            returnPtr = ptr;
            lengthToAllocate = length;
        } else {

            // align the pointer
            ptr = (char *) ((size_t) ptr + alignment);
            ptr = (char *) ((size_t) ptr & mask);

            //
            returnPtr = ptr;
            lengthToAllocate = length +
                (ptr -
                 (char *) memSegments_m[poolIndex][segmentIndex].
                 currentPtr_m);
        }

    }                           // end  " segmentIndex < 0 "

    // update memSegments_m

    // set currentPtr_m
    memSegments_m[poolIndex][segmentIndex].currentPtr_m = (void *)
        ((char *) (memSegments_m[poolIndex][segmentIndex].currentPtr_m) +
         lengthToAllocate);

    // set memAvailable_m
    memSegments_m[poolIndex][segmentIndex].memAvailable_m -=
        lengthToAllocate;

    return returnPtr;
}
