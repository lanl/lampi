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

#include "internal/mpi.h"
#include "queue/Communicator.h"
#include "queue/globals.h"
#include "util/Utility.h"
#include "mem/ULMMallocMacros.h"
#include "collective/coll_fns.h"
#include "util/inline_copy_functions.h"
#include "internal/type_copy.h"

/*
 * This routine is used bye the root of a scatter operation to pack the
 * data to be scattered
 */
int packScatterData(Communicator *commPtr, void *typebuf, size_t sendcount,
                    ULMType_t *type, void *packedBuf,
                    size_t bytesPerProcToCopy, size_t bytesPerProcCopied,
                    size_t *type_index, size_t *map_index,
                    size_t *map_offset, int rootHost)
{
    int rv;
    int proc;
    size_t typeIndex, mapIndex, mapOffset;
    size_t typeIndex_sav = *type_index, mapIndex_sav =
        *map_index, mapOffset_sav = *map_offset;
    Group *group = commPtr->localGroup;

    size_t bufOffset = 0;

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
             commPtr->localGroup->groupHostData[host].nGroupProcIDOnHost;
             p++) {
            proc =
                commPtr->localGroup->groupHostData[host].
                groupProcIDOnHost[p];
            size_t procOffest = proc * sendcount * type->extent;
            void *dest = (void *) ((char *) packedBuf + bufOffset);
            if (type->layout == CONTIGUOUS) {
                void *src =
                    (void *) ((char *) typebuf + procOffest +
                              bytesPerProcCopied);
                MEMCOPY_FUNC(src, dest, bytesPerProcToCopy);
            } else {
                size_t offset = 0;
                typeIndex = typeIndex_sav + proc * sendcount;
                mapIndex = mapIndex_sav;
                int sendCount = sendcount + proc * sendcount;
                mapOffset = mapOffset_sav;
                rv = type_pack(TYPE_PACK_PACK, dest, bytesPerProcToCopy,
                              &offset, typebuf, sendCount, type,
                              &typeIndex, &mapIndex, &mapOffset);
                if (rv < 0) {
                    return ULM_ERROR;
                }

                /* update for next stripe */
                if ((h == 0) && (p == 0)) {
		   /* proc * sendcount accounts for the offset within the process list, so
		    *    that the offset is a proc relative offset */
                    *type_index = typeIndex-proc * sendcount;
                    *map_index = mapIndex;
                    *map_offset = mapOffset;
                }
            }
            /* update offset into packed buffer */
            bufOffset += bytesPerProcToCopy;
        }                       /* end p loop */
    }                           /* end h loop */

    return ULM_SUCCESS;
}

extern "C" int ulm_scatter(void *sendbuf, int sendcount,
                           ULMType_t *sendtype, void *recvbuf,
                           int recvcount, ULMType_t *recvtype, int root,
                           int comm)
{
    int rc = ULM_SUCCESS;
    Communicator *commPtr = (Communicator *) communicators[comm];
    Group *group = commPtr->localGroup;
    int myRank = commPtr->localGroup->ProcID;
    int myLocalRank = commPtr->localGroup->onHostProcID;
    int rootHost = commPtr->localGroup->mapGroupProcIDToHostID[root];
    rootHost = commPtr->localGroup->groupHostDataLookup[rootHost];
    int myHostRank = commPtr->localGroup->hostIndexInGroup;
    int myNodeHostRank = myHostRank - rootHost;
    if (myNodeHostRank < 0)
        myNodeHostRank += group->numberOfHostsInGroup;
    size_t send_type_index = 0, send_map_index = 0, send_map_offset = 0;
    size_t recv_type_index = 0, recv_map_index = 0, recv_map_offset = 0;

    size_t maxBytesPerProc;
    int numStripes;
    if (recvcount > 0) {
        maxBytesPerProc = recvcount * recvtype->packed_size;
        numStripes =
            ((maxBytesPerProc -
              1) / commPtr->collectiveOpt.perRankStripeSize) + 1;
    } else {
        maxBytesPerProc = 0;
        numStripes = 1;
    }

    /* get tag - single tag is sufficient, since send/recv/tag is a unique
     *   triplet for all sends
     */
    long long tag = commPtr->get_base_tag(1);

    /* get collectives descriptor from the communicator */
    CollectiveSMBuffer_t *collDesc =
        commPtr->getCollectiveSMBuffer(tag, ULM_COLLECTIVE_SCATTER);

    /* shared buffer */
    void *RESTRICT_MACRO sharedBuffer = collDesc->mem;
    size_t bytesPerProcToSend = sendcount * sendtype->packed_size;
    size_t bytesPerProcSent = 0;

    /* loop over data stripes */
    for (int stripe = 0; stripe < numStripes; stripe++) {

        ssize_t sendNow = bytesPerProcToSend;
        if (sendNow > commPtr->collectiveOpt.perRankStripeSize)
            sendNow = commPtr->collectiveOpt.perRankStripeSize;
        /* pack data to be sent */
        if (myRank == root) {
            rc = packScatterData(commPtr, sendbuf, (size_t) sendcount,
                                sendtype, sharedBuffer, sendNow,
                                bytesPerProcSent, &send_type_index,
                                &send_map_index, &send_map_offset,
                                rootHost);
            if (rc != ULM_SUCCESS) {
                goto AT_ERR;
            }

            /* set flag indicating data has been copied into the send buffer */
            mb();
            collDesc->flag = 1;
        }

        /* send data */
        if (myLocalRank == 0) {
            /* comm root on rootHost spins until root is done writting the data */
            if (myHostRank == rootHost) {
                ULM_SPIN_AND_MAKE_PROGRESS(collDesc->flag < 1);
            }

            /* interhost scatter */
            rc = ulm_scatter_interhost(comm, sharedBuffer,
                    &(commPtr->localGroup->
                        interhostGatherScatterTree),
                    sendNow, root, tag);
            if (rc != ULM_SUCCESS) {
                goto AT_ERR;
            }

            /* set flag indicating off-host data has been sent/recved */
            mb();
            collDesc->flag = 2;
        }

        /* spin until data is ready to copy */
        ULM_SPIN_AND_MAKE_PROGRESS(collDesc->flag < 2);

        /* copy scattered data to final destination */
        void *srcBuffer =
            (void *) ((char *) sharedBuffer + myLocalRank * sendNow);
        if (recvtype->layout == CONTIGUOUS) {
            void *dest = (void *) ((char *) recvbuf + bytesPerProcSent);
            MEMCOPY_FUNC(srcBuffer, dest, sendNow);
        } else {
            size_t offset = 0;
            rc = type_pack(TYPE_PACK_UNPACK, srcBuffer, sendNow, &offset,
                    recvbuf, recvcount, recvtype, &recv_type_index,
                    &recv_map_index, &recv_map_offset);
            if (rc < 0) {
                goto AT_ERR;
            }
        }

        /* reset control data for next stripe */
        if (numStripes > 1) {
            /* reset flag for next iteration */
            commPtr->smpBarrier(commPtr->barrierData);
            collDesc->flag = 0;
            commPtr->smpBarrier(commPtr->barrierData);
            /* update number of bytes per proc send count */
            bytesPerProcSent += sendNow;
            bytesPerProcToSend -= sendNow;
        }

    }                           /* end stripe loop */

AT_ERR:
    /* return collectives descriptor to the communicator */
    commPtr->releaseCollectiveSMBuffer(collDesc);

    /* set MPI return type */
    rc = (rc == ULM_SUCCESS) ? MPI_SUCCESS : _mpi_error(rc);
    if (rc != MPI_SUCCESS) {
        _mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    return rc;
}
