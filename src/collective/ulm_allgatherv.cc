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



#include <assert.h>

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "mem/ULMMallocMacros.h"
#include "util/Utility.h"
#include "util/inline_copy_functions.h"
#include "internal/mpi.h"
#include "internal/type_copy.h"

/*
 * This routine is used to compute the contributions of a given host
 * to the allgatherv data exchange
 *
 *   Input:
 *     commPtr - Communicator Pointer
 *     hostIndex - host index
 *     localProcessRank - rank of process for which data will be evaluated
 *     count - amount of data that each process will send
 *     dataType - contiguous data type
 *     stripeSize - buffer size (in bytes)
 *   OutPut:
 *     firstStripe - A description of localProcessRank's the first byte's
 *       contribution to the shared memory buffer
 *     lastStripe - A description of localProcessRank's the last byte's
 *       contribution to the shared memory buffer
 */

typedef struct stripeContribution {
    size_t offset;
    size_t bytesInStripe;
    int stripe;
} stripeContribution_t;

void hostAlgathervSendData(Communicator *commPtr, int hostIndex,
                           int localProcessRank, int *count,
                           ULMType_t *dataType, size_t stripeSize,
                           stripeContribution *firstStripe,
                           stripeContribution *lastStripe)
{
    /* get cummulative amount of data up to localProcessRank */
    size_t byteOffset = 0;
    for (int lProc = 0; lProc < localProcessRank; lProc++) {
        int rankInCommunicator =
            commPtr->localGroup->groupHostData[hostIndex].
            groupProcIDOnHost[lProc];
        byteOffset += (count[rankInCommunicator] * dataType->packed_size);
    }

    /* find on host starting/ending element index */
    firstStripe->stripe = byteOffset / stripeSize;
    firstStripe->offset = byteOffset - firstStripe->stripe * stripeSize;
    int groupRank =
        commPtr->localGroup->groupHostData[hostIndex].
        groupProcIDOnHost[localProcessRank];
    size_t bytesToScatter = count[groupRank] * dataType->packed_size;
    firstStripe->bytesInStripe = bytesToScatter;
    if (bytesToScatter > (stripeSize - (firstStripe->offset)))
        firstStripe->bytesInStripe = stripeSize - firstStripe->offset;

    lastStripe->stripe = (byteOffset + bytesToScatter - 1) / stripeSize;
    lastStripe->offset =
        byteOffset + bytesToScatter - (lastStripe->stripe) * stripeSize;
    if (firstStripe->stripe == lastStripe->stripe) {
        lastStripe->bytesInStripe = firstStripe->bytesInStripe;
    } else {
        lastStripe->bytesInStripe = lastStripe->offset;
    }

    return;
}


/*
 * This routine is used to copy data into the shared memory buffer for
 * distribution to other hosts
 */
