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
#include <stdlib.h>
#include <strings.h>
#include <new>

#ifdef QUADRICS
#include <elan3/elan3.h>
#include "path/quadrics/state.h"
#endif

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "util/Lock.h"
#include "util/dclock.h"
#ifdef SHARED_MEMORY
# include "path/sharedmem/SMPSharedMemGlobals.h"
#endif // SHARED_MEMORY
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/system.h"
#include "internal/collective.h"
#include "mpi/constants.h"
#include "path/common/path.h"
#include "os/atomic.h"
#include "ulm/ulm.h"

/*
 * some auxiliary functions used to initialize the communicator
 */

/*
 * This routine is used to compute the send/recv pairs for the
 * allgather type of data exchange based on a binary tree algorithm.
 * The data exchange pattern is computed only for the largest number
 * of processes in the group that are a power of 2.
 */
int setupSendRecvTree(int nNodes, list_t * exchangeList, int *nExtra,
                      int *extraOffset, int *treeDepth)
{
    int returnValue = ULM_SUCCESS;
    int level;

    /* figure out the tree's depth */
    int nMax = nNodes;
    int depth = 0;
    while (nMax > 0) {
        nMax = nMax >> 1;
        depth++;
    }
    int nTreeNodes = 1;
    for (level = 1; level < depth; level++) {
        nTreeNodes *= 2;
    }
    *nExtra = nNodes - nTreeNodes;
    *extraOffset = nTreeNodes;
    *treeDepth = depth;

    /*
     * loop over all nodes in the tree and figure out the exchange
     * pairs at each level
     */
    for (int node = 0; node < nTreeNodes; node++) {

        /* allocate memory */
        exchangeList[node].length = depth - 1;
        if (exchangeList[node].length == 0) {
            exchangeList[node].list = NULL;
            continue;
        }

        exchangeList[node].list =
            (int *) ulm_malloc(sizeof(int) * (depth - 1));
        if ((depth > 1) && !exchangeList[node].list) {
            return ULM_ERR_OUT_OF_RESOURCE;
        }
        /*
         * figure out send/recv pattern
         */

        /* figure out how many sends will take place */
        int stride = 1;
        int cnt = 0;
        for (level = 1; level < depth; level++) {

            /* figure out if I am in the lower or upper half of the "group" of
             *   processes, at this level
             */
            int upper = node / (2 * stride);
            upper = node - (upper * 2 * stride);
            upper = upper - stride;
            if (upper >= 0)
                upper = 1;
            else
                upper = 0;

            int exchangePartner;
            if (upper)
                exchangePartner = node - stride;
            else
                exchangePartner = node + stride;


            exchangeList[node].list[cnt] = exchangePartner;

            cnt++;
            stride *= 2;
        }                       /* end level loop */
    }                           /* end node loop */

    return returnValue;
}


/*
 * Recursive routine used to setup the host order in which we expect
 * the data to arrive.  Note that the order will be different on each
 * host.
 */
void getSendingHostRank(list_t * sources, int *cnt, int *list,
                        int currentLevel, int callingRank, int nExtra,
                        int extraOffset)
{
    list[*cnt] = callingRank;
    (*cnt)++;
    /*
     * account for the fact that the ranks that don't correspond to a
     * full power of 2 level, first sent there data to the rank that
     * is the order of the tree lower than it
     */
    if ((nExtra > 0) & (callingRank < nExtra)) {
        list[*cnt] = callingRank + extraOffset;
        (*cnt)++;
    }
    int nSendToNow = currentLevel - 1;
    for (int i = 0; i < nSendToNow; i++) {
        int treeLevel = i + 1;
        getSendingHostRank(sources, cnt, list, treeLevel,
                           sources[callingRank].list[i], nExtra,
                           extraOffset);
    }
}


/*
 * The init function is used to set up group properties, including:
 *   - path selection functions
 *   - group list
 *   - map from process ProcID to global ProcID
 *   - map from process ProcID to host ProcID
 *   - thread usage
 *   - setup process private queues (process shared arrays are set up
 *      before the fork)
 */
