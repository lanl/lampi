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

#include <assert.h>

#include "internal/mpi.h"
#include "queue/Communicator.h"
#include "queue/globals.h"
#include "util/Utility.h"
#include "mem/ULMMallocMacros.h"
#include "collective/coll_fns.h"
#include "util/inline_copy_functions.h"


/*
 * This routine is used to copy data to the final destination for the
 * Gather routine.
 */
static void copyDataFromSharedBufferGather(Communicator *commPtr,
                                           void *destinationBuffer,
                                           void *sharedBuffer,
                                           int *dataHostOrder,
                                           int rootHost, int recvcount,
                                           int numHosts,
                                           ULMType_t *recvtype,
                                           int stripeID)
{
    /* set source buffer pointer */
    void *sourcePointer = sharedBuffer;

    if (recvtype->layout == CONTIGUOUS) {

        /*
         * contiguous data
         */

        /* loop over hosts */
        for (int hostIdx = 0; hostIdx < numHosts; hostIdx++) {
            int host = dataHostOrder[hostIdx];
            host += rootHost;
            if (host >= numHosts)
                host -= numHosts;

            /* loop over the local procs */
            int nLocalRanks =
                commPtr->localGroup->groupHostData[host].
                nGroupProcIDOnHost;
            for (int proc = 0; proc < nLocalRanks; proc++) {
                int groupRank =
                    commPtr->localGroup->groupHostData[host].
                    groupProcIDOnHost[proc];
                size_t rankOffset =
                    stripeID * commPtr->collectiveOpt.perRankStripeSize;
                ssize_t bytesToCopyNow =
                    recvcount * recvtype->packed_size - rankOffset;
                if (bytesToCopyNow <= 0)
                    continue;
                if (bytesToCopyNow >
                    commPtr->collectiveOpt.perRankStripeSize)
                    bytesToCopyNow =
                        commPtr->collectiveOpt.perRankStripeSize;

                /* caluclate destination buffer pointer */

                void *destination = (void *) ((char *) destinationBuffer +
                                              groupRank *
                                              (recvtype->extent *
                                               recvcount) + rankOffset);

                /* copy data to destination */
                MEMCOPY_FUNC(sourcePointer, destination, bytesToCopyNow);

                /* reset calculate source buffer pointer */
                sourcePointer =
                    (void *) ((char *) sourcePointer + bytesToCopyNow);

            }
        }                       /* end host list */

    } else {

        /*
         * non-contiguous data
         */
        /* loop over hosts */
        void *sourcePointer = sharedBuffer;
        for (int hostIdx = 0; hostIdx < numHosts; hostIdx++) {
            int host = dataHostOrder[hostIdx];
            host += rootHost;
            if (host >= numHosts)
                host -= numHosts;

            /* loop over the local procs */
            int nLocalRanks =
                commPtr->localGroup->groupHostData[host].
                nGroupProcIDOnHost;
            for (int proc = 0; proc < nLocalRanks; proc++) {
                int groupRank =
                    commPtr->localGroup->groupHostData[host].
                    groupProcIDOnHost[proc];
                size_t rankOffset =
                    stripeID * commPtr->collectiveOpt.perRankStripeSize;
                ssize_t bytesToCopyNow =
                    recvcount * recvtype->packed_size - rankOffset;
                if (bytesToCopyNow <= 0)
                    continue;
                if (bytesToCopyNow >
                    commPtr->collectiveOpt.perRankStripeSize)
                    bytesToCopyNow =
                        commPtr->collectiveOpt.perRankStripeSize;

                /* copy data to destination */
                /* base offset for data from the current sourceRank */
                void *dest = (void *) ((char *) destinationBuffer +
                                       groupRank * (recvtype->extent *
                                                    recvcount));

                ssize_t bytesCopied;
                contiguous_to_noncontiguous_copy(recvtype, dest,
                                                 &bytesCopied, rankOffset,
                                                 bytesToCopyNow,
                                                 sourcePointer);

                sourcePointer =
                    (void *) ((char *) sourcePointer + bytesCopied);

            }
        }                       /* end host list */
    }

    return;
}

/*
 * this version of the gather is intended for the case in which the
 * data all fits into 1 stripe of the shared memory buffer.  It this
 * case there is no need to pay the overhead penatly of the parallel
 * write to the shared memroy buffer, since this happens
 * "automatically".
 */
