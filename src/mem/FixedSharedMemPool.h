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

#ifndef _FIXEDSHAREDMEMPOOL
#define _FIXEDSHAREDMEMPOOL

#include "internal/malloc.h"

// Class used to satisfy shared memory requests. Assumes that request
// are made before the child process are forked, and that this memory
// will not be recycled or freed until the app exits.

class FixedSharedMemPool {

    // Structure describing an allocated segment of memory that will
    // be granted for use upon request.  The assumptions are that
    // allocation will be sequential, and that "freed" memory will not
    // be reused.

    class MemorySegment_t {
    public:
        void *basePtr_m;         // pointer to the allocation base
        void *currentPtr_m;      // pointer to current free segment
        ssize_t segmentLength_m; // length of allocated segment
        ssize_t memAvailable_m;  // remaining "free" memory
    };

public:

    // --STATE--

    MemorySegment_t **memSegments_m; // description of available
                                     // memory - structures reside in
                                     // private memory - will only be
                                     // modified before the
                                     // fork. There are nPools number
                                     // of arrays of memory segments

    int *nMemorySegments_m;          // number of MemorySegment_t elements
    int *nMemorySegmentsInArray_m;   // number of MemorySegment_t elements in array
    ssize_t minAllocationSize_m;     // size of smallest memory segment
    int nArrayElementsToAdd_m;       // number of MemorySegment_t to add to memSegments_m
    int nPools_m;                    // number of pools
    bool poolOkToUse_m;              // flag to indicate if pool can be used
    bool applyMemoryAffinity;        // flag to indicate if memory affinity is to be applied


    // --METHODS--

    // default constructor
    FixedSharedMemPool()
        {
            poolOkToUse_m = false;
            nMemorySegments_m = NULL;
            nMemorySegmentsInArray_m = NULL;
            memSegments_m = NULL;
            minAllocationSize_m = -1;
            applyMemoryAffinity = false;
        }

    // destructor
    ~FixedSharedMemPool()
        {
            int i;

            if (memSegments_m) {
                for (i = 0; i < nPools_m; i++) {
                    ulm_free(memSegments_m[i]);
                }
                ulm_free(memSegments_m);
                memSegments_m = 0;
            }
            if (nMemorySegments_m) {
                ulm_free(nMemorySegments_m);
                nMemorySegments_m = 0;
            }
            if (nMemorySegmentsInArray_m) {
                ulm_free(nMemorySegmentsInArray_m);
                nMemorySegmentsInArray_m = 0;
            }
        }

    // initialization function
    void init(ssize_t initialAllocation, ssize_t minAllocationSize,
              int nPools, int nArrayElementsToAdd, bool applyMemAffinity);

    // get a shared memory segment
    void *getMemorySegment(size_t length, size_t align, int Pool = 0);
};

#endif /* !_FIXEDSHAREDMEMPOOL */