int Communicator::init(int ctxID, bool threadUsage, int group1Index,
                       int group2Index, bool firstInstanceOfContext,
                       bool setCtxCache, int sizeCtxCache,
                       int *lstCtxElements, int commType,
                       bool okToDeleteComm, int useSharedMem,
                       int useSharedMemCollOpts)
{
    //
    // set group
    //
    // sanity checks
    if (group1Index >= grpPool.poolSize || group1Index < 0) {
        ulm_err(("Error: Communicator::init: group index out of range\n"));
        return ULM_ERR_BAD_PARAM;
    }
    if (group2Index >= grpPool.poolSize || group2Index < 0) {
        ulm_err(("Error: Communicator::init: group index out of range\n"));
        return ULM_ERR_BAD_PARAM;
    }
    if (grpPool.groups[group1Index] == 0) {
        ulm_err(("Error: Communicator::init: null group\n"));
        return ULM_ERR_BAD_PARAM;
    }
    localGroup = grpPool.groups[group1Index];
    if (grpPool.groups[group2Index] == 0) {
        ulm_err(("Error: Communicator::init: null group\n"));
        return ULM_ERR_BAD_PARAM;
    }
    remoteGroup = grpPool.groups[group2Index];

    //
    // set useSharedMemForCollectives
    //
    if (useSharedMemCollOpts)
        useSharedMemForCollectives = 1;
    else
        useSharedMemForCollectives = 0;

    // set path selection functions to default
    pt2ptPathSelectionFunction = ulm_bind_pt2pt_message;

    // set contextID
    contextID = ctxID;

    // set request reference count
    requestRefCount = 0;

    // initialize flag indicating threads will be used
    if (usethreads()) {
        useThreads = true;
    } else {
        useThreads = false;
    }

    markedForDeletion = 0;

    // initialize generic communicator lock
    //!!!!! threaded-lock
    commLock.init();

    // initialize reference count lock
    //!!!!! threaded-lock
    refCounLock.init();

    // set base_tag
    base_tag = ULM_UNIQUE_BASE_TAG;
    //!!!!! threaded-lock
    base_tag_lock.init();

    // initialize topology pointer to NULL
    topology = (ULMTopology_t *) NULL;

    // initialize error handler index
    errorHandlerIndex = MPI_ERRHANDLER_DEFAULT;

    // set communicator type
    communicatorType = commType;

    // attribute cache
    attributeCount = 0;
    sizeOfAttributeArray = 0;
    attributeList = (commAttribute_t *) 0;

    // set communicator cache
    commCache =
        (int *) ulm_malloc(sizeof(int) *
                           Communicator::MAX_COMM_CACHE_SIZE);
    if (!commCache) {
        ulm_warn(("Warning: Unable to allocate space for communicator cache\n"));
        return ULM_ERR_OUT_OF_RESOURCE;
    }

    //if (setCtxCache) {
    //    // non-empty cache
    //    cacheSize = Communicator::MAX_COMM_CACHE_SIZE;
    //    for (int i = 0; i < sizeCtxCache; i++) {
    //        commCache[i] = lstCtxElements[i];
    //    }
    //    availInCommCache = sizeCtxCache;
    //} else {
    //    // empty cache
    //    cacheSize = Communicator::MAX_COMM_CACHE_SIZE;
    //    availInCommCache = 0;
    // }

    // ok to delete communicator ?
    canFreeCommunicator = okToDeleteComm;

    // initialize flag indicating threads will be used
    if (threadUsage) {
        useThreads = true;
    } else {
        useThreads = false;
    }

    // initialize counters
    // posted receive counter
    setBigAtomicUnsignedInt(&next_irecv_id_counter, 1);
    /* !!!!! threaded-lock */
    next_irecv_id_counterLock.init();

    // next sequence number expected to arrive on the receive side
    next_expected_isendSeqs =
        ulm_new(ULM_64BIT_INT, remoteGroup->groupSize);
    if (!next_expected_isendSeqs) {
        ulm_exit((-1, "Error: Communicator::init: "
                  "Unable to allocate space for next_expected_isendSeqs\n"));
    }
    next_expected_isendSeqsLock = ulm_new(Locks, remoteGroup->groupSize);
    if (!next_expected_isendSeqsLock) {
        ulm_exit((-1, "Error: Communicator::init: Unable to allocate "
                  "space for next_expected_isendSeqsLock\n"));
    }
    // !!!!! threaded-lock
    for (int ele = 0; ele < remoteGroup->groupSize; ele++) {
        next_expected_isendSeqsLock[ele].init();
    }
    for (int i = 0; i < remoteGroup->groupSize; ++i)
        next_expected_isendSeqs[i] = 0;

    // set up receive lock
    recvLock = ulm_new(Locks, remoteGroup->groupSize);
    if (!recvLock) {
        ulm_exit((-1, "Error: Communicator::init: "
                  "Unable to allocate space for recvLock\n"));
    }
    //!!!!! threaded-lock
    for (int ele = 0; ele < remoteGroup->groupSize; ele++) {
        recvLock[ele].init();
    }

    // next sequence number to be assigned for a send
    next_isendSeqs = ulm_new(ULM_64BIT_INT, remoteGroup->groupSize);
    if (!next_isendSeqs) {
        ulm_exit((-1, "Error: Communicator::init: "
                  "Unable to allocate space for next_isendSeqs\n"));
    }

    for (int i = 0; i < remoteGroup->groupSize; ++i)
        next_isendSeqs[i] = 0;

    next_isendSeqsLock = ulm_new(Locks, remoteGroup->groupSize);
    if (!next_isendSeqsLock) {
        ulm_exit((-1, "Error: Communicator::init: "
                  "Unable to allocate space for next_isendSeqsLock\n"));
    }
    //!!!!! threaded-lock
    for (int ele = 0; ele < remoteGroup->groupSize; ele++) {
        next_isendSeqsLock[ele].init();
    }

    // initialize queues
    //

    // posted wild receive queue
    //
    // !!!!! threaded-lock
    privateQueues.PostedWildRecv.Lock.init();

    // Posted utrecv queue
    privateQueues.PostedUtrecvs.Lock.init();

    //
    // List of posted Specific ireceives - list resides in process private memory
    //
    privateQueues.PostedSpecificRecv =
        ulm_new(ProcessPrivateMemDblLinkList *, remoteGroup->groupSize);
    for (int proc = 0; proc < remoteGroup->groupSize; proc++) {
        privateQueues.PostedSpecificRecv[proc] =
            ulm_new(ProcessPrivateMemDblLinkList, 1);
        if (!privateQueues.PostedSpecificRecv) {
            ulm_exit((-1, "Error: Communicator::init: Unable to allocate "
                      "space for privateQueues.PostedSpecificRecv[proc]\n"));
        }
        // initialize locks
        // !!!!! threaded-lock
        privateQueues.PostedSpecificRecv[proc]->Lock.init();
    }                           // end proc loop

    //
    //  Allocate list of lists for Matched Ireceives
    //
    privateQueues.MatchedRecv =
        ulm_new(ProcessPrivateMemDblLinkList *, remoteGroup->groupSize);
    for (int proc = 0; proc < remoteGroup->groupSize; proc++) {
        privateQueues.MatchedRecv[proc] =
            ulm_new(ProcessPrivateMemDblLinkList, 1);
        if (!privateQueues.MatchedRecv[proc]) {
            ulm_exit((-1, "Error: Communicator::init: Unable to allocate"
                      "space for privateQueues.MatchedRecv[proc]\n"));
        }
        // Initialize locks
        // !!!!! threaded-lock
        privateQueues.MatchedRecv[proc]->Lock.init();
    }

    //
    //  Allocate list of lists for Ahead of Sequeunce frags
    //
    privateQueues.AheadOfSeqRecvFrags =
        ulm_new(ProcessPrivateMemDblLinkList *, remoteGroup->groupSize);
    for (int proc = 0; proc < remoteGroup->groupSize; proc++) {
        privateQueues.AheadOfSeqRecvFrags[proc] =
            ulm_new(ProcessPrivateMemDblLinkList, 1);
        if (!privateQueues.AheadOfSeqRecvFrags[proc]) {
            ulm_exit((-1, "Error: Communicator::init: Unable to allocate "
                      "space for privateQueues.AheadOfSeqRecvFrags[proc]\n"));
        }
        // initialize lock
        // !!!!! threaded-lock
        privateQueues.AheadOfSeqRecvFrags[proc]->Lock.init();
    }

    //
    // List of message frags recieved in sequence, but with no ireceive posted -
    //   list resides in process private memory
    //
    privateQueues.OkToMatchRecvFrags =
        ulm_new(ProcessPrivateMemDblLinkList *, nprocs());
    for (int proc = 0; proc < nprocs(); proc++) {
        privateQueues.OkToMatchRecvFrags[proc] =
            ulm_new(ProcessPrivateMemDblLinkList, 1);
        if (!privateQueues.OkToMatchRecvFrags[proc]) {
            ulm_exit((-1, "Error: Communicator::init: Unable to allocate "
                      "space for privateQueues.OkToMatchRecvFrags[proc]\n"));
        }
        // initialize locks
        // !!!!! threaded-lock
        privateQueues.OkToMatchRecvFrags[proc]->Lock.init();
    }
#ifdef SHARED_MEMORY
    //
    // List of SMP message frags recieved in sequence, but with no ireceive posted -
    //   list resides in process private memory - only 0th frag of a message
    //   is appended on this list.
    //
    privateQueues.OkToMatchSMPFrags =
        ulm_new(ProcessPrivateMemDblLinkList *, nprocs());
    for (int proc = 0; proc < nprocs(); proc++) {
        privateQueues.OkToMatchSMPFrags[proc] =
            ulm_new(ProcessPrivateMemDblLinkList, 1);
        if (!privateQueues.OkToMatchSMPFrags[proc]) {
            ulm_exit((-1, "Error: Communicator::init: Unable to allocate "
                      "space for privateQueues.OkToMatchSMPFrags[proc]\n"));
        }
        // initialize locks
        // !!!!! threaded-lock
        privateQueues.OkToMatchSMPFrags[proc]->Lock.init();
    }

#endif                          // SHARED_MEMORY

    //
    // barrier initialization - host specific code
    //
    int RetVal = barrierInit(firstInstanceOfContext);
    if (RetVal != ULM_SUCCESS) {
        ulm_err(("Error: barrierInit (%d)\n", RetVal));
        return RetVal;
    }
    //
    // initialize data structures for collective optimizations
    //
    RetVal = initializeCollectives(useSharedMemCollOpts);
    if (RetVal != ULM_SUCCESS) {
        ulm_err(("Error: initializeCollectives (%d)\n", RetVal));
        return RetVal;
    }

    // initialize multicast members
    multicast_vpid = 0;
    elan_mcast_buf = 0;
    hw_ctx_stripe = -1;

    return ULM_SUCCESS;
}