static void copyDataToSharedBuffer(int currentStripe, size_t stripeSize,
                                   stripeContribution *firstStripe,
                                   stripeContribution *lastStripe,
                                   void *sourceBuffer, void *sharedBuffer,
                                   int sendcount, ULMType_t *sendtype)
{
    if (sendtype->layout == CONTIGUOUS) {

        /*
         * contiguous data
         */
        if (currentStripe == firstStripe->stripe) {
            /*
             * first stripe
             */
            void *src = sourceBuffer;
            void *dest =
                (void *) ((char *) sharedBuffer + firstStripe->offset);
            size_t lenToCopy = firstStripe->bytesInStripe;
            /* copy data */
            MEMCOPY_FUNC(src, dest, lenToCopy);

        } else if (currentStripe == lastStripe->stripe) {
            /*
             * last stripe
             */
            size_t offset =
                (lastStripe->stripe -
                 (firstStripe->stripe + 1)) * stripeSize +
                firstStripe->bytesInStripe;
            void *src = (void *) ((char *) sourceBuffer + offset);
            void *dest = sharedBuffer;
            size_t lenToCopy = lastStripe->bytesInStripe;
            /* copy data */
            MEMCOPY_FUNC(src, dest, lenToCopy);

        } else {
            /*
             * full stripes
             */
            size_t offset =
                (currentStripe - (firstStripe->stripe + 1)) * stripeSize +
                firstStripe->bytesInStripe;
            void *src = (void *) ((char *) sourceBuffer + offset);
            void *dest = sharedBuffer;
            size_t lenToCopy = stripeSize;
            /* copy data */
            MEMCOPY_FUNC(src, dest, lenToCopy);

        }

    } else {

        /*
         * non-contiguous data
         */
        /* compute offset into the data to be sent */
        size_t alreadyCopied = 0;
        if (currentStripe != firstStripe->stripe) {
            alreadyCopied += firstStripe->bytesInStripe;
            alreadyCopied +=
                (currentStripe - (firstStripe->stripe + 1)) * stripeSize;
        }

        /* figure out how many bytes to copy */
        size_t bytesToCp;
        if ((currentStripe > firstStripe->stripe)
            && (currentStripe < lastStripe->stripe)) {
            bytesToCp = stripeSize;
        } else if (currentStripe == firstStripe->stripe) {
            bytesToCp = firstStripe->bytesInStripe;
        } else {
            bytesToCp = lastStripe->bytesInStripe;
        }

        /*
         * initialize destination pointer
         */
        void *destination = sharedBuffer;
        if (currentStripe == firstStripe->stripe)
            destination =
                (void *) ((char *) destination + firstStripe->offset);

	/* figure out which data structure is being copied */
	size_t type_index=alreadyCopied/sendtype->packed_size;
	size_t full_copied_data=type_index*sendtype->packed_size;
	if( full_copied_data  != alreadyCopied ) {
		type_index++;
	}
	/* find last partially copied field of data structure */
	size_t map_index = 0;
	size_t partialSum = full_copied_data ;
	while (sendtype->type_map[map_index].size + partialSum < alreadyCopied) {
		partialSum += sendtype->type_map[map_index].size; 
		map_index++;
	}
	/* find out how many bytes have already been copied from the last map element */
	size_t map_offset=alreadyCopied-partialSum;

	size_t offset=0;
	int rv = type_pack(TYPE_PACK_PACK, destination, bytesToCp,
			&offset, sourceBuffer, sendcount, sendtype,
			&type_index, &map_index, &map_offset);
	if (rv < 0) {
		return;
	}

    }

    return;
}


/*
 * This routine is used to copy data into the shared memory buffer for
 * distribution to other hosts
 */
void copyDataToSharedBufferMultipleStripes(size_t bytesToCopy,int sendcount,
                                           size_t bytesAlreadyCopied,
                                           size_t offsetIntoSharedBuffer,
                                           void *sendbuf,
                                           void *sharedBuffer,
                                           ULMType_t *sendtype)
{
    if (sendtype->layout == CONTIGUOUS) {

        /*
         * contiguous data
         */
        void *src = (void *) ((char *) sendbuf + bytesAlreadyCopied);
        void *dest =
            (void *) ((char *) sharedBuffer + offsetIntoSharedBuffer);
        /* copy data */
        MEMCOPY_FUNC(src, dest, bytesToCopy);

    } else {

        /*
         * non-contiguous data
         */

        void *dest =
            (void *) ((char *) sharedBuffer + offsetIntoSharedBuffer);

	/* figure out which data structure is being copied */
	size_t type_index=bytesAlreadyCopied/sendtype->packed_size;
	size_t full_copied_data=type_index*sendtype->packed_size;

	/* find last partially copied field of data structure */
	size_t map_index = 0;
	size_t partialSum = full_copied_data ;
	while (sendtype->type_map[map_index].size + partialSum < bytesAlreadyCopied) {
		partialSum += sendtype->type_map[map_index].size; 
		map_index++;
	}
	/* find out how many bytes have already been copied from the last map element */
	size_t map_offset=bytesAlreadyCopied-partialSum;

	size_t offset=0;
	int rv = type_pack(TYPE_PACK_PACK, dest, bytesToCopy,
			&offset, sendbuf, sendcount, sendtype,
			&type_index, &map_index, &map_offset);
	if (rv < 0) {
		return;
	}

    }

    return;
}

