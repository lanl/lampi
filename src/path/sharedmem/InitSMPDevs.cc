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
#include "util/Utility.h"
#include "util/cbQueue.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/system.h"
#include "internal/types.h"
#include "mem/FreeLists.h"
#include "os/numa.h"

#ifdef ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPfns.h"
#include "path/sharedmem/SMPDev.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#endif

// number of shared memory "devices"
int NSMPDevs;

// array of shared memory "devices"
SMPSharedMem_logical_dev_t *SMPSharedMemDevs;
ssize_t nSMPSharedMemPages = -1;
ssize_t maxPgsIn1SMPSharedMemPagesStack = -1;
ssize_t minPgsIn1SMPSharedMemPagesStack = -1;
long maxSMPSharedMemPagesStackRetries = 1000;
bool SMPSharedMemPagesStackAbortWhenNoResource = true;

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

// pool for local collective frags - resides in process shared memory.
// All structures are initialized before the fork() and are not modified
// thereafter.
ssize_t nSMPCollFragDescPages = -1;
ssize_t maxPgsIn1SMPCollFragDescList = -1;
ssize_t minPgsIn1SMPCollFragDescList = -1;
long maxSMPCollFragDescRetries = 1000;
bool SMPCollFragDescAbortWhenNoResource = true;

// upper limit on number of pages per forked proc used for on-SMP
// messaging
int NSMPSharedMemPagesPerProc = -1;

// size of largest bucket (for an optimization)
int SizeLargestSMPSharedMemBucket;

/*
 *  Initialize the SMP shared memory "devices"
 */