// This returns a buffer sitting at the same address on
// all hosts in the communicator (per rail).  Used by
// ulm_bcast_quadrics.  The size of the buffer is returned
// in *sz, and a pointer to the buffer is the return value
// of the method.  In case of error, zero is returned.
void* Communicator::getMcastBuf(int rail, size_t *sz)
{
#ifndef QUADRICS
    *sz = 0;
    return 0;
#else
    unsigned char *p, *q;
    ELAN3_CTX *ctx;
    int i;

    // see if we've already allocated these
    if (elan_mcast_buf && elan_mcast_buf[rail]) {
        *sz = MCAST_BUF_SZ;
        return elan_mcast_buf[rail];
    }

    // allocate buffers
    elan_mcast_buf = (unsigned char **) 
        ulm_malloc(quadricsNRails * sizeof(unsigned char *));
    for (i = 0; i < quadricsNRails; i++) {
        ctx = getElan3Ctx(i);
        elan_mcast_buf[i] = (unsigned char *) 
            elan3_allocMain(ctx, getpagesize(), MCAST_BUF_SZ);
        if (elan_mcast_buf[i] == NULL) {
            ulm_err(("Cannot allocate memory for multicast"));
            *sz = 0;
            return 0;            
        }
        
        ulm_reduce_p2p(&elan_mcast_buf[i], &q, 1, (ULMType_t *) MPI_LONG, 
                       (ULMOp_t *) MPI_MAX, 0, contextID);
        ulm_bcast_p2p(&q, 1, (ULMType_t *) MPI_LONG,
                      0, contextID);

        if (q > elan_mcast_buf[i]) {
            p = (unsigned char *) 
                elan3_allocMain(ctx, getpagesize(), q - elan_mcast_buf[i]);
            if (p == NULL) {
                ulm_err(("Cannot allocate memory for multicast"));
                *sz = 0;
                return 0;
            }            
            elan_mcast_buf[i] = q;
        }
    }

    *sz = MCAST_BUF_SZ;
    return elan_mcast_buf[rail];
#endif
}

