/*
 * Copyright 2002-2003. The Regents of the University of California. This material 
 * was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
 * National Laboratory, which is operated by the University of California for 
 * the U.S. Department of Energy. The Government is granted for itself and 
 * others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
 * license in this material to reproduce, prepare derivative works, and 
 * perform publicly and display publicly. Beginning five (5) years after 
 * October 10,2002 subject to additional five-year worldwide renewals, the 
 * Government is granted for itself and others acting on its behalf a paid-up, 
 * nonexclusive, irrevocable worldwide license in this material to reproduce, 
 * prepare derivative works, distribute copies to the public, perform publicly 
 * and display publicly, and to permit others to do so. NEITHER THE UNITED 
 * STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
 * CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
 * IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
 * PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
 * OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <new>

#include "queue/Communicator.h"
#include "queue/barrier.h"
#include "queue/globals.h"
#include "client/daemon.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/system.h"
#include "internal/types.h"

// instantiate swBarrier
SWBarrierPool swBarrier;

//
//  Simple on host barrier
//
void SMPSWBarrier(volatile void *barrierData)
{
    int count = 0;
    // cast data to real type
    volatile swBarrierData *barData = (volatile swBarrierData *) barrierData;

    // increment "release" count
    barData->releaseCnt += barData->commSize;

    // increment fetch and op variable
    barData->lock->lock();
    *(barData->Counter) += 1;
    barData->lock->unlock();

    // spin until fetch and op variable == release count
    while (*(barData->Counter) < barData->releaseCnt) {
	if (count++ == 10000) {
            ulm_make_progress();
            count = 0;
	}
    }

    return;
}


//
//  This routine is used to allocate a pool of O2k atomic fetch-
//    and-op variables.
//

int allocSWSMPBarrierPools()
{
    //
    // set the number of pools
    //
    swBarrier.nPools = 1;

    //
    // check to make sure nCpuNodes is positive
    //
    if (swBarrier.nPools <= 0) {
        return ULM_SUCCESS;
    }
    //
    //  determine how many pages to allocate (the granularity of the
    //    per node allocation is 1 page)
    //
    int nPages = 1;

    //
    // allocate nElementsInPool
    //
    swBarrier.nElementsInPool = (int *) ulm_malloc(sizeof(int) * swBarrier.nPools);
    if (!(swBarrier.nElementsInPool)) {
        ulm_exit(("swBarrier.nElementsInPool is invalid\n"));
    }
    //
    // allocate lastPoolUsed
    //
    swBarrier.lastPoolUsed = (int *) SharedMemoryPools.getMemorySegment
        (sizeof(int), CACHE_ALIGNMENT);
    if (!(swBarrier.lastPoolUsed)) {
        ulm_exit(("swBarrier.lastPoolUsed is invalid\n"));
    }
    *(swBarrier.lastPoolUsed) = 0;

    //
    // allocate Lock and initialize it
    //
    swBarrier.Lock = (Locks *) SharedMemoryPools.getMemorySegment
        (sizeof(Locks), CACHE_ALIGNMENT);
    if (!(swBarrier.Lock)) {
        ulm_exit(("swBarrier.Lock is invalid\n"));
    }
    swBarrier.Lock->init();
    //
    // allocate pool
    //
    swBarrier.pool = (swBarrierCtlData **)
        ulm_malloc(sizeof(swBarrierCtlData *) * swBarrier.nPools);
    if (!(swBarrier.pool)) {
        ulm_exit(("swBarrier.pool is invalid\n"));
    }
    // allocate shared memory
    long long memory = nPages * getpagesize();
    swBarrierData *memPtr = (swBarrierData *)
        SharedMemoryPools.getMemorySegment(memory, getpagesize());
    if (!memPtr) {
        swBarrier.nPools = 0;
        ulm_free(swBarrier.nElementsInPool);
        ulm_free(swBarrier.pool);
        return ULM_SUCCESS;
    }
    // !!!! memory locality


    //
    // setup the memory pools
    //
    assert(nPages == swBarrier.nPools);
    long long lockSize = sizeof(Locks);
    lockSize = ((lockSize / (sizeof(long long) - 1)) + 1) * sizeof(long long);
    long long barrierDataLen = lockSize + sizeof(long long);
    long long nElements = nPages * getpagesize() / barrierDataLen;
    for (int pl = 0; pl < swBarrier.nPools; pl++) {
        swBarrier.nElementsInPool[pl] = nElements;
        swBarrier.pool[pl] = (swBarrierCtlData *)
	    SharedMemoryPools.getMemorySegment(nElements * sizeof(swBarrierCtlData),
                                               CACHE_ALIGNMENT);
        if (!(swBarrier.pool[pl])) {
            ulm_exit(("swBarrier.pool[%i] is invalid\n", pl));
        }
        char *counter = (char *) memPtr + pl * nPages * getpagesize();
        for (int ele = 0; ele < nElements; ele++) {
	    swBarrier.pool[pl][ele].inUse = 0;
	    swBarrier.pool[pl][ele].commRoot = -1;
	    swBarrier.pool[pl][ele].contextID = -1;
	    swBarrier.pool[pl][ele].nAllocated = 0;
	    swBarrier.pool[pl][ele].nFreed = 0;
            // allocate the barrierData element out of process private memory
            (swBarrier.pool[pl][ele]).barrierData = (swBarrierData *) ulm_malloc(sizeof(swBarrierData));
            if (!(swBarrier.pool[pl][ele].barrierData)) {
                ulm_exit(("swBarrier.pool[%i][%i].barrierData is "
                          "invalid\n", pl, ele));
            }
            // set the pointer to the fetchop variable
            swBarrier.pool[pl][ele].barrierData->Counter = (long long *)
                ((char *) counter + lockSize);

            // allocate lock and Counter data - make sure long long is aligned
            //  on long long boundry
            swBarrier.pool[pl][ele].barrierData->lock = (Locks *) counter;
            // placement new
            new(swBarrier.pool[pl][ele].barrierData->lock) Locks;
            // initialize locks
            swBarrier.pool[pl][ele].barrierData->lock->init();

            counter = (char *) counter + barrierDataLen;

        }                       // end element loop
    }                           // end pool loop

    return ULM_SUCCESS;
}