/*
 * This routine is used to exchange between all the local comm-roots
 * of a given communicator.  This routine is based on a binary tree
 * algorithm, with each local comm-root serving as a root of a binary
 * tree.
 */
int exchangeStripeOfData(Communicator *commPtr, void *SharedBuffer,
                         list_t *hostExchangeList, size_t *dataToSendNow,
                         size_t *dataToRecvNow, int numHosts, int tag,
                         int nExtra, int extraOffset)
{
    int returnValue = ULM_SUCCESS;
    int myHostRank = commPtr->localGroup->hostIndexInGroup;
    ULMRequest_t sendRequest, recvRequest;
    ULMStatus_t sendStatus, recvStatus;
    size_t lastSendRecv = 0;
    int destHost, destRank, srcHost, srcRank;


    /* if the number of hosts is not a power of 2, then the "extra"
     *   hosts send their data to a "low-order" host
     */
    if (nExtra > 0) {
        /* compute amount of data to send to the "extra" processes */
        lastSendRecv = 0;
        for (int host = 0; host < numHosts; host++) {
            lastSendRecv += dataToSendNow[host];
        }
        if (myHostRank < nExtra) {
            /* destination */
            srcHost = myHostRank + extraOffset;
            srcRank =
                commPtr->localGroup->groupHostData[srcHost].
                groupProcIDOnHost[0];
            size_t recvLength = dataToSendNow[srcHost];
            void *destBuffer =
                (void *) ((char *) SharedBuffer +
                          dataToSendNow[myHostRank]);
            if (recvLength > 0) {
                returnValue =
                    ulm_irecv(destBuffer, recvLength,
                              (ULMType_t *) MPI_BYTE, srcRank, tag,
                              commPtr->contextID, &recvRequest);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
                returnValue = ulm_wait(&recvRequest, &recvStatus);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
            }
        } else if (myHostRank >= extraOffset) {
            /* source */
            destHost = myHostRank - extraOffset;
            destRank =
                commPtr->localGroup->groupHostData[destHost].
                groupProcIDOnHost[0];
            size_t sendLength = dataToSendNow[myHostRank];
            if (sendLength > 0) {
                returnValue =
                    ulm_isend(SharedBuffer, sendLength,
                              (ULMType_t *) MPI_BYTE, destRank, tag,
                              commPtr->contextID, &sendRequest,
                              ULM_SEND_STANDARD);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
                returnValue = ulm_wait(&sendRequest, &sendStatus);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
            }
        }
        /* update dataToSendNow to reflect the extra data that the
         *   "lower" process will be sending around
         */
        for (int host = 0; host < nExtra; host++) {
            dataToSendNow[host] += dataToSendNow[host + extraOffset];
        }
    }

    /* the processes that are part of the largest power of 2 set exchange
     *   their data
     */
    if (myHostRank < extraOffset) {

        for (int level = 0; level < hostExchangeList[myHostRank].length;
             level++) {

            /* post send message out of the shared buffer */
            destHost = hostExchangeList[myHostRank].list[level];
            destRank =
                commPtr->localGroup->groupHostData[destHost].
                groupProcIDOnHost[0];
            size_t sendLength = dataToSendNow[myHostRank];

            if (sendLength > 0) {
                returnValue =
                    ulm_isend(SharedBuffer, sendLength,
                              (ULMType_t *) MPI_BYTE, destRank, (int) tag,
                              commPtr->contextID, &sendRequest,
                              ULM_SEND_STANDARD);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
            }

            /* post recv message to the shared buffer - right after the
             *   current data being sent, so that the data we need to send
             *   next iteration is contiguous
             */

            srcHost = destHost;
            srcRank = destRank;
            size_t recvLength = dataToSendNow[srcHost];
            void *destBuffer =
                (void *) ((char *) SharedBuffer +
                          dataToSendNow[myHostRank]);
            if (recvLength > 0) {
                returnValue =
                    ulm_irecv(destBuffer, recvLength,
                              (ULMType_t *) MPI_BYTE, srcRank, (int) tag,
                              commPtr->contextID, &recvRequest);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
            }

            /* wait on send and recv to complete */
            if (recvLength > 0) {
                returnValue = ulm_wait(&recvRequest, &recvStatus);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
            }
            if (sendLength > 0) {
                returnValue = ulm_wait(&sendRequest, &sendStatus);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
            }

            /* update dataToSendNow */
            /* create temp copy of current send array */
            for (int host = 0; host < extraOffset; host++) {
                dataToRecvNow[host] = dataToSendNow[host];
            }
            /* update send array to reflect new data that has arrived */
            for (int host = 0; host < extraOffset; host++) {
                srcHost = hostExchangeList[host].list[level];
                dataToSendNow[host] += dataToRecvNow[srcHost];
            }
        }
    }

    /* if the number of hosts is not a power of 2, then the "extra"
     *   hosts get the resulting data from a "low order" host
     */
    if (nExtra > 0) {
        if (myHostRank >= extraOffset) {
            /* destination */
            srcHost = myHostRank - extraOffset;
            srcRank =
                commPtr->localGroup->groupHostData[srcHost].
                groupProcIDOnHost[0];
            size_t recvLength = lastSendRecv;
            /* we write over the shared buffer */
            void *destBuffer = SharedBuffer;
            if (recvLength > 0) {
                returnValue =
                    ulm_irecv(destBuffer, recvLength,
                              (ULMType_t *) MPI_BYTE, srcRank, tag,
                              commPtr->contextID, &recvRequest);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
                returnValue = ulm_wait(&recvRequest, &recvStatus);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
            }
        } else if (myHostRank < nExtra) {
            /* source */
            destHost = myHostRank + extraOffset;
            destRank =
                commPtr->localGroup->groupHostData[destHost].
                groupProcIDOnHost[0];
            size_t sendLength = lastSendRecv;
            if (sendLength > 0) {
                returnValue =
                    ulm_isend(SharedBuffer, sendLength,
                              (ULMType_t *) MPI_BYTE, destRank, tag,
                              commPtr->contextID, &sendRequest,
                              ULM_SEND_STANDARD);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
                returnValue = ulm_wait(&sendRequest, &sendStatus);
                if (returnValue != ULM_SUCCESS)
                    return returnValue;
            }
        }
    }

    return returnValue;
}