// This method returns the multicast vpid (in *vp) for use
// on the rail "rail".  This vpid is used for hardware multicast
// on quadrics, specifically in ulm_bcast_quadrics.  The
// return value of the method is the error status (ULM_SUCCESS, etc)
// and, if no such vpid is available, then *vp = -1.
int Communicator::getMcastVpid(int rail, int *vp)
{
#ifndef QUADRICS
    *vp = -1;
    return ULM_SUCCESS;
#else
    ELAN_LOCATION loc;
    ELAN3_CTX *ctx;
    int *vpids;
    int i, rc;
    int myVp, mcast_vp;
    int lowvp, highvp;
    int _hw_ctx[4] = {0, 0, 0, 0};

    // see if we've already computed this value...
    if (multicast_vpid) {
        *vp = multicast_vpid[rail];
        return ULM_SUCCESS;
    } 
    
    // set up multicast vpids
    multicast_vpid = (int *) ulm_malloc(quadricsNRails * sizeof(int));
    for (i = 0; i < quadricsNRails; i++) {
        multicast_vpid[i] = -1;
    }

    // No inter-communicators yet
    if (communicatorType == INTER_COMMUNICATOR) {
        *vp = -1;
        return ULM_SUCCESS;
    }

    // fill out array of vpids
    vpids = (int *) ulm_malloc(localGroup->groupSize * sizeof(int));
    for (i = 0; i < localGroup->groupSize; i++) {
        vpids[i] = localGroup->mapGroupProcIDToGlobalProcID[i];
    }

    // The quadrics hardware broadcast requires a "stripe" of
    // processes across all participating hosts, e.g. process 0
    // on all hosts.  Here we determine if such a stripe exists,
    // and store it in 
    //   Communicator::hw_ctx_stripe
    // for use in ulm_bcast_quadrics.
    myVp = localGroup->mapGroupProcIDToGlobalProcID[localGroup->ProcID];
    for (i = 0; i < localGroup->groupSize; i++) {
        loc = elan3_vp2location(vpids[i],  &quadricsCap);
        _hw_ctx[loc.Context]++;
    }
    for (i = 0; i < 4; i++) {
        if (_hw_ctx[i] == localGroup->numberOfHostsInGroup) {
            hw_ctx_stripe = i;
            break;
        }
    }
    if (hw_ctx_stripe == -1) { // no stripe exists
        *vp = -1;
        return ULM_SUCCESS;
    }

    // In order to set up a broadcast vpid, we must determine
    // the processes IN THE STRIPE with the lowest, highest
    // ranks.
    lowvp = localGroup->groupSize - 1; 
    highvp = 0;
    for (i = 0; i < localGroup->groupSize; i++) {
        loc = elan3_vp2location(vpids[i],  &quadricsCap);
        if (loc.Context == hw_ctx_stripe) {
            if (vpids[i] < lowvp) {
                lowvp = vpids[i];
            }
            if (vpids[i] > highvp) {
                highvp = vpids[i];
            }
        }
    }

    // try to allocate, add the bcast vp
    for (i = 0; i < quadricsNRails; i++) {
        ctx = getElan3Ctx(i);
        multicast_vpid[i] = elan3_allocatebcastvp(ctx);
        if (multicast_vpid[i] == -1) {
            perror("elan3_allocatebcastvp returned -1 \n");
            return ULM_ERROR;
        }
        rc = elan3_addbcastvp(ctx, multicast_vpid[i], lowvp, highvp);
        if (rc < 0) {
            ulm_err(("mcast_vp=%d, lowvp=%d, highvp=%d \n",
                     mcast_vp, lowvp, highvp));
            perror("elan3_addbcastvp returned -1");
            return ULM_ERROR;
        }
    }

    ulm_free(vpids);
    *vp = multicast_vpid[rail];
    return ULM_SUCCESS;
#endif
}

/*
 *  free process private group resources
 */
int Communicator::freeCommunicator()
{
    int i;

    // reset counters
    // reset receive counter
    setBigAtomicUnsignedInt(&next_irecv_id_counter, 1);

    ulm_delete(next_expected_isendSeqs);
    ulm_delete(next_expected_isendSeqsLock);
    ulm_delete(next_isendSeqs);
    ulm_delete(next_isendSeqsLock);
    ulm_delete(recvLock);

    // free queues
    // NOTE:  these are List**, so need to walk the array, calling delete, 
    // then free everything else
    for (i = 0; i < remoteGroup->groupSize; i++) {
        ulm_delete(privateQueues.PostedSpecificRecv[i]);
        ulm_delete(privateQueues.MatchedRecv[i]);
        ulm_delete(privateQueues.AheadOfSeqRecvFrags[i]);
    }
    ulm_delete(privateQueues.PostedSpecificRecv);
    ulm_delete(privateQueues.MatchedRecv);
    ulm_delete(privateQueues.AheadOfSeqRecvFrags);

    // free list of message frags recieved in sequence, but with 
    // no ireceive posted list resides in process private memory
    for (i = 0; i < nprocs(); i++) {
        ulm_delete(privateQueues.OkToMatchRecvFrags[i]);
    }
    ulm_delete(privateQueues.OkToMatchRecvFrags);

#ifdef SHARED_MEMORY
    for (i = 0; i < nprocs(); i++) {
        ulm_delete(privateQueues.OkToMatchSMPFrags[i]);
    }
    ulm_delete(privateQueues.OkToMatchSMPFrags);
#endif                          // SHARED_MEMORY

    // free barrier resources
    int retVal = barrierFree();
    if (retVal != ULM_SUCCESS) {
        ulm_err(("Error: barrierFree (%d)\n", retVal));
        return retVal;
    }
    // free context cache
    ulm_free(commCache);

    // free attributes list
    if (attributeList) {
        ulm_free(attributeList);
        attributeList = (commAttribute_t *) 0;
    }
    // free resources for collective optimization
    freeCollectivesResources();

    return ULM_SUCCESS;
}


