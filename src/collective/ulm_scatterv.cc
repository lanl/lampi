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



#include "queue/Communicator.h"
#include "queue/globals.h"
#include "mem/ULMMallocMacros.h"
#include "util/Utility.h"
#include "util/inline_copy_functions.h"
#include "internal/mpi.h"
#include "collective/coll_fns.h"
#include "internal/type_copy.h"


extern "C" int ulm_scatterv(void *sendbuf, int *sendcounts, int *displs,
                            ULMType_t *sendtype, void *recvbuf,
                            int recvcount, ULMType_t *recvtype, int root,
                            int comm)
{
    int returnValue = ULM_SUCCESS;
    /* get communicator and group pointers */
    Communicator *commPtr = (Communicator *) communicators[comm];
    Group *group = commPtr->localGroup;

    /* get tag - single tag is sufficient, since send/recv/tag is a unique
     *   triplet for all sends
     */
    long long tag = commPtr->get_base_tag(1);

    /* get collectives descriptor from the communicator */
    CollectiveSMBuffer_t *collDesc =
        commPtr->getCollectiveSMBuffer(tag, ULM_COLLECTIVE_SCATTERV);

    /* shared buffer */
    void *RESTRICT_MACRO sharedBuffer = collDesc->mem;

    /* the root host has the data for all hosts, so it computes the setup
     * information, and broadcasts this to all the rest of the hosts
     *   This infomation include - number of bytes each proc will send
     */

    /* check to make sure that is enough shared memory to hold the
     * bytesPerProc array.
     */
    int nProcs = commPtr->localGroup->groupSize;
    int myLocalRank = commPtr->localGroup->onHostProcID;
    if (commPtr->sharedCollectiveData->max_length <
        (sizeof(size_t) * nProcs)) {
        ulm_err(("Error: in ulm_gatherv - not enough space for interhost broadcast\n"));
        commPtr->releaseCollectiveSMBuffer(collDesc);
        return ULM_ERR_OUT_OF_RESOURCE;
    }

    size_t *bytesPerProc = commPtr->collectiveOpt.recvScratchSpace;
    size_t *bpc = (size_t *) sharedBuffer;
    int myRank = commPtr->localGroup->ProcID;
    if (myRank == root) {
        for (int p = 0; p < nProcs; p++) {
            bpc[p] = sendcounts[p] * sendtype->packed_size;
        }
        mb();
    }

    /* broadcast this information to each host */
    size_t len = nProcs * sizeof(size_t);
    returnValue =
        ulm_bcast_interhost(bpc, len, (ULMType_t *) MPI_BYTE, root, comm);
    if (returnValue != ULM_SUCCESS) {
        ulm_err(("Error: ulm_bcast_interhost returned error %d\n",
                 returnValue));
        commPtr->releaseCollectiveSMBuffer(collDesc);
        return returnValue;
    }

    /* wait until all the data has arrived, and then copy it into the
     * bytesPerProc array
     */
    int numHosts = commPtr->localGroup->numberOfHostsInGroup;
    int rootHost = commPtr->localGroup->mapGroupProcIDToHostID[root];
    rootHost = commPtr->localGroup->groupHostDataLookup[rootHost];
    int myHostRank = commPtr->localGroup->hostIndexInGroup;
    int myNodeHostRank = myHostRank - rootHost;
    if (myNodeHostRank < 0)
        myNodeHostRank += group->numberOfHostsInGroup;
    int iAmLocalBCastRoot = 0;
    if (myHostRank == rootHost) {
        if (root == myRank) {
            iAmLocalBCastRoot = 1;
        }
    } else {
        if (myLocalRank == 0)
            iAmLocalBCastRoot = 1;
    }
    if (iAmLocalBCastRoot) {
        collDesc->flag = 1;
        mb();
    }

    ULM_SPIN_AND_MAKE_PROGRESS(collDesc->flag < 1);

    for (int p = 0; p < nProcs; p++) {
        bytesPerProc[p] = bpc[p];
    }

    /* figure out how many data stripes will be used, and other data sending
     *   data parameters
     */
    size_t perRankStripeSize = commPtr->collectiveOpt.perRankStripeSize;
    size_t *dataToSend = commPtr->collectiveOpt.dataToSend;
    size_t *dataToSendNow = commPtr->collectiveOpt.dataToSendNow;

    int numStripes = 0;
    for (int host = 0; host < numHosts; host++) {

        size_t totalBytes = 0;
        for (int p = 0; p < group->groupHostData[host].nGroupProcIDOnHost;
             p++) {
            int proc = group->groupHostData[host].groupProcIDOnHost[p];
            size_t procBytes = bytesPerProc[proc];
            totalBytes += procBytes;
            if (procBytes > 0) {
                int nStripesForThisProc =
                    ((procBytes - 1) / perRankStripeSize) + 1;
                if (nStripesForThisProc > numStripes)
                    numStripes = nStripesForThisProc;
            }
        }

        /* fill in the amount of data that host "host" has to exchange */
        dataToSend[host] = totalBytes;

    }

    /* barrier to make sure all have read bytesPerProc before moving on */
    commPtr->smpBarrier(commPtr->barrierData);

    /* loop over data stripes */
    for (int stripeID = 0; stripeID < numStripes; stripeID++) {

        /* write data to source buffer, if this process contributes to this
         *   stripeID
         */
        if (root == myRank) {
            size_t bytesAlreadyCopied =
                stripeID * commPtr->collectiveOpt.perRankStripeSize;
            size_t offsetIntoSharedBuffer = 0;
            /* loop over remote hosts - root only calls this */
            for (int h = 0; h < group->lenScatterDataNodeList[0]; h++) {
                /* get host index */
                int host = group->scatterDataNodeList[0][h];
                host += rootHost;
                if (host >= group->numberOfHostsInGroup)
                    host -= group->numberOfHostsInGroup;
                /* loop over all procs on the host */
                for (int p = 0;
                     p <
                     commPtr->localGroup->groupHostData[host].
                     nGroupProcIDOnHost; p++) {
                    int proc =
                        commPtr->localGroup->groupHostData[host].
                        groupProcIDOnHost[p];
                    ssize_t bytesToCopy =
                        bytesPerProc[proc] -
                        stripeID *
                        commPtr->collectiveOpt.perRankStripeSize;
                    if (bytesToCopy <= 0)
                        continue;
                    if (bytesToCopy >
                        commPtr->collectiveOpt.perRankStripeSize)
                        bytesToCopy =
                            commPtr->collectiveOpt.perRankStripeSize;
                    /* copy data to shared buffer */
                    void *pSendBuff =
                        (void *) ((char *) sendbuf +
                                  displs[proc] * sendtype->extent);
                    copyDataToSharedBufferMultipleStripes(bytesToCopy,sendcounts[proc],
                                                          bytesAlreadyCopied,
                                                          offsetIntoSharedBuffer,
                                                          pSendBuff,
                                                          sharedBuffer,
                                                          sendtype);
                    offsetIntoSharedBuffer += bytesToCopy;
                }               /* end p loop */
            }                   /* end h loop */
            mb();
            collDesc->flag = 2;
            mb();
        }

        /* root on the root host spins until the data is packed to send */
        if ((myHostRank == rootHost) && (myLocalRank == 0)) {
            ULM_SPIN_AND_MAKE_PROGRESS(collDesc->flag < 2);
        }

        /*
         * Read the shared memory buffer - interhost data exchange
         */
        if (commPtr->localGroup->onHostProcID == 0) {
            for (int host = 0; host < numHosts; host++) {
                dataToSendNow[host] = 0;
                int nLocalRanks =
                    commPtr->localGroup->groupHostData[host].
                    nGroupProcIDOnHost;
                for (int proc = 0; proc < nLocalRanks; proc++) {
                    int groupRank =
                        commPtr->localGroup->groupHostData[host].
                        groupProcIDOnHost[proc];
                    ssize_t bytesToCopyNow =
                        bytesPerProc[groupRank] -
                        stripeID *
                        commPtr->collectiveOpt.perRankStripeSize;
                    if (bytesToCopyNow <= 0)
                        continue;
                    if (bytesToCopyNow >
                        commPtr->collectiveOpt.perRankStripeSize)
                        bytesToCopyNow =
                            commPtr->collectiveOpt.perRankStripeSize;
                    dataToSendNow[host] += bytesToCopyNow;
                }
            }

            /* if local commRoot, first exchange data with other hosts, and
             *   then read in all data
             */
            /* setup initial values for dataToSendNow and recvScratchSpace */

            returnValue = ulm_scatterv_interhost(comm, sharedBuffer,
                                                 &(commPtr->localGroup->
                                                   interhostGatherScatterTree),
                                                 dataToSendNow, root, tag);
            if (returnValue != ULM_SUCCESS) {
                ulm_err(("Error: error returned from ulm_scatterv_interhost %d\n", returnValue));
                commPtr->releaseCollectiveSMBuffer(collDesc);
                return returnValue;
            }

            /* set flag indicating data exchange is done */
            mb();
            collDesc->flag = 3;
            mb();

        }
        /* spin until all data has been received */
        ULM_SPIN_AND_MAKE_PROGRESS(collDesc->flag < 3);

        /* copy scattered data to final destination */
        ssize_t bytesToRead = bytesPerProc[myRank] -
            stripeID * commPtr->collectiveOpt.perRankStripeSize;
        if (bytesToRead > 0) {
            if (bytesToRead > commPtr->collectiveOpt.perRankStripeSize)
                bytesToRead = commPtr->collectiveOpt.perRankStripeSize;
            /* get offset into packed buffer */
            size_t offsetIntoPackedBuffer = 0;
            for (int proc = 0; proc < myLocalRank; proc++) {
                int groupRank =
                    commPtr->localGroup->groupHostData[myHostRank].
                    groupProcIDOnHost[proc];
                ssize_t bts =
                    bytesPerProc[groupRank] -
                    stripeID * commPtr->collectiveOpt.perRankStripeSize;
                if (bts <= 0)
                    continue;
                if (bts > commPtr->collectiveOpt.perRankStripeSize)
                    bts = commPtr->collectiveOpt.perRankStripeSize;
                offsetIntoPackedBuffer += bts;
            }

            size_t bytesAlreadyRead =
                stripeID * commPtr->collectiveOpt.perRankStripeSize;
            void *srcBuffer =
                (void *) ((char *) sharedBuffer + offsetIntoPackedBuffer);
            if (recvtype->layout == CONTIGUOUS) {
                void *dest =
                    (void *) ((char *) recvbuf + bytesAlreadyRead);
                MEMCOPY_FUNC(srcBuffer, dest, bytesToRead);
            } else {
                size_t offset = 0;
                /* find last partially copied field of data structure */
                size_t recv_type_index =
                    bytesAlreadyRead / recvtype->packed_size;
                size_t recv_map_index = 0;
                size_t partialSum =
                    recv_type_index * recvtype->packed_size;
                while (recvtype->type_map[recv_map_index].size +
                       partialSum < bytesAlreadyRead) {
                    partialSum += recvtype->type_map[recv_map_index].size;
                    recv_map_index++;
                }
                size_t recv_map_offset = bytesAlreadyRead - partialSum;
                returnValue =
                    type_pack(TYPE_PACK_UNPACK, srcBuffer, bytesToRead,
                              &offset, recvbuf, recvcount, recvtype,
                              &recv_type_index, &recv_map_index,
                              &recv_map_offset);
                if (returnValue < 0 ) {
                    ulm_err(("Error: ulm_scatterv : error returned from type_pack :: %d\n", returnValue));
                    commPtr->releaseCollectiveSMBuffer(collDesc);
                    return ULM_ERROR;
                }
            }
        }

        /* spin until all data has been received */
        if (numStripes > 1) {
            /* reset flag for next iteration */
            commPtr->smpBarrier(commPtr->barrierData);
            collDesc->flag = 0;
            commPtr->smpBarrier(commPtr->barrierData);
            /* update number of bytes per proc send count */
        }

    }                           /* end stripeID loop */

    /* return collectives descriptor to the communicator */
    commPtr->releaseCollectiveSMBuffer(collDesc);

    return returnValue;
}