/*
 * This routine is used to copy data from the shared memory buffer to
 * local buffer.
 */
static void copyDataFromSharedBuffer(Communicator *commPtr,
                                     void *destinationBuffer,
                                     void *sharedBuffer,
                                     int *dataHostOrder, int *recvcount,
                                     int *displs, int numHosts,
                                     size_t *dataToSendThisStripe,
                                     int *currentLocalRank,
                                     size_t *currentRankBytesRead,
                                     ULMType_t *recvtype)
{
    /* set source buffer pointer */
    void *sourcePointer = sharedBuffer;

    if (recvtype->layout == CONTIGUOUS) {

        /* loop over hosts */
        for (int hostIdx = 0; hostIdx < numHosts; hostIdx++) {
            int host = dataHostOrder[hostIdx];

            /* figure out how much data will be read in */
            size_t bytesToRead = dataToSendThisStripe[host];

            /* loop until all data has been read */
            while (bytesToRead) {

                /* setup offsets into recv buffers */
                int localRank = currentLocalRank[host];
                int sourceRank =
                    commPtr->localGroup->groupHostData[host].
                    groupProcIDOnHost[localRank];
                size_t offsetIntoRankBuffer = currentRankBytesRead[host];

                /* calculate number of bytes to read in */
                size_t toRead = bytesToRead;
                size_t bytesLeftForFromCurrentRank =
                    recvcount[sourceRank] * recvtype->packed_size -
                    currentRankBytesRead[host];
                if (toRead > bytesLeftForFromCurrentRank) {
                    toRead = bytesLeftForFromCurrentRank;
                    /* reset information for next read */
                    currentLocalRank[host]++;
                    currentRankBytesRead[host] = 0;
                    if (currentLocalRank[host] ==
                        commPtr->localGroup->groupHostData[host].
                        nGroupProcIDOnHost)
                        currentLocalRank[host] = 0;

                } else {
                    /* reset information for next read */
                    currentRankBytesRead[host] += toRead;
                }
                if (toRead == 0)
                    continue;

                /* caluclate destination buffer pointer */
                void *destination = (void *) ((char *) destinationBuffer +
                                              displs[sourceRank] *
                                              recvtype->extent +
                                              offsetIntoRankBuffer);

                /* copy data to destination */
                MEMCOPY_FUNC(sourcePointer, destination, toRead);

                /* reset calculate source buffer pointer */
                sourcePointer = (void *) ((char *) sourcePointer + toRead);

                /* reset bytesToRead */
                bytesToRead -= toRead;

            }
        }                       /* end host list */

    } else {

        /*
         * non-contiguous data
         */

        /* loop over hosts */
        for (int hostIdx = 0; hostIdx < numHosts; hostIdx++) {
            int host = dataHostOrder[hostIdx];

            /* figure out how much data will be read in */
            size_t bytesToRead = dataToSendThisStripe[host];

            /* loop until all data has been read */
            while (bytesToRead) {

                /* setup offsets into recv buffers */
                int localRank = currentLocalRank[host];
                int sourceRank =
                    commPtr->localGroup->groupHostData[host].
                    groupProcIDOnHost[localRank];
                size_t offsetIntoRankBuffer = currentRankBytesRead[host];
                size_t bytesLeftForFromCurrentRank =
                    recvcount[sourceRank] * recvtype->packed_size -
                    currentRankBytesRead[host];

                /* calculate number of bytes to read in */
                size_t toRead;
                if (bytesLeftForFromCurrentRank > bytesToRead) {
                    /* all the rest of the data for this process */
                    toRead = bytesToRead;
                    currentRankBytesRead[host] += toRead;
                } else {
                    /* read the rest froms this sprocess - more data */
                    toRead = bytesLeftForFromCurrentRank;
                    currentLocalRank[host]++;
                    currentRankBytesRead[host] = 0;
                }

                if (toRead == 0)
                    continue;

                /* copy data to destination */
                /* base offset for data from the current sourceRank */
                void *dest = (void *) ((char *) destinationBuffer +
                                       displs[sourceRank] *
                                       recvtype->extent);

                ssize_t bytesCopied;
                contiguous_to_noncontiguous_copy(recvtype, dest,
                                                 &bytesCopied,
                                                 offsetIntoRankBuffer,
                                                 toRead, sourcePointer);

                sourcePointer =
                    (void *) ((char *) sourcePointer + bytesCopied);

                /* decrement number of bytes left to read */
                bytesToRead -= toRead;

            }                   /* end while (bytesToRead ) */
        }                       /* end host loop */

    }

    return;
}