/*
 * check if message has been acked, and release resources if so
 */
void CheckForAckedMessages(double timeNow)
{
    // Loop over the sends that have not yet been acked.
    BaseSendDesc_t *SendDesc = 0;
    int errorCode;

    // lock list to make sure reads are atomic

    if (usethreads())
        UnackedPostedSends.Lock.lock();

    for (SendDesc = (BaseSendDesc_t *) UnackedPostedSends.begin();
         SendDesc != (BaseSendDesc_t *) UnackedPostedSends.end();
         SendDesc = (BaseSendDesc_t *) SendDesc->next) {

        int LockReturn = 1;
        if (usethreads())
            LockReturn = SendDesc->Lock.trylock();

        // process only if lock is available, else try again later
        if (LockReturn == 1) {  // we've acquired the lock
            // sanity check
            assert(SendDesc->WhichQueue == UNACKEDISENDQUEUE);
            if (SendDesc->path_m
                && SendDesc->path_m->sendDone(SendDesc, timeNow,
                                              &errorCode)) {
                if (!SendDesc->sendDone) {
                    SendDesc->requestDesc->messageDone = true;
                    SendDesc->sendDone = 1;
                }
                BaseSendDesc_t *TmpDesc = (BaseSendDesc_t *)
                    UnackedPostedSends.RemoveLinkNoLock(SendDesc);
                if (usethreads())
                    SendDesc->Lock.unlock();
                SendDesc->path_m->ReturnDesc(SendDesc);
                SendDesc = TmpDesc;
            } else {
                // unlock
                if (usethreads())
                    SendDesc->Lock.unlock();
            }
        }                       // end trylock region
    }                           // end for loop

    // unlock list
    if (usethreads())
        UnackedPostedSends.Lock.unlock();
}


#ifdef RELIABILITY_ON

/*
 * Check to see if frag needs to be retransmitted.
 */
int CheckForRetransmits()
{
    int errorCode = ULM_SUCCESS;

    BaseSendDesc_t *sendDesc = 0;
    // Loop over sends that have not been acked

    // lock list to make sure reads are atomic
    UnackedPostedSends.Lock.lock();

    for (sendDesc = (BaseSendDesc_t *) UnackedPostedSends.begin();
         sendDesc != (BaseSendDesc_t *) UnackedPostedSends.end();
         sendDesc = (BaseSendDesc_t *) sendDesc->next) {

        // lock descriptor
        sendDesc->Lock.lock();

        //check for retransmit
        if (sendDesc->path_m->retransmitP(sendDesc)) {
            BaseSendDesc_t *TmpDesc = 0;
            do {
                if (sendDesc->path_m->resend(sendDesc, &errorCode)) {
                    // move to incomplete isend list
                    TmpDesc = (BaseSendDesc_t *)
                        UnackedPostedSends.RemoveLinkNoLock(sendDesc);
                    IncompletePostedSends.Append(sendDesc);
                } else if (errorCode == ULM_SUCCESS) {
                    break;
                } else if (errorCode == ULM_ERR_BAD_PATH) {
// revisit                    // unbind message from old path
// revisit                    sendDesc->path_m->unbind(sendDesc, (int *) 0, 0);
// revisit                    // select a new path
// revisit                    Communicator *commPtr =
// revisit                        communicators[sendDesc->ctx_m];
// revisit                    errorCode =
// revisit                        (commPtr-> pt2ptPathSelectionFunction) ((void *) sendDesc);
// revisit                    if (errorCode != ULM_SUCCESS) {
// revisit                        sendDesc->Lock.unlock();
// revisit                        ulm_exit((-1,
// revisit                                  "Error: CheckForRetransmits: no path "
// revisit                                  "available to send message\n"));
// revisit                    }
// revisit                    // initialize the descriptor for this path
// revisit                    sendDesc->path_m->init(sendDesc);
// revisit
// revisit                    // put the descriptor on the incomplete list
// revisit                    TmpDesc = (BaseSendDesc_t *)
// revisit                        UnackedPostedSends.RemoveLinkNoLock(sendDesc);
// revisit                    IncompletePostedSends.Append(sendDesc);
                } else {
                    // unbind should free frag descriptors, etc.
                    sendDesc->path_m->unbind(sendDesc, (int *) 0, 0);
                    // resend failed with fatal error
                    sendDesc->Lock.unlock();
                    ulm_exit((-1,
                              "Error: CheckForRetransmits: resend failed "
                              "with fatal error\n"));
                }
            } while (errorCode != ULM_SUCCESS);
            // unlock descriptor
            sendDesc->Lock.unlock();
            // reset sendDesc to previous value for proper list iteration
            if (TmpDesc)
                sendDesc = TmpDesc;
        } else {
            // unlock descriptor
            sendDesc->Lock.unlock();
        }

    }

    // unlock list
    UnackedPostedSends.Lock.unlock();

    // loop over sends that have not yet been copied into pinned memory

    // lock list to make sure reads are atomic
    IncompletePostedSends.Lock.lock();

    for (sendDesc = (BaseSendDesc_t *) IncompletePostedSends.begin();
         sendDesc != (BaseSendDesc_t *) IncompletePostedSends.end();
         sendDesc = (BaseSendDesc_t *) sendDesc->next) {

        // lock descriptor
        sendDesc->Lock.lock();

        //check for retransmit
        if (sendDesc->path_m->retransmitP(sendDesc)) {
            do {
                bool resendnow =
                    sendDesc->path_m->resend(sendDesc, &errorCode);
                if (!resendnow) {
                    if (errorCode == ULM_SUCCESS) {
                        break;
                    } else if (errorCode == ULM_ERR_BAD_PATH) {
                        // rebind message to new path
// revisit                        sendDesc->path_m->unbind(sendDesc, (int *) 0, 0);
// revisit                        // select a new path
// revisit                        Communicator *commPtr =
// revisit                            communicators[sendDesc->ctx_m];
// revisit                        errorCode =
// revisit                            ((commPtr->
// revisit                              pt2ptPathSelectionFunction)) ((void *)
// revisit                                                            sendDesc);
// revisit                        if (errorCode != ULM_SUCCESS) {
// revisit                            sendDesc->Lock.unlock();
// revisit                            ulm_err(("Error: CheckForRetransmits: no path available to send message\n"));
// revisit                            return errorCode;
// revisit                        }
// revisit                        // initialize the descriptor for this path
// revisit                        sendDesc->path_m->init(sendDesc);
                    } else {
                        // unbind should free frag descriptors, etc.
                        sendDesc->path_m->unbind(sendDesc, (int *) 0, 0);
                        // resend failed with fatal error
                        sendDesc->Lock.unlock();
                        ulm_exit((-1, "Error: CheckForRetransmits: resend "
                                  "failed with fatal error.\n"));
                    }
                }
            } while (errorCode != ULM_SUCCESS);
            sendDesc->Lock.unlock();
        } else {
            // unlock descriptor
            sendDesc->Lock.unlock();
        }

    }

    // unlock list
    IncompletePostedSends.Lock.unlock();
    return errorCode;
}
#endif