void InitSMPSharedMemDevices(int NumLocalProcs)
{
    // initial sanity check - make sure SMPSharedMem_logical_dev_t is
    // a multiple of pages size in its size.
    ssize_t Devsize = sizeof(SMPSharedMem_logical_dev_t);
    if ((Devsize / SMPPAGESIZE) * SMPPAGESIZE != Devsize) {
        ulm_exit((-1, "SMP shared memory device is not the right size\n"
                  "   Size is: %ld, but should be a multiple of page "
                  "size %u\n", Devsize, SMPPAGESIZE));
    }
    // get number of Shared memory devices
    NSMPDevs = NumLocalProcs;

    // allocate data structures

    // get the memory for all devices
    ssize_t lenToAllocate = NSMPDevs * sizeof(SMPSharedMem_logical_dev_t);

    SMPSharedMemDevs = (SMPSharedMem_logical_dev_t *)
        SharedMemoryPools.getMemorySegment(lenToAllocate, SMPPAGESIZE);

    if (!SMPSharedMemDevs) {
        ulm_exit((-1, "Unable to allocate memory for SMPSharedMemDevs.\n"
                  "  Bytes requested %ld\n", lenToAllocate));
    }
    // determine the size of the shared memory pool
    long long NBytes;
    if (nSMPSharedMemPages == -1) {
        NBytes = ((long long) SMPDevDefaults::PagesPerDevice)
            * ((long long) NSMPDevs)
            * ((long long) SMPDevDefaults::SMPPageSize);
    } else
        NBytes = ((long long) nSMPSharedMemPages)
            * ((long long) NSMPDevs)
            * ((long long) SMPDevDefaults::SMPPageSize);

    // allocate the pool
    ssize_t poolSize =
        sizeof(ULMMemoryPool(SMPDevDefaults::LogBase2MaxPoolSize,
                             SMPDevDefaults::LogBase2ChunkSize,
                             SMPDevDefaults::LogBase2PageSize,
                             SMPDevDefaults::MProt,
                             SMPDevDefaults::MFlags,
                             NBytes));
    ULMMemoryPool *sp = (ULMMemoryPool *)
        SharedMemoryPools.getMemorySegment(poolSize, SMPPAGESIZE);
    if (!sp) {
        ulm_exit((-1, "Unable to get shared memory for ULMMemoryPool "
                  "for SMPSharedMemDevs.\n"));
    }

    new(sp) ULMMemoryPool(SMPDevDefaults::LogBase2MaxPoolSize,
                          SMPDevDefaults::LogBase2ChunkSize,
                          SMPDevDefaults::LogBase2PageSize,
                          SMPDevDefaults::MProt,
                          SMPDevDefaults::MFlags, NBytes);
    // initialize pool lock
    sp->Lock.init();

    // place the devices (and run the constructors)
    for (int dev = 0; dev < NSMPDevs; dev++) {
        // !!! memory locality
        new(&(SMPSharedMemDevs[dev])) SMPSharedMem_logical_dev_t(sp);
        // initialize device lock
        SMPSharedMemDevs[dev].lock_m.init();
    }

    // initialize the memory buckets
    // loop over devices
    for (int dev = 0; dev < NSMPDevs; dev++) {

        // set input for init function
        long minPagesPerStack[NumSMPMemoryBuckets];
        long maxPagesPerStack[NumSMPMemoryBuckets];
        long mxConsecReqFailures[NumSMPMemoryBuckets];
        long defaultMinPagesPerStack = 200;
        long defaultNPagesPerStack = 4000;
        for (int bucket = 0; bucket < NumSMPMemoryBuckets; bucket++) {
            minPagesPerStack[bucket] = minPgsIn1SMPSharedMemPagesStack;
            maxPagesPerStack[bucket] = maxPgsIn1SMPSharedMemPagesStack;
            mxConsecReqFailures[bucket] = maxSMPSharedMemPagesStackRetries;
        }
        int Log2BucketSizes[NumSMPMemoryBuckets] = { 8, 14, 16 };
//              int Log2BucketSizes[NumSMPMemoryBuckets]={8, 12, 18};
        int retVal =
            SMPSharedMemDevs[dev].Init(SMPPAGESIZE, minPagesPerStack,
                                       defaultMinPagesPerStack,
                                       defaultNPagesPerStack,
                                       maxPagesPerStack,
                                       mxConsecReqFailures,
                                       " SMP shared Memory data pages ",
                                       NBytes / SMPPAGESIZE,
                                       Log2BucketSizes,
                                       SMPSharedMemPagesStackAbortWhenNoResource);      // Ignores size of Raw mem pool, already provided
        if (retVal) {
            ulm_exit((-1, "SMP Data Buffers::Init Unable to initialize "
                      "memory pool\n"));
        }
        // loop over buckets to fill initial memory allocation request
        for (int bucket = 0; bucket < NumSMPMemoryBuckets; bucket++) {
            // initialize stack locks
            SMPSharedMemDevs[dev].MemoryBuckets.buckets[bucket].stack_m.
                Lock.init();
            // initialize bucket lock
            SMPSharedMemDevs[dev].MemoryBuckets.buckets[bucket].lock_m.
                init();
            // add memory to stack until initial allocation is satisfied
            long long memToAdd =
                SMPSharedMemDevs[dev].MemoryBuckets.buckets[bucket].
                minBytesPushedOnStack_m;
            long long memAdded = 0;
            while (memAdded < memToAdd) {

                // request chunk of memory
                size_t lenData;
                int errCode;
                void *tmpPtr =
                    SMPSharedMemDevs[dev].GetChunkOfMemory(bucket,
                                                           &lenData,
                                                           &errCode, dev);
                if (tmpPtr == (void *) (-1L)) {
                    ulm_exit((-1, "SMP Data Buffers::Init Unable to get "
                              "memory from pool requested %ld bytes\n",
                              SMPSharedMemDevs[dev].SharedPool->
                              PoolChunkSize));
                }
                // !!! memory locality

                // push chunk onto stack
                SMPSharedMemDevs[dev].PushChunkOnStack(bucket,
                                                       (char *) tmpPtr);

                // increment memAdded
                memAdded +=
                    SMPSharedMemDevs[dev].SharedPool->PoolChunkSize;

            }                   // end while loop

        }                       // end loop over buckets

    }                           // end dev loop

    // set the size of the largest bucket - used to optimize striping
    //   structure ==> always look at local device 0.
    int NBuckets = SMPSharedMemDevs[0].MemoryBuckets.NumBuckets;
    // get size of largest bucket - this will determine the message striping
    SizeLargestSMPSharedMemBucket =
        SMPSharedMemDevs[0].MemoryBuckets.buckets[NBuckets -
                                                  1].segmentSize_m;

#ifdef ENABLE_SHARED_MEMORY
    // allocate shared memory send descriptors
    InitSMPSharedMemDescriptors(NumLocalProcs);
#endif

}

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
SharedMemDblLinkList **SMPincomingFrags;
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
    int LocProc;
    size_t sizeofcbQueue;
    int srcProc;
    int retVal;

    //
    // Sorted list of unprocessed frags for pt2pt - resides in shared memory
    //
    SMPincomingFrags =
        (SharedMemDblLinkList **) ulm_malloc(sizeof(DoubleLinkList *) *
                                             local_nprocs());

    if (!SMPincomingFrags) {
        ulm_exit((-1, "Unable to allocate space for SMPincomingFrags *\n"));
    }


    for (LocProc = 0; LocProc < local_nprocs(); LocProc++) {
        SMPincomingFrags[LocProc] = (SharedMemDblLinkList *)
            SharedMemoryPools.getMemorySegment(getpagesize(),
                                               getpagesize());
        if (!(SMPincomingFrags[LocProc])) {
            ulm_exit((-1, "Error: Out of memory\n"));
        }

        if (enforceAffinity()) {
            if (!setAffinity((void *) SMPincomingFrags[LocProc],
                             getpagesize(), LocProc)) {
                ulm_exit((-1, "Error: Can't Set memory policy\n"));
            }
        }
        // run constructor
        new(SMPincomingFrags[LocProc]) SharedMemDblLinkList;
        // initialize the lock
        SMPincomingFrags[LocProc]->Lock.init();
    }
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