/*
 * This routine is used to copy data from the shared memory buffer to
 * local buffer.
 */
static void copyDataFromSharedBufferMultipleStripes(Communicator *commPtr,
                                                    void *destinationBuffer,
                                                    void *sharedBuffer,
                                                    int *dataHostOrder,
                                                    int *recvcount,
                                                    int *displs,
                                                    int numHosts,
                                                    size_t *
                                                    dataToSendThisStripe,
                                                    int *currentLocalRank,
                                                    size_t *
                                                    currentRankBytesRead,
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
                    recvcount[groupRank] * recvtype->packed_size -
                    rankOffset;
                if (bytesToCopyNow >
                    commPtr->collectiveOpt.perRankStripeSize)
                    bytesToCopyNow =
                        commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow > 0) {

                    /* caluclate destination buffer pointer */

                    void *destination =
                        (void *) ((char *) destinationBuffer +
                                  displs[groupRank] * recvtype->extent +
                                  rankOffset);

                    /* copy data to destination */
                    MEMCOPY_FUNC(sourcePointer, destination,
                                 bytesToCopyNow);

                    /* reset calculate source buffer pointer */
                    sourcePointer =
                        (void *) ((char *) sourcePointer + bytesToCopyNow);

                }
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
                    recvcount[groupRank] * recvtype->packed_size -
                    rankOffset;
                if (bytesToCopyNow >
                    commPtr->collectiveOpt.perRankStripeSize)
                    bytesToCopyNow =
                        commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow > 0) {


                    /* copy data to destination */
                    /* base offset for data from the current sourceRank */
                    void *dest = (void *) ((char *) destinationBuffer +
                                           displs[groupRank] *
                                           recvtype->extent);

                    ssize_t bytesCopied;
                    contiguous_to_noncontiguous_copy(recvtype, dest,
                                                     &bytesCopied,
                                                     rankOffset,
                                                     bytesToCopyNow,
                                                     sourcePointer);

                    sourcePointer =
                        (void *) ((char *) sourcePointer + bytesCopied);

                }
            }
        }                       /* end host list */
    }

    return;
}


