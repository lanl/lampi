/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <stdio.h>

#include "client/ULMClient.h"
#include "internal/buffer.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "mpi/mpi.h"
#include "path/common/InitSendDescriptors.h"
#include "path/common/path.h"
#include "queue/Communicator.h"
#include "queue/globals.h"
#include "queue/globals.h"
#include "ulm/ulm.h"


// Start sending data (if possible), add send descriptor to message
// queues

int Communicator::isend_start(SendDesc_t **pSendDesc)
{
    SendDesc_t *SendDesc = *pSendDesc;
    bool incomplete;
    int rc = ULM_SUCCESS;
    int sendReturn;
    unsigned long seq;

    // make sure that he request object is in the inactive state

    if (SendDesc->status == ULM_STATUS_INVALID) {
        ulm_err(("Error: ULM request structure has invalid status (%d)\n",
                 SendDesc->status));
        return ULM_ERR_REQUEST;
    }

    // increment reference count on datatype for non-persistent requests

    if (!SendDesc->persistent) {
        ulm_type_retain(SendDesc->datatype);
        ulm_type_retain(SendDesc->bsendDatatype);
    }

    // for buffered send copy data from user space into the "buffer"

    if ((SendDesc->sendType == ULM_SEND_BUFFERED) &&
        (SendDesc->persistent)) {
        int offset = 0;
        rc = PMPI_Pack(SendDesc->appBufferPointer,
                       SendDesc->bsendDtypeCount,
                       (MPI_Datatype) SendDesc->bsendDatatype,
                       SendDesc->addr_m,
                       SendDesc->bsendBufferSize, &offset, contextID);
        if (rc != MPI_SUCCESS) {
            return rc;
        }
    }

    // set sent completion to false

    SendDesc->messageDone = REQUEST_INCOMPLETE;

    // set the clear to send flag - depending on send mode

    if (SendDesc->sendType != ULM_SEND_SYNCHRONOUS) {
        SendDesc->clearToSend_m = true;
    } else {
        SendDesc->clearToSend_m = false;
    }

    // for buffered send, send is already complete, since the data
    // resides in the buffered region by this stage; also we need to
    // find our buffer allocation and increment the reference count
    // for this send descriptor

    if (SendDesc->sendType == ULM_SEND_BUFFERED) {
        ULMRequest_t request = (ULMRequest_t *) (SendDesc);

        SendDesc->bsendOffset =
            (ssize_t) ((long) SendDesc->addr_m -
                       (long) lampiState.bsendData->buffer);
        if (!(SendDesc->persistent)) {
            if ((SendDesc->posted_m.length_m > 0) &&
                !ulm_bsend_increment_refcount(request,
                                              SendDesc->bsendOffset)) {
                SendDesc->path_m->ReturnDesc(SendDesc);
                return ULM_ERR_FATAL;
            }
        }
    }

    // set the destination process ID, communicator ID, user tag, and
    // length of the message and other fields not specific to a given
    // path...

    SendDesc->NumAcked = 0;
    SendDesc->NumSent = 0;
    SendDesc->NumFragDescAllocated = 0;
    SendDesc->messageDone =
        ((SendDesc->sendType == ULM_SEND_BUFFERED) && !(SendDesc->persistent))
        ? REQUEST_COMPLETE : REQUEST_INCOMPLETE;
#ifdef ENABLE_RELIABILITY
    SendDesc->earliestTimeToResend = -1.0;
#endif
    SendDesc->freeCalled = 0;

    if (!(SendDesc->posted_m.length_m)) {
        SendDesc->addr_m = 0;
    }

    // path-specific initialization of descriptor for fields like
    // numfrags...

    SendDesc->path_m->init(SendDesc);

    // change status

    SendDesc->status = ULM_STATUS_INCOMPLETE;

    // lock descriptor, so if data gets sent and is acked before we
    // have a chance to post the descriptor on either the
    // IncompletePostedSends or UnackedPostedSends list, we don't try
    // to move it between lists.
    //
    // get sequence number...now that we can't fail for ordinary reasons...

    if (SendDesc->path_m->pathType_m != SHAREDMEM) {
        if (usethreads())
            next_isendSeqsLock[SendDesc->posted_m.peer_m].lock();
        seq = next_isendSeqs[SendDesc->posted_m.peer_m]++;
        if (usethreads())
            next_isendSeqsLock[SendDesc->posted_m.peer_m].unlock();
        // set sequence number
        SendDesc->isendSeq_m = seq;
    }

    if (usethreads())
        SendDesc->Lock.lock();

    // start sending data

    while (!SendDesc->path_m->send(SendDesc, &incomplete, &sendReturn)) {
        if (sendReturn == ULM_ERR_BAD_PATH) {
            if (0) {            // +++++++++++++ REVISIT ++++++++++++
                // unbind from the current path
                SendDesc->path_m->unbind(SendDesc, (int *) 0, 0);
                // select a new path, if at all possible
                sendReturn =
                    (*pt2ptPathSelectionFunction) ((void **) &SendDesc, 0, 0);
                if (sendReturn != ULM_SUCCESS) {
                    if (usethreads())
                        SendDesc->Lock.unlock();
                    SendDesc->path_m->ReturnDesc(SendDesc);
                    return sendReturn;
                }
                // initialize the descriptor for this path
                SendDesc->path_m->init(SendDesc);
            }                   // ------------ END REVISIT -------------
            if (usethreads())
                SendDesc->Lock.unlock();
            return ULM_ERROR;
        } else {

            // unbind should empty SendDesc of frag descriptors...

            SendDesc->path_m->unbind(SendDesc, (int *) 0, 0);
            if (usethreads())
                SendDesc->Lock.unlock();
            SendDesc->path_m->ReturnDesc(SendDesc);
            return sendReturn;
        }
    }

    // push record onto the appropriate list

    if (incomplete) {
        // We're going to have to push this along later, so save it.
        SendDesc->WhichQueue = INCOMPLETEISENDQUEUE;
        IncompletePostedSends.Append(SendDesc);
    } else {
        SendDesc->WhichQueue = UNACKEDISENDQUEUE;
        UnackedPostedSends.Append(SendDesc);
    }

    // unlock descriptor
    if (usethreads()) {
        SendDesc->Lock.unlock();
    }

    return ULM_SUCCESS;
}