int ulm_gather1Stripe(void *sendbuf, int sendcount, ULMType_t *sendtype,
                      void *recvbuf, int recvcount, ULMType_t *recvtype,
                      size_t *dataToSend, int root, Communicator *commPtr)
{
    int returnCode = ULM_SUCCESS;

    /*  while loop over message stripes.
     *
     *  At each stage a shared memory buffer is filled in by the processes
     *    on this host.  This data is sent to other hosts, and read by the
     *    local processes.
     *
     *  The algorithm used to exchange data between inolves partitioning the hosts
     *    into two groups, one the largest number of host ranks that is a
     *    power of 2 (called the exchange group of size exchangeSize) and the remainder.
     *    The data exchange pattern is:
     *    - the remainder group hosts send their data to myHostRank-exchangeSize
     *    - the exchange group hosts exchange data between sets of these
     *    hosts.  The size of the first set is 2, then 4, then 8, etc. with
     *    each host in the lower half of the set exchanging data with one in
     *    the upper half of the set.
     *    - the remainder group hosts get their data to myHostRank-exchangeSize
     */

    int numHosts = commPtr->localGroup->numberOfHostsInGroup;
    size_t *dataToSendThisStripe =
        commPtr->collectiveOpt.dataToSendThisStripe;
    size_t *dataToSendNow = commPtr->collectiveOpt.dataToSendNow;
    size_t localStripeSize = commPtr->collectiveOpt.localStripeSize;
    int *dataHostOrder = commPtr->localGroup->gatherDataNodeList[0];
    int rootHost = commPtr->localGroup->mapGroupProcIDToHostID[root];
    rootHost = commPtr->localGroup->groupHostDataLookup[rootHost];

    /* get tag - single tag is sufficient, since send/recv/tag is a unique
     *   triplet for all sends
     */
    long long tag = commPtr->get_base_tag(1);

    /* get collectives descriptor from the communicator */
    CollectiveSMBuffer_t *collDesc = commPtr->getCollectiveSMBuffer(tag);

    /* shared buffer */
    void *RESTRICT_MACRO sharedBuffer = collDesc->mem;

    /* figure out to which stripes this process contributes */

    /*
     * Fill in shared memory buffer
     */
    int stripeID = 0;
    copyDataToSharedBuffer1Stripe(stripeID, sendbuf, sharedBuffer,
                                  sendcount, sendtype, commPtr);

    /* set flag for releasing after interhost data exchange -
     *   must be set before this barrier to avoid race conditions.
     *   saves a barrier call after the data exchange
     */
    if (commPtr->localGroup->onHostProcID == 0) {
        collDesc->flag = 0;
    }

    /* don't exhange data until all have written to the shared
     */
    commPtr->smpBarrier(commPtr->barrierData);

    /*
     * interhost data exchange
     */
    for (int host = 0; host < numHosts; host++) {
        dataToSendNow[host] = localStripeSize;
        if (localStripeSize > dataToSend[host])
            dataToSendNow[host] = dataToSend[host];
        dataToSend[host] -= dataToSendNow[host];
        dataToSendThisStripe[host] = dataToSendNow[host];
    }

    if (commPtr->localGroup->onHostProcID == 0) {

        /* if local commRoot, first exchange data with other hosts, and
         *   then read in all data
         */

        returnCode = ulm_gather_interhost(commPtr->contextID, sharedBuffer,
                                          &(commPtr->localGroup->
                                            interhostGatherScatterTree),
                                          dataToSendNow, root, tag);
        if (returnCode != ULM_SUCCESS) {
            commPtr->releaseCollectiveSMBuffer(collDesc);
            return returnCode;
        }

        /* set flag indicating data exchange is done */
        collDesc->flag = 1;

    }
    /* spin until all data has been received */
    ULM_SPIN_AND_MAKE_PROGRESS(!collDesc->flag);


    /* read the exhanged data */
    if (root == commPtr->localGroup->ProcID) {
        copyDataFromSharedBufferGather(commPtr, recvbuf, sharedBuffer,
                                       dataHostOrder, rootHost, recvcount,
                                       numHosts, recvtype, 0);
    }

    /* return collectives descriptor to the communicator */
    commPtr->releaseCollectiveSMBuffer(collDesc);

    return returnCode;
}