/*
 * this version of the allgatherv is intended for the case in which
 * the data all fits into 1 stripe of the shared memory buffer.  It
 * this case there is no need to pay the overhead penatly of the
 * parallel write to the shared memroy buffer, since this happens
 * "automatically".
 */
int ulm_allgatherv1Stripe(void *sendbuf, int sendcount,
                          ULMType_t *sendtype, void *recvbuf,
                          int *recvcount, int *displs, ULMType_t *recvtype,
                          size_t *dataToSend, Communicator *commPtr)
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
    int myHostRank = commPtr->localGroup->hostIndexInGroup;
    size_t *dataToSendThisStripe =
        commPtr->collectiveOpt.dataToSendThisStripe;
    int *currentLocalRank = commPtr->collectiveOpt.currentLocalRank;
    size_t *currentRankBytesRead =
        commPtr->collectiveOpt.currentRankBytesRead;
    size_t *dataToSendNow = commPtr->collectiveOpt.dataToSendNow;
    size_t *recvScratchSpace = commPtr->collectiveOpt.recvScratchSpace;
    size_t localStripeSize = commPtr->collectiveOpt.localStripeSize;
    list_t *hostExchangeList = commPtr->collectiveOpt.hostExchangeList;
    int nExtra = commPtr->collectiveOpt.nExtra;
    int extraOffset = commPtr->collectiveOpt.extraOffset;
    int *dataHostOrder = commPtr->collectiveOpt.dataHostOrder;

    /* get tag - single tag is sufficient, since send/recv/tag is a unique
     *   triplet for all sends
     */
    long long tag = commPtr->get_base_tag(1);

    /* get collectives descriptor from the communicator */
    CollectiveSMBuffer_t *collDesc = commPtr->getCollectiveSMBuffer(tag);

    /* shared buffer */
    void *RESTRICT_MACRO sharedBuffer = collDesc->mem;

    /* figure out to which stripes this process contributes */
    stripeContribution_t firstStripe, lastStripe;
    int rank = commPtr->localGroup->onHostProcID;
    hostAlgathervSendData(commPtr, myHostRank, rank, recvcount,
                          recvtype, localStripeSize, &firstStripe,
                          &lastStripe);

    /*
     * Fill in shared memory buffer
     */
    int stripeID = 0;
    copyDataToSharedBuffer(stripeID, localStripeSize,
                           &firstStripe, &lastStripe, sendbuf,
                           sharedBuffer, sendcount, sendtype);

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
        /* setup initial values for dataToSendNow and recvScratchSpace */
        for (int host = 0; host < numHosts; host++) {
            recvScratchSpace[host] = 0;
        }

        returnCode =
            exchangeStripeOfData(commPtr, sharedBuffer, hostExchangeList,
                                 dataToSendNow, recvScratchSpace, numHosts,
                                 tag, nExtra, extraOffset);
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
    copyDataFromSharedBuffer(commPtr, recvbuf, sharedBuffer, dataHostOrder,
                             recvcount, displs, numHosts,
                             dataToSendThisStripe, currentLocalRank,
                             currentRankBytesRead, recvtype);

    /* return collectives descriptor to the communicator */
    commPtr->releaseCollectiveSMBuffer(collDesc);

    return returnCode;
}