/*
 * send acks that have not yet been sent
 */
void SendUnsentAcks()
{
    BaseRecvFragDesc_t *tmpDesc;

    // check to see if any acks to send

    if (UnprocessedAcks.size() == 0)
        return;

    // Loop over the acks that have not yet been sent

    if (usethreads())
        UnprocessedAcks.Lock.lock();
    for (BaseRecvFragDesc_t * AckDesc =
         (BaseRecvFragDesc_t *) UnprocessedAcks.begin();
         AckDesc != (BaseRecvFragDesc_t *) UnprocessedAcks.end();
         AckDesc = (BaseRecvFragDesc_t *) AckDesc->next) {
        if (AckDesc->AckData()) {
            // if ack sent, remove from list
            tmpDesc =
                (BaseRecvFragDesc_t *) UnprocessedAcks.
                RemoveLinkNoLock(AckDesc);
            AckDesc->ReturnDescToPool(getMemPoolIndex());
            AckDesc = tmpDesc;
        }
    }
    if (usethreads())
        UnprocessedAcks.Lock.unlock();
}


//
// This routine is used to copy out data from the receive buffers
// to user buffers.  Acks are also initiated.
//
void Communicator::ProcessMatchedData(RecvDesc_t * MatchedPostedRecvHeader,
                                      BaseRecvFragDesc_t * DataHeader,
                                      double timeNow, bool * recvDone)
{
    // CopyToAppLock returns >= 0 if data is okay, and -1 if data is corrupt...
    // we don't care either way since we call AckData in either case...
    MatchedPostedRecvHeader->CopyToAppLock((void *) DataHeader, recvDone);
    bool acked = DataHeader->AckData(timeNow);
    if (acked) {
        // return descriptor to pool
        DataHeader->ReturnDescToPool(getMemPoolIndex());
    } else {
        // move descriptor to list of unsent acks
        DataHeader->WhichQueue = FRAGSTOACK;
        UnprocessedAcks.Append(DataHeader);
    }
}


int Communicator::setupSharedMemoryQueuesInPrivateMemory()
{
    /* set index infomration */
    indexSharedMemoryQueue = -1;
    shareMemQueuesUsed = false;

    return ULM_SUCCESS;
}


