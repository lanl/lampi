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

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <new>

#include "queue/globals.h"
#include "client/ULMClient.h"
#include "path/mcast/utsendInit.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "path/mcast/utsendDesc.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/system.h"
#include "internal/types.h"

// upper limit on number of pages per forked proc used for on-SMP
// messaging
// size of largest bucket (for an optimization)


// extern'd in the sharedmem/SMPSharedMemGlobals.h
// i'll run this by code czar  Rasmussen later
FreeLists < DoubleLinkList, UtsendDesc_t, int, MMAP_SHARED_PROT,
    MMAP_SHARED_FLAGS, MMAP_SHARED_FLAGS > UtsendDescriptors;

SharedMemDblLinkList **UnackedUtsendDescriptors;

SharedMemDblLinkList **IncompleteUtsendDescriptors;

/*
 *  Initialize the collective communication shared memory "devices"
 */
void InitUtsendMemDevices(int NumLocalProcs)
{
    long nPagesPerList = 3;
    long minPagesPerList = 1;
    long maxPagesPerList = -1;
    ssize_t pageSize = SMPPAGESIZE;
    ssize_t eleSize = sizeof(UtsendDesc_t);
    ssize_t poolChunkSize = SMPPAGESIZE;
    int nFreeLists = NumLocalProcs;
    long maxUtsendRetries = 100;
    bool enforceMemoryPolicy = false;
    bool irecvDescDescAbortWhenNoResource = true;
    int retryForMoreResources = 1;
    int memAffinityPool = getMemPoolIndex();

    /* !!!! thread lock - is this really true ? */
    UtsendDescriptors.Init(nFreeLists, nPagesPerList, poolChunkSize,
                           pageSize, eleSize, minPagesPerList,
                           maxPagesPerList, maxUtsendRetries,
                           " Utsend descriptors ", retryForMoreResources,
                           &memAffinityPool, enforceMemoryPolicy,
                           ShareMemDescPool, irecvDescDescAbortWhenNoResource);

/*	allocate the SharedMemLinkedList
	Allocating the IncoUtsendDescriptor Shared Memory
	Doubly Linked List
*/
    IncompleteUtsendDescriptors =
        (SharedMemDblLinkList **) SharedMemoryPools.
        getMemorySegment(sizeof(DoubleLinkList *) * local_nprocs(),
                         CACHE_ALIGNMENT);


    // allocate individual lists and initialize locks
    for (int comm_rt = 0; comm_rt < local_nprocs(); comm_rt++) {
        // allocate shared memory
        IncompleteUtsendDescriptors[comm_rt] = (SharedMemDblLinkList *)
            SharedMemoryPools.getMemorySegment(sizeof(DoubleLinkList),
                                               CACHE_ALIGNMENT);

        if (!(IncompleteUtsendDescriptors[comm_rt])) {
            ulm_exit((-1, "Unable to allocate space for "
                      "IncompleteUtsendDescriptor[%d]\n", comm_rt));
        }
        // run the constuctor
        new(IncompleteUtsendDescriptors[comm_rt]) SharedMemDblLinkList;

        // initialize locks
        IncompleteUtsendDescriptors[comm_rt]->Lock.init();
    }


/*      allocate the SharedMemLinkedList
        Allocating the IncoUtsendDescriptor Shared Memory
        Doubly Linked List
*/
    UnackedUtsendDescriptors =
        (SharedMemDblLinkList **) SharedMemoryPools.
        getMemorySegment(sizeof(DoubleLinkList *) * local_nprocs(),
                         CACHE_ALIGNMENT);


    // allocate individual lists and initialize locks
    for (int comm_rt = 0; comm_rt < local_nprocs(); comm_rt++) {
        // allocate shared memory
        UnackedUtsendDescriptors[comm_rt] = (SharedMemDblLinkList *)
            SharedMemoryPools.getMemorySegment(sizeof(DoubleLinkList),
                                               CACHE_ALIGNMENT);

        if (!(UnackedUtsendDescriptors[comm_rt])) {
            ulm_exit((-1,
                      "Unable to allocate space for UnackedUtsendDescriptor"
                      "[%d]\n", comm_rt));
        }
        // run the constuctor
        new(UnackedUtsendDescriptors[comm_rt]) SharedMemDblLinkList;

        // initialize locks
        UnackedUtsendDescriptors[comm_rt]->Lock.init();
    }
}