/*
 * this version of allgatherv is used when more that 1 stripe of data
 * is needed to send all the data.  It is designed to make sure all
 * processes contribute data to each stripe, and better parallelizes
 * the "send side" of the alrogithm.
 */
int ulm_allgathervMultipleStripes(void *sendbuf, int sendcount,
                                  ULMType_t *sendtype,
                                  void *recvbuf, int *recvcount,
                                  int *displs, ULMType_t *recvtype,
                                  Communicator *commPtr, int numStripes)
{
    int returnCode = ULM_SUCCESS;

    /*  
     * while loop over message stripes
     *
     * At each stage a shared memory buffer is filled in by the
     * processes on this host.  This data is sent to other hosts, and
     * read by the local processes.
     *
     * The algorithm used to exchange data between inolves
     * partitioning the hosts into two groups, one the largest number
     * of host ranks that is a power of 2 (called the exchange group
     * of size exchangeSize) and the remainder.
     *
     * The data exchange pattern is:
     *
     * - the remainder group hosts send their data to
     *   myHostRank-exchangeSize
     *
     * - the exchange group hosts exchange data between sets of these
     *   hosts.  The size of the first set is 2, then 4, then 8,
     *   etc. with each host in the lower half of the set exchanging
     *   data with one in the upper half of the set.
     *
     * - the remainder group hosts get their data to
     *   myHostRank-exchangeSize
     */

    int numHosts = commPtr->localGroup->numberOfHostsInGroup;
    int myHostRank = commPtr->localGroup->hostIndexInGroup;
    int myCommRank = commPtr->localGroup->ProcID;
    int myLocalRank = commPtr->localGroup->onHostProcID;
    size_t *dataToSendThisStripe =
        commPtr->collectiveOpt.dataToSendThisStripe;
    int *currentLocalRank = commPtr->collectiveOpt.currentLocalRank;
    size_t *currentRankBytesRead =
        commPtr->collectiveOpt.currentRankBytesRead;
    size_t *dataToSendNow = commPtr->collectiveOpt.dataToSendNow;
    size_t *recvScratchSpace = commPtr->collectiveOpt.recvScratchSpace;
    list_t *hostExchangeList = commPtr->collectiveOpt.hostExchangeList;
    int nExtra = commPtr->collectiveOpt.nExtra;
    int extraOffset = commPtr->collectiveOpt.extraOffset;
    int *dataHostOrder = commPtr->collectiveOpt.dataHostOrder;

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
        ssize_t leftToCopy = recvcount[myCommRank] * recvtype->packed_size -
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
                int groupRank =
                    commPtr->localGroup->groupHostData[myHostRank].
                    groupProcIDOnHost[proc];
                ssize_t bytesToCopyNow =
                    recvcount[groupRank] * recvtype->packed_size -
                    stripeID * commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow >
                    commPtr->collectiveOpt.perRankStripeSize)
                    bytesToCopyNow =
                        commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow > 0)
                    offsetIntoSharedBuffer += bytesToCopyNow;
            }

            copyDataToSharedBufferMultipleStripes(bytesToCopy,sendcount,
                                                  bytesAlreadyCopied,
                                                  offsetIntoSharedBuffer,
                                                  sendbuf, sharedBuffer,
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
                int groupRank =
                    commPtr->localGroup->groupHostData[host].
                    groupProcIDOnHost[proc];
                ssize_t bytesToCopyNow =
                    recvcount[groupRank] * recvtype->packed_size -
                    stripeID * commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow >
                    commPtr->collectiveOpt.perRankStripeSize)
                    bytesToCopyNow =
                        commPtr->collectiveOpt.perRankStripeSize;
                if (bytesToCopyNow > 0)
                    dataToSendNow[host] += bytesToCopyNow;
            }
        }

        if (commPtr->localGroup->onHostProcID == 0) {

            /* if local commRoot, first exchange data with other hosts, and
             *   then read in all data
             */
            /* setup initial values for dataToSendNow and recvScratchSpace */
            for (int host = 0; host < numHosts; host++) {
                recvScratchSpace[host] = 0;
            }

            returnCode =
                exchangeStripeOfData(commPtr, sharedBuffer,
                                     hostExchangeList, dataToSendNow,
                                     recvScratchSpace, numHosts, tag,
                                     nExtra, extraOffset);
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
        copyDataFromSharedBufferMultipleStripes(commPtr, recvbuf,
                                                sharedBuffer,
                                                dataHostOrder, recvcount,
                                                displs, numHosts,
                                                dataToSendThisStripe,
                                                currentLocalRank,
                                                currentRankBytesRead,
                                                recvtype, stripeID);

        /* spin until all data has been received */
        if (numStripes > 1)
            commPtr->smpBarrier(commPtr->barrierData);

    }                           /* end stripeID loop */

    /* return collectives descriptor to the communicator */
    commPtr->releaseCollectiveSMBuffer(collDesc);

    return returnCode;
}


