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

#ifndef _FIXEDSHAREDMEMPOOL
#define _FIXEDSHAREDMEMPOOL

#include "internal/malloc.h"

// this structure is used to describe an allocated segment of memory
// that will be granted for use upon request.  The assumtions are that
// allocation will be sequentional, and that "freed" memory will not
// be reused.

typedef struct MemorySegment {
    // pointer to the allocation base
    void *basePtr_m;

    // pointer to current free segment
    void *currentPtr_m;

    // length of allocated segment
    ssize_t segmentLength_m;

    // remaining "free" memory
    ssize_t memAvailable_m;
} MemorySegment_t;

// this class is used to satisfy shared memory requests.  This class
// assumes that request are made before the child process are fork'ed,
// and that this memory will not be recycled or freed until the end of
// the app run.
class FixedSharedMemPool {
public:
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

    // number of pools
    int nPools_m;

    // description of available memory - structures
    //  reside in private memory - will only be modified
    //  before the fork. There are nPools number of arrays of
    //  memory segments.
    MemorySegment_t **memSegments_m;

    // number of MemorySegment_t elements
    int *nMemorySegments_m;

    // number of MemorySegment_t elements in array
    int *nMemorySegmentsInArray_m;

    // number of MemorySegment_t to add to memSegments_m at a time
    int nArrayElementsToAdd_m;

    // flag to indicate if pool can be used
    bool poolOkToUse_m;

    // flag to indicate if memory affinity is to be applied
    bool applyMemoryAffinity;

    // size of smallest memory segment controlled by memSegments_m
    //  (int bytes)
    ssize_t minAllocationSize_m;

    // initialization function
    void init(ssize_t initialAllocation, ssize_t minAllocationSize,
              int nPools, int nArrayElementsToAdd, bool applyMemAffinity);

    // get a shared memory segment
    void *getMemorySegment(size_t length, size_t align, int Pool = 0);
};

#endif /* !_FIXEDSHAREDMEMPOOL */
