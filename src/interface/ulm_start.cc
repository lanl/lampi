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

#include "internal/log.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "queue/globals.h"
#include "ulm/ulm.h"

/*
 *  Start processing the request - either send or receive.
 *
 *  \param request      ULM request handle
 *  \return             ULM return code
 */
extern "C" int ulm_start(ULMRequest_t *request)
{
    RequestDesc_t *RequestDesc = (RequestDesc_t *) (*request);
    int comm = RequestDesc->ctx_m;
    Communicator *commPtr = communicators[comm];
    int rc;

    if (RequestDesc->requestType == REQUEST_TYPE_RECV) {

        rc = commPtr->irecv_start(request);

    } else if (RequestDesc->requestType == REQUEST_TYPE_SEND) {

        SendDesc_t *SendDesc = (SendDesc_t *) RequestDesc;

        // for persistent send, may need to reset request
        if (RequestDesc->persistent) {

            // check to see if the SendDesc can be reused, or if we
            // need to allocate a new one. If 0 == SendDesc->numfrag,
            // then the send descriptor was initialized but a send was
            // never started, so we can reuse its resources.

            int reuseDesc = 0;
            if (((SendDesc->numfrags <= SendDesc->NumAcked) &&
                 (SendDesc->messageDone == REQUEST_COMPLETE) &&
                 (ONNOLIST == SendDesc->WhichQueue)) ||
                (ULM_STATUS_INITED == SendDesc->status)) {
                reuseDesc = 1;
            }
            if (!reuseDesc) {

                // allocate new descriptor

                SendDesc_t *oldSendDesc = SendDesc;
                int dest = SendDesc->posted_m.peer_m;

                rc = commPtr->pt2ptPathSelectionFunction((void **) (&SendDesc),
                                                         comm, dest);
                if (rc != ULM_SUCCESS) {
                    return rc;
                }

                // fill in new descriptor information
                oldSendDesc->shallowCopyTo((RequestDesc_t *) SendDesc);

                SendDesc->WhichQueue = REQUESTINUSE;
                SendDesc->status = ULM_STATUS_INITED;

                // reset the old descriptor
                oldSendDesc->persistent = false;

                // wait/test can't be called on the descriptor any
                // more, since we change the handle that is returned
                // to the app

                oldSendDesc->messageDone = REQUEST_RELEASED;

                if (oldSendDesc->sendType == ULM_SEND_BUFFERED) {
                    size_t size;
                    void *buf, *sendBuffer;
                    int count, commctx, rc;
                    ULMBufferRange_t *newAlloc = NULL;
                    ULMType_t *dtype;

                    if (usethreads())
                        lock(&(lampiState.bsendData->Lock));

                    // grab a new alloc block for new descriptor using
                    // same info as current descriptor

                    ulm_bsend_info_get(oldSendDesc, &buf, &size, &count,
                                       &dtype, &commctx);
                    newAlloc = ulm_bsend_alloc(size, 0);

                    if (newAlloc == NULL) {
                        // not enough buffer space - check progress,
                        // clean prev. alloc. and try again
                        if (lampiState.usethreads)
                            unlock(&(lampiState.bsendData->Lock));
                        ulm_make_progress();
                        if (lampiState.usethreads)
                            lock(&(lampiState.bsendData->Lock));
                        ulm_bsend_clean_alloc(0);
                        newAlloc = ulm_bsend_alloc(size, 1);

                        if (newAlloc == NULL) {
                            // unlock buffer pool
                            unlock(&(lampiState.bsendData->Lock));
                            rc = ULM_ERR_BUFFER;
                            return rc;
                        }
                    }
                    ulm_bsend_info_set(SendDesc, buf, size, count, dtype);
                    sendBuffer = (void *) ((unsigned char *) lampiState.bsendData->buffer +
                                           newAlloc->offset);
                    if (dtype && dtype->layout == CONTIGUOUS && dtype->num_pairs != 0) {
                        SendDesc->addr_m = (void *) ((char *) sendBuffer +
                                                      dtype->type_map[0].offset);
                    } else {
                        SendDesc->addr_m = sendBuffer;
                    }
                    SendDesc->bsendOffset = newAlloc->offset;
                    newAlloc->request = SendDesc;

                    if (usethreads())
                        unlock(&(lampiState.bsendData->Lock));
                }

                *request = SendDesc;
            }
        }

        rc = commPtr->isend_start(&SendDesc);

    } else {

        ulm_err(("Error: ulm_start: Unrecognized message request %d\n",
                 RequestDesc->requestType));
        rc = ULM_ERR_REQUEST;
    }

    return rc;
}