/*
 * this version of gather is used when more that 1 stripe of data
 * is needed to send all the data.  It is designed to make sure all
 * processes contribute data to each stripe, and better parallelizes
 * the "send side" of the alrogithm.
 */
int ulm_gatherMultipleStripes(void *sendbuf, int sendcount,
                              ULMType_t *sendtype, void *recvbuf,
                              int recvcount, ULMType_t *recvtype, int root,
                              Communicator *commPtr, int numStripes)
{
    int returnCode = ULM_SUCCESS;
    void *RESTRICT_MACRO recvBuff = recvbuf;
    void *RESTRICT_MACRO sendBuff = sendbuf;

    /*  while loop over message stripes.
     *
     *  At each stage a shared memory buffer is filled in by the processes
     *    on this host.  This data is sent to other hosts, and read by the
     *    local processes.
     *
     *  The algorithm used to exchange data between inolves partitioning the hosts
     *    into two groups, one the largest number of host ranks that is a
     *    power of 2 (called the exchange group of size exchangeSize) and the remainder.
     *    The data exchange pattern is:
     *    - the remainder group hosts send their data to myHostRank-exchangeSize
     *    - the exchange group hosts exchange data between sets of these
     *    hosts.  The size of the first set is 2, then 4, then 8, etc. with
     *    each host in the lower half of the set exchanging data with one in
     *    the upper half of the set.
     *    - the remainder group hosts get their data to myHostRank-exchangeSize
     */

    int numHosts = commPtr->localGroup->numberOfHostsInGroup;
    int myLocalRank = commPtr->localGroup->onHostProcID;
    size_t *dataToSendNow = commPtr->collectiveOpt.dataToSendNow;
    int *dataHostOrder = commPtr->localGroup->gatherDataNodeList[0];
    int rootHost = commPtr->localGroup->mapGroupProcIDToHostID[root];
    rootHost = commPtr->localGroup->groupHostDataLookup[rootHost];

    /* get tag - single tag is sufficient, since send/recv/tag is a unique
     *   triplet for all sends
     */
    long long tag = commPtr->get_base_tag(1);

    /* get collectives descriptor from the communicator */
    CollectiveSMBuffer_t *collDesc = commPtr->getCollectiveSMBuffer(tag);

    /* shared buffer */
    void *RESTRICT_MACRO sharedBuffer = collDesc->mem;


    for (int stripeID = 0; stripeID < numStripes; stripeID++) {

        /* write data to source buffer, if this process contributes to this
         *   stripeID
         */
        ssize_t leftToCopy = recvcount * recvtype->packed_size -
            stripeID * commPtr->collectiveOpt.perRankStripeSize;

        if (leftToCopy > 0) {

            /*
             * Fill in shared memory buffer
             */
            ssize_t bytesToCopy = leftToCopy;
            if (bytesToCopy > commPtr->collectiveOpt.perRankStripeSize)
                bytesToCopy = commPtr->collectiveOpt.perRankStripeSize;
            size_t bytesAlreadyCopied =
                stripeID * commPtr->collectiveOpt.perRankStripeSize;
            size_t offsetIntoSharedBuffer = 0;
            for (int proc = 0; proc < myLocalRank; proc++) {
                ssize_t bytesToCopyNow =
                    recvcount * recvtype->packed_size -
                    stripeID * commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow >
                    commPtr->collectiveOpt.perRankStripeSize)
                    bytesToCopyNow =
                        commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow > 0)
                    offsetIntoSharedBuffer += bytesToCopyNow;
            }

            copyDataToSharedBufferMultipleStripesGather(bytesToCopy,
                                                        bytesAlreadyCopied,
                                                        offsetIntoSharedBuffer,
                                                        sendBuff,
                                                        sharedBuffer,
                                                        sendtype);
        }

        /* set flag for releasing after interhost data exchange -
         *   must be set before this barrier to avoid race conditions.
         *   saves a barrier call after the data exchange
         */
        if (commPtr->localGroup->onHostProcID == 0) {
            collDesc->flag = 0;
        }

        /* don't send data until all have written to the shared buffer
         */
        commPtr->smpBarrier(commPtr->barrierData);

        /*
         * Read the shared memory buffer - interhost data exchange
         */
        for (int host = 0; host < numHosts; host++) {
            dataToSendNow[host] = 0;
            int nLocalRanks =
                commPtr->localGroup->groupHostData[host].
                nGroupProcIDOnHost;
            for (int proc = 0; proc < nLocalRanks; proc++) {
                ssize_t bytesToCopyNow =
                    recvcount * recvtype->packed_size -
                    stripeID * commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow <= 0)
                    continue;
                if (bytesToCopyNow >
                    commPtr->collectiveOpt.perRankStripeSize)
                    bytesToCopyNow =
                        commPtr->collectiveOpt.perRankStripeSize;
                dataToSendNow[host] += bytesToCopyNow;
            }
        }

        if (commPtr->localGroup->onHostProcID == 0) {

            /* if local commRoot, first exchange data with other hosts, and
             *   then read in all data
             */

            returnCode =
                ulm_gather_interhost(commPtr->contextID, sharedBuffer,
                                     &(commPtr->localGroup->
                                       interhostGatherScatterTree),
                                     dataToSendNow, root, tag);
            if (returnCode != ULM_SUCCESS) {
                commPtr->releaseCollectiveSMBuffer(collDesc);
                return returnCode;
            }

            /* memory barrier to ensure that all data has been written before setting flag */
            mb();

            /* set flag indicating data exchange is done */
            collDesc->flag = 1;

        }
        /* spin until all data has been received */
        ULM_SPIN_AND_MAKE_PROGRESS(!collDesc->flag);

        /* memory barrier... */
        mb();

        /* root reads the exhanged data */
        if (commPtr->localGroup->ProcID == root) {
            copyDataFromSharedBufferGather(commPtr, recvBuff, sharedBuffer,
                                           dataHostOrder, rootHost,
                                           recvcount, numHosts, recvtype,
                                           stripeID);
        }

        /* spin until all data has been received */
        if (numStripes > 1)
            commPtr->smpBarrier(commPtr->barrierData);

    }                           /* end stripeID loop */

    /* return collectives descriptor to the communicator */
    commPtr->releaseCollectiveSMBuffer(collDesc);

    return returnCode;
}