extern "C" int ulm_allgatherv(void *sendbuf, int sendcount,
                              ULMType_t *sendtype, void *recvbuf,
                              int *recvcount, int *displs,
                              ULMType_t *recvtype, int comm)
{
    int returnCode = ULM_SUCCESS;
    /* get communicator pointer */
    Communicator *commPtr = (Communicator *) communicators[comm];
    Group *group = commPtr->localGroup;

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
    int *currentLocalRank = commPtr->collectiveOpt.currentLocalRank;
    size_t *currentRankBytesRead = commPtr->collectiveOpt.currentRankBytesRead;
    size_t perRankStripeSize = commPtr->collectiveOpt.perRankStripeSize;
    size_t *dataToSend = commPtr->collectiveOpt.dataToSend;

    int numStripes = 0;
    for (int host = 0; host < numHosts; host++) {

        size_t totalBytes = 0;
        for (int p = 0; p < group->groupHostData[host].nGroupProcIDOnHost;
             p++) {
            int proc = group->groupHostData[host].groupProcIDOnHost[p];
            size_t procBytes = recvcount[proc] * recvtype->packed_size;
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

        /* initialize some data */
        currentLocalRank[host] = 0;
        currentRankBytesRead[host] = 0;
    }

    /* decide which algorithm to use */
    if (numStripes > 1) {
        returnCode =
            ulm_allgathervMultipleStripes(sendbuf, sendcount, sendtype,
                                          recvbuf, recvcount, displs,
                                          recvtype, commPtr, numStripes);
    } else {
        returnCode =
            ulm_allgatherv1Stripe(sendbuf, sendcount, sendtype, recvbuf,
                                  recvcount, displs, recvtype, dataToSend,
                                  commPtr);
    }

    return returnCode;
}
