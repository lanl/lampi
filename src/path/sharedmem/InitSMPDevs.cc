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

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <new>

#include "queue/globals.h"
#include "client/ULMClient.h"
#include "util/Utility.h"
#include "util/cbQueue.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/system.h"
#include "internal/types.h"
#include "mem/FreeLists.h"
#include "os/numa.h"

#if ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPfns.h"
#include "path/sharedmem/SMPDev.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#endif

// pool for isend headers
ssize_t nSMPISendDescPages = -1;
ssize_t maxPgsIn1SMPISendDescList = -1;
ssize_t minPgsIn1SMPISendDescList = -1;
long maxSMPISendDescRetries = 1000;
bool SMPISendDescAbortWhenNoResource = true;

// pool for recv frags - resides in process shared memory.  All structures
// are initialized before the fork() and are not modified there after.
ssize_t nSMPRecvDescPages = -1;
ssize_t maxPgsIn1SMPRecvDescList = -1;
ssize_t minPgsIn1SMPRecvDescList = -1;
long maxSMPRecvDescRetries = 1000;
bool SMPRecvDescAbortWhenNoResource = true;

// upper limit on number of pages per forked proc used for on-SMP
// messaging
int NSMPSharedMemPagesPerProc = -1;

// size of largest bucket (for an optimization)
int SizeLargestSMPSharedMemBucket;

FreeListShared_t<sharedMemData_t> SMPSendDescs;
FreeListShared_t<SMPSecondFragDesc_t> SMPFragPool;

// first frags for which the payload buffers are not yet ready
ProcessPrivateMemDblLinkList firstFrags;

// frag list for on-host messages
// fifo matching queue
cbQueue < SMPFragDesc_t *, MMAP_SHARED_FLAGS, MMAP_SHARED_FLAGS >
    ***SharedMemIncomingFrags;
SharedMemDblLinkList **SMPSendsToPost;
SharedMemDblLinkList **SMPMatchedFrags;
//  list of on host posted sends that have not yet completed sending
//  all frags
//    sorted based on source process
ProcessPrivateMemDblLinkList IncompletePostedSMPSends;

//  list of posted sends that have not yet been fully acked
//    sorted based on source process
ProcessPrivateMemDblLinkList UnackedPostedSMPSends;

void InitSMPSharedMemDescriptors(int NumLocalProcs)
{
    long minPagesPerList;
    long maxPagesPerList;
    long nPagesPerList;
    ssize_t poolChunkSize;
    int nFreeLists;
    int retryForMoreResources;
    int *memAffinityPool;
    int threshToGrowList;
    int retVal;

    // assignment of local variables
    maxPagesPerList = maxPgsIn1SMPISendDescList;
    minPagesPerList = 100;
    nPagesPerList = 400;
    maxPagesPerList = maxPgsIn1SMPISendDescList;
    ssize_t pageSize = SMPPAGESIZE;
    ssize_t eleSize =
        (((sizeof(sharedMemData_t) + sizeof(SMPFragDesc_t) -
           1) / CACHE_ALIGNMENT) + 1) * CACHE_ALIGNMENT;
    eleSize +=
        ((((SMPFirstFragPayload - 1) / CACHE_ALIGNMENT) +
          1) * CACHE_ALIGNMENT);
    poolChunkSize = 10 * SMPPAGESIZE;
    nFreeLists = NumLocalProcs;
    retryForMoreResources = 1;
    threshToGrowList = 0;

    memAffinityPool = (int *) ulm_malloc(sizeof(int) * NumLocalProcs);

    if (!memAffinityPool) {
        ulm_exit((-1, "Out of memory\n"));
    }
    // fill in memory affinity index
    for (int i = 0; i < nFreeLists; i++) {
        memAffinityPool[i] = i;
    }
    //
    // Initialize the SMPSendDesc_t Queue..
    //
    retVal =
        SMPSendDescs.Init(nFreeLists, nPagesPerList, poolChunkSize,
                          pageSize, eleSize, minPagesPerList,
                          maxPagesPerList, maxSMPISendDescRetries,
                          " SMP isend descriptors ", retryForMoreResources,
                          memAffinityPool, enforceAffinity(),
                          largeShareMemDescPool,
                          SMPISendDescAbortWhenNoResource,
                          threshToGrowList);
    if (retVal) {
        ulm_exit((-1, "Error: Initialization of SMP descriptor pool\n"));
    }
    //
    // initialize pool of SMP frag descriptors
    //
    minPagesPerList = 400;
    nPagesPerList = 1000;
    maxPagesPerList = maxPgsIn1SMPRecvDescList;
    pageSize = SMPPAGESIZE;

    /* allocate descriptor and payload */
    eleSize = sizeof(SMPSecondFragDesc_t);
    eleSize = ((eleSize + CACHE_ALIGNMENT - 1) / CACHE_ALIGNMENT) + 1;
    eleSize *= CACHE_ALIGNMENT;
    eleSize += SMPSecondFragPayload;

    poolChunkSize = 10 * SMPPAGESIZE;
    nFreeLists = NumLocalProcs;
    retryForMoreResources = 0;
    threshToGrowList = 0;

    // fill in memory affinity index
    for (int i = 0; i < nFreeLists; i++) {
        memAffinityPool[i] = i;
    }
    retVal =
        SMPFragPool.Init(nFreeLists, nPagesPerList, poolChunkSize,
                         pageSize, eleSize, minPagesPerList,
                         maxPagesPerList, maxSMPISendDescRetries,
                         " SMP frag descriptors ", retryForMoreResources,
                         memAffinityPool, enforceAffinity(),
                         largeShareMemDescPool,
                         SMPRecvDescAbortWhenNoResource, threshToGrowList);

    // clean up 
    ulm_free(memAffinityPool);

    if (retVal) {
        ulm_exit((-1, "Error: Initialization of SMP frag pool\n"));
    }
}