int Communicator::initializeCollectives(int useSharedMemCollOpts)
{
    int returnValue = ULM_SUCCESS;

    if (useSharedMemCollOpts) {
        /*
         * shared memory collective optimizations
         */

        /* lock resource queue */
        SharedMemForCollective->lock.lock();

        int poolID = -1;
        int freeElement = -1;
        int localCommRoot = localGroup->mapGroupProcIDToGlobalProcID[0];
        /* check to see if the communicator has already grabbed a pool element */
        for (int i = 0; i < MAX_COMMUNICATOR_SHARED_OBJECTS; i++) {
            if ((SharedMemForCollective->collCtlData[i].contextID ==
                 contextID)
                && (SharedMemForCollective->collCtlData[i].localCommRoot ==
                    localCommRoot)) {
                poolID = i;
                break;
            }
            if ((!(SharedMemForCollective->collCtlData[i].inUse))
                && (freeElement == -1)) {
                freeElement = i;
            }
        }
        if (poolID != -1) {

            /*  other process alreay allocat ed element */
            collectivePoolIndex = poolID;
            collectiveIndexDesc = 0;
            sharedCollectiveData =
                SharedMemForCollective->collCtlData[poolID].collDescriptor;
        } else if (freeElement != -1) {

            /* first element to attach to pool */
            SharedMemForCollective->collCtlData[freeElement].inUse = 1;
            SharedMemForCollective->collCtlData[freeElement].contextID =
                contextID;
            SharedMemForCollective->collCtlData[freeElement].
                localCommRoot = localCommRoot;
            SharedMemForCollective->collCtlData[freeElement].nFreed = 0;
            for (int desc = 0; desc < N_COLLCTL_PER_COMM; desc++) {
                SharedMemForCollective->collCtlData[freeElement].
                    collDescriptor[desc].tag = 0;
                SharedMemForCollective->collCtlData[freeElement].
                    collDescriptor[desc].flag = 0;
            }
            collectiveIndexDesc = 0;
            collectivePoolIndex = freeElement;
            sharedCollectiveData =
                SharedMemForCollective->collCtlData[freeElement].
                collDescriptor;
            for (int dN = 0; dN < N_COLLCTL_PER_COMM; dN++) {

                for (int lp = 0;
                     lp < localGroup->
                     groupHostData[localGroup->hostIndexInGroup].
                     nGroupProcIDOnHost; lp++) {
                    sharedCollectiveData[dN].controlFlags[lp].flag =
                        READY_TO_READ;
                }
            }
        } else {

            /* no resource available */
            returnValue = ULM_ERR_OUT_OF_RESOURCE;
            /* unlock resource */
            SharedMemForCollective->lock.unlock();

            ulm_err(("Error: Communicator::initializeCollectives: "
                     "out of shared memory:  comm = %d\n", contextID));
            return returnValue;
        }

        /* unlock resource */
        SharedMemForCollective->lock.unlock();

    } else {
        /*
         * process private collective optimizations - really for comm_self
         */
        /* figure out size of work region */
        size_t collectiveMem = MIN_COLL_MEM_PER_PROC * local_nprocs();
        if (collectiveMem < SMPPAGESIZE)
            collectiveMem = SMPPAGESIZE;

        /* allocate pool */
        sharedCollectiveData =
            (CollectiveSMBuffer_t *)
            ulm_malloc(sizeof(CollectiveSMBuffer_t)
                       * N_COLLCTL_PER_COMM);
        if (!sharedCollectiveData) {
            return ULM_ERR_OUT_OF_RESOURCE;
        }

        collectiveIndexDesc = 0;
        collectivePoolIndex = 0;
        for (int i = 0; i < N_COLLCTL_PER_COMM; i++) {
            sharedCollectiveData[i].allocRefCount = 0;
            sharedCollectiveData[i].freeRefCount = -1;
            sharedCollectiveData[i].tag = 0;
            sharedCollectiveData[i].flag = 0;
            sharedCollectiveData[i].max_length = collectiveMem;
            sharedCollectiveData[i].mem = (void *)
                ulm_malloc(collectiveMem);
            if (!sharedCollectiveData[i].mem) {
                return ULM_ERR_OUT_OF_RESOURCE;
            }
            sharedCollectiveData[i].controlFlags = (flagElement_t *)
                ulm_malloc(local_nprocs() * sizeof(flagElement_t));
            if (!sharedCollectiveData[i].controlFlags) {
                return ULM_ERR_OUT_OF_RESOURCE;
            }
            sharedCollectiveData[i].lock.init();

        }

    }

    /* initialize collectiveOpt */
    int numHosts = localGroup->numberOfHostsInGroup;

    /* setup send/recv pattern */
    collectiveOpt.hostExchangeList =
        (list_t *) ulm_malloc(sizeof(list_t) * numHosts);
    if (!(collectiveOpt.hostExchangeList)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }

    /* nExtra and extraOffset are used to handle getting data to and from
     *   the processes that are not in the "base" power of 2 set  */
    returnValue =
        setupSendRecvTree(numHosts, collectiveOpt.hostExchangeList,
                          &(collectiveOpt.nExtra),
                          &(collectiveOpt.extraOffset),
                          &(collectiveOpt.treeDepth));
    if (returnValue != ULM_SUCCESS) {
        return returnValue;
    }

    /* setup the order in which the data will be arrive from the remote hosts */
    collectiveOpt.dataHostOrder =
        (int *) ulm_malloc(sizeof(int) * numHosts);
    if (!(collectiveOpt.dataHostOrder)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    int myHostRank = localGroup->hostIndexInGroup;
    int topNode;
    if ((collectiveOpt.nExtra > 0)
        && (myHostRank >= collectiveOpt.extraOffset)) {
        topNode = myHostRank - collectiveOpt.extraOffset;
    } else {
        topNode = myHostRank;
    }
    int cnt = 0;
    getSendingHostRank(collectiveOpt.hostExchangeList, &cnt,
                       collectiveOpt.dataHostOrder,
                       collectiveOpt.treeDepth, topNode,
                       collectiveOpt.nExtra, collectiveOpt.extraOffset);

    /* size of shared memory buffer allocated for the local host, and
     * the upper limit on the amount of data each host contirbutes
     */
    int maxLocalProcs = 0;
    /* find the host with the most number of processes */
    for (int host = 0; host < numHosts; host++) {
        if (localGroup->groupHostData[host].nGroupProcIDOnHost >
            maxLocalProcs)
            maxLocalProcs =
                localGroup->groupHostData[host].nGroupProcIDOnHost;
    }
    collectiveOpt.perRankStripeSize = sharedCollectiveData[0].max_length;
    collectiveOpt.perRankStripeSize /= (numHosts * maxLocalProcs);
    /* round off to h/w friendly alignment */
    collectiveOpt.perRankStripeSize =
        ((collectiveOpt.perRankStripeSize / CACHE_ALIGNMENT) *
         CACHE_ALIGNMENT);
    collectiveOpt.localStripeSize =
        collectiveOpt.perRankStripeSize * maxLocalProcs;

    /* calulate the amount of data that each host will send around
     *   and the number of stripes for this collective  */
    collectiveOpt.dataToSend =
        (size_t *) ulm_malloc(numHosts * sizeof(size_t));
    if (!(collectiveOpt.dataToSend)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    /* allocate array to store the amount of data that each host sends in
     *   the current stripe - this is not a running sum */
    collectiveOpt.dataToSendThisStripe =
        (size_t *) ulm_malloc(numHosts * sizeof(size_t));
    if (!(collectiveOpt.dataToSendThisStripe)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }

    /* array to keep track of which local process's data is being read in */
    collectiveOpt.currentLocalRank =
        (int *) ulm_malloc(numHosts * sizeof(int));
    if (!(collectiveOpt.currentLocalRank)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }

    /* array to keep track of the data offset into the current local process's
     *   data that is in the shared memory buffer. */
    collectiveOpt.currentRankBytesRead =
        (size_t *) ulm_malloc(numHosts * sizeof(size_t));
    if (!(collectiveOpt.currentRankBytesRead)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }

    /* setup array indicating how much data to send this time around  */
    collectiveOpt.dataToSendNow =
        (size_t *) ulm_malloc(sizeof(size_t) * numHosts);
    if (!(collectiveOpt.dataToSendNow)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }

    /* scratch space */
    collectiveOpt.recvScratchSpace = (size_t *) ulm_malloc(sizeof(size_t) *
                                                           localGroup->
                                                           groupSize);
    if (!(collectiveOpt.recvScratchSpace)) {
        return ULM_ERR_OUT_OF_RESOURCE;
    }

    /*
     * Shared buffer offset used for reduce on this host, and the
     * minimum such offset across all hosts.  Each process uses a
     * segment of the shared memory buffer starting at
     * reduceOffset * onOnHostProcID.
     *
     * 8-byte aligned
     */
    collectiveOpt.reduceOffset =
        (sharedCollectiveData[0].max_length /
         localGroup->onHostGroupSize) & ~7;
    collectiveOpt.minReduceOffset =
        (sharedCollectiveData[0].max_length /
         localGroup->maxOnHostGroupSize) & ~7;
    collectiveOpt.maxReduceExtent =
        collectiveOpt.minReduceOffset - 2 * sizeof(int);

    return returnValue;
}


void Communicator::freeCollectivesResources()
{
    if (useSharedMemForCollectives) {
        /*
         * shared memory collectives optimization
         */
        SharedMemForCollective->lock.lock();
        SharedMemForCollective->collCtlData[collectivePoolIndex].nFreed++;
        int nToFree=localGroup->hostIndexInGroup;
        nToFree=localGroup->groupHostData[nToFree].nGroupProcIDOnHost;
        /* free resource if last to free */
        if (SharedMemForCollective->collCtlData[collectivePoolIndex].
            nFreed == nToFree) {
            /*nFreed == localGroup->groupSize) { */
            SharedMemForCollective->collCtlData[collectivePoolIndex].
                contextID = -1;
            SharedMemForCollective->collCtlData[collectivePoolIndex].
                localCommRoot = -1;
            SharedMemForCollective->collCtlData[collectivePoolIndex].
                inUse = 0;
        }
        SharedMemForCollective->lock.unlock();
    } else {
        /*
         * process private collectives resource
         */
        for (int i = 0; i < N_COLLCTL_PER_COMM; i++) {
            ulm_free(sharedCollectiveData[i].mem);
            ulm_free(sharedCollectiveData[i].controlFlags);
        }
        ulm_free(sharedCollectiveData);
    }

    assert(collectiveOpt.recvScratchSpace != NULL);
    ulm_free(collectiveOpt.recvScratchSpace);

    int numHosts = localGroup->numberOfHostsInGroup;
    for (int host = 0; host < numHosts - collectiveOpt.nExtra; host++) {
        if (collectiveOpt.hostExchangeList[host].length) {
            assert(collectiveOpt.hostExchangeList[host].list);
            ulm_free(collectiveOpt.hostExchangeList[host].list);
        }
    }

    assert(collectiveOpt.hostExchangeList != NULL);
    ulm_free(collectiveOpt.hostExchangeList);

    assert(collectiveOpt.dataHostOrder != NULL);
    ulm_free(collectiveOpt.dataHostOrder);
    assert(collectiveOpt.dataToSend != NULL);
    ulm_free(collectiveOpt.dataToSend);
    assert(collectiveOpt.dataToSendThisStripe != NULL);
    ulm_free(collectiveOpt.dataToSendThisStripe);
    assert(collectiveOpt.currentLocalRank != NULL);
    ulm_free(collectiveOpt.currentLocalRank);
    assert(collectiveOpt.currentRankBytesRead != NULL);
    ulm_free(collectiveOpt.currentRankBytesRead);
    assert(collectiveOpt.dataToSendNow != NULL);
    ulm_free(collectiveOpt.dataToSendNow);
}