extern "C" int ulm_gather(void *sendbuf, int sendcount,
                          ULMType_t *sendtype, void *recvbuf,
                          int recvcount, ULMType_t *recvtype, int root,
                          int comm)
{
    int returnCode = ULM_SUCCESS;
    Communicator *commPtr = (Communicator *) communicators[comm];
    int numHosts = commPtr->localGroup->numberOfHostsInGroup;
    size_t localStripeSize = commPtr->collectiveOpt.localStripeSize;
    size_t *dataToSend = commPtr->collectiveOpt.dataToSend;

    int numStripes = 0;
    size_t maxBytesPerProc = recvcount * recvtype->packed_size;
    for (int host = 0; host < numHosts; host++) {

        int nLocalProcs =
            commPtr->localGroup->groupHostData[host].nGroupProcIDOnHost;
        size_t totalBytes =
            nLocalProcs * recvcount * recvtype->packed_size;
        int nStripesOnThisHost;
        if (totalBytes == 0) {
            nStripesOnThisHost = 1;
            numStripes = 1;
        } else {
            nStripesOnThisHost = ((totalBytes - 1) / localStripeSize) + 1;
            if (nStripesOnThisHost > numStripes)
                numStripes = nStripesOnThisHost;
        }

        /* fill in the amount of data that host "host" has to exchange */
        dataToSend[host] = totalBytes;
    }

    /* decide which algorithm to use */
    if (numStripes > 1) {
        numStripes =
            ((maxBytesPerProc -
              1) / commPtr->collectiveOpt.perRankStripeSize) + 1;
        returnCode =
            ulm_gatherMultipleStripes(sendbuf, sendcount, sendtype,
                                      recvbuf, recvcount, recvtype, root,
                                      commPtr, numStripes);
    } else {
        returnCode =
            ulm_gather1Stripe(sendbuf, sendcount, sendtype, recvbuf,
                              recvcount, recvtype, dataToSend, root,
                              commPtr);
    }

    /* set MPI return type */
    returnCode =
        (returnCode == ULM_SUCCESS) ? MPI_SUCCESS : _mpi_error(returnCode);
    if (returnCode != MPI_SUCCESS) {
        _mpi_errhandler(comm, returnCode, __FILE__, __LINE__);
    }

    return returnCode;
}