// routine to set up shared memory queues

void SetUpSharedMemoryQueues(int nLocalProcs)
{
    size_t sizeofcbQueue;
    int srcProc;
    int retVal;

    //
    // Sorted list of unprocessed frags for pt2pt - resides in shared memory
    //
    // setup SharedMemIncomingFrags
    sizeofcbQueue =
        sizeof(cbQueue < SMPFragDesc_t *, MMAP_SHARED_FLAGS,
               MMAP_SHARED_FLAGS > **);

    SharedMemIncomingFrags =
        (cbQueue < SMPFragDesc_t *, MMAP_SHARED_FLAGS,
         MMAP_SHARED_FLAGS > ***)
        ulm_malloc(nLocalProcs * sizeofcbQueue);

    if (!(SharedMemIncomingFrags)) {
        ulm_exit((-1, "Error: Out of memory\n"));
    }
    //
    // setting up  of cbQueue**
    //
    for (srcProc = 0; srcProc < nLocalProcs; srcProc++) {
        sizeofcbQueue =
            sizeof(cbQueue < SMPFragDesc_t *, MMAP_SHARED_FLAGS,
                   MMAP_SHARED_FLAGS > *);
        SharedMemIncomingFrags[srcProc] =
            (cbQueue < SMPFragDesc_t *, MMAP_SHARED_FLAGS,
             MMAP_SHARED_FLAGS > **)
            ulm_malloc(nLocalProcs * sizeofcbQueue);
        if (!(SharedMemIncomingFrags[srcProc])) {
            ulm_exit((-1, "Error: Out of memory\n"));
        }


        for (int destProc = 0; destProc < nLocalProcs; destProc++) {
            sizeofcbQueue =
                sizeof(cbQueue < SMPFragDesc_t *, MMAP_SHARED_FLAGS,
                       MMAP_SHARED_FLAGS >);
            // allocate space for queue
            SharedMemIncomingFrags[srcProc][destProc] =
                (cbQueue < SMPFragDesc_t *, MMAP_SHARED_FLAGS,
                 MMAP_SHARED_FLAGS > *)
                PerProcSharedMemoryPools.getMemorySegment(sizeofcbQueue,
                                                          CACHE_ALIGNMENT,
                                                          destProc);

            if (!(SharedMemIncomingFrags[srcProc][destProc])) {
                ulm_exit((-1, "Error: Out of memory\n"));
            }

            /* !!! locking for thread safety */

            retVal =
                SharedMemIncomingFrags[srcProc][destProc]->
                init(cbQueue < SMPFragDesc_t *, MMAP_SHARED_FLAGS,
                     MMAP_SHARED_FLAGS >::defaultSize, destProc, srcProc,
                     destProc, (SMPFragDesc_t *) (1),
                     (SMPFragDesc_t *) (2), (SMPFragDesc_t *) (3));
            if (retVal != ULM_SUCCESS) {
                ulm_exit((-1, "Error: Initializing frag queue\n"));
            }

        }                       // end of destProc loop
    }                           // end of srcProc loop
    //
    // Sorted list of unprocessed frags for pt2pt - resides in shared memory
    //
    SMPSendsToPost = (SharedMemDblLinkList **)
        ulm_malloc(sizeof(DoubleLinkList *) * local_nprocs());
    if (SMPSendsToPost == NULL) {
        ulm_exit((-1, "Error: Out of memory\n"));
    }
    createDoubleLinkList(SMPSendsToPost);

    //
    // Sorted list of unprocessed matched frags for pt2pt - resides in shared memory
    //
    SMPMatchedFrags = (SharedMemDblLinkList **)
        ulm_malloc(sizeof(DoubleLinkList *) * local_nprocs());
    if (SMPMatchedFrags == NULL) {
        ulm_exit((-1, "Error: Out of memory\n"));
    }
    createDoubleLinkList(SMPMatchedFrags);


    // initialize locks for first frags that have been matched, but their data
    //   has not yet been read
    //   !!!! threaded lock
    firstFrags.Lock.init();
}
