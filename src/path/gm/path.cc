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

#include "queue/globals.h"
#include "internal/options.h"
#include "path/gm/path.h"
#include "os/atomic.h"

inline bool gmPath::canReach(int globalDestProcessID)
{
    // return true only for processes not on our "box"

    int destinationHostID;
    int dev;

    destinationHostID = global_proc_to_host(globalDestProcessID);
    if (myhost() != destinationHostID) {
        for (dev = 0; dev < gmState.nDevsAllocated; dev++) {
            if (gmState.localDevList[dev].remoteDevList[globalDestProcessID].node_id
                != (unsigned int) -1) {
                return true;
            }
        }
    }

    return false;
}

int maxOutstandingGmFrags = 5;

bool gmPath::send(BaseSendDesc_t *message, bool *incomplete, int *errorCode)
{
    Group *group;
    gmSendFragDesc *sfd;
    gmSendFragDesc *afd;
    int globalDestProc;
    int nDescsToAllocate;
    int rc;
    int tmapIndex;
    size_t leftToSend;
    size_t payloadSize;
    size_t offset;

    assert(message);

    *incomplete = true;
    *errorCode = ULM_SUCCESS;

    group = communicators[message->ctx_m]->remoteGroup;
    globalDestProc = group->mapGroupProcIDToGlobalProcID[message->posted_m.proc.destination_m];

    // always allocate and try to send the first frag

    if (message->NumFragDescAllocated == 0) {
	message->clearToSend_m = true;
    }

    // allocate and initialize fragment descriptors

    offset = gmState.bufSize * message->NumFragDescAllocated;
    leftToSend = message->posted_m.length_m - offset;
    nDescsToAllocate = message->numfrags - message->NumFragDescAllocated;

    while (nDescsToAllocate--) {

        // slow down the send -- always initialize and send the first frag
        if (message->NumFragDescAllocated != 0) {
            if ((maxOutstandingGmFrags != -1) &&
                (message->NumFragDescAllocated -
                 message->NumAcked >= (unsigned int) maxOutstandingGmFrags)) {
                message->clearToSend_m = false;
            }

            // Request To Send/Clear To Send - always send the first frag
            if (!message->clearToSend_m) {
                break;
            }
        }

        // how much data left?
        payloadSize = (leftToSend < gmState.bufSize) ? leftToSend : gmState.bufSize;

        // get an unused frag descriptor
        if (usethreads()) {
            sfd = gmState.sendFragList.getElement(0, rc);
        } else {
            sfd = gmState.sendFragList.getElementNoLock(0, rc);
        }
        if (rc != ULM_SUCCESS) {
            // try to make progress here...
            if ((rc == ULM_ERR_OUT_OF_RESOURCE) || (rc == ULM_ERR_FATAL)) {
                *errorCode = rc;
                return false;
            } else {
                // clean-up here...
                break;
            }
        }

        // calculate tmapIndex for non-zero non-contiguous data
        if (message->datatype == NULL || message->datatype->layout == CONTIGUOUS) {
            // an invalid array index is used to indicate that no packedData buffer
            // is needed
            tmapIndex = -1;
        } else {
            int dtype_cnt = offset / message->datatype->packed_size;
            size_t data_copied = dtype_cnt * message->datatype->packed_size;
            ssize_t data_remaining = (ssize_t)(offset - data_copied);
            tmapIndex = message->datatype->num_pairs - 1;
            for (int ti = 0; ti < message->datatype->num_pairs; ti++) {
                if (message->datatype->type_map[ti].seq_offset == data_remaining) {
                    tmapIndex = ti;
                    break;
                } else if (message->datatype->type_map[ti].seq_offset > data_remaining) {
                    tmapIndex = ti - 1;
                    break;
                }
            }
        }

        // initialize descriptor...does almost everything if it can...
        sfd->init(globalDestProc,
                  tmapIndex,
                  message,
                  offset,
                  payloadSize);
        if (0) { // debug
            ulm_warn(("%d sending frag %p to %d: tmapIndex %d offset %ld payloadSize %ld\n",
                      myproc(), sfd, globalDestProc, tmapIndex, (long)offset, (long)payloadSize));
        }

        // put on the FragsToSend list -- may not be able to send immediately
        sfd->WhichQueue = GMFRAGSTOSEND;
        message->FragsToSend.AppendNoLock(sfd);
        (message->NumFragDescAllocated)++;

        // update offset and leftToSend
        leftToSend -= payloadSize;
        offset += payloadSize;
    }


    // send fragments if they are ready; otherwise, try to finish their initialization
    for (sfd = (gmSendFragDesc *) message->FragsToSend.begin();
         sfd != (gmSendFragDesc *) message->FragsToSend.end();
         sfd = (gmSendFragDesc *) sfd->next) {

        bool OKToSend = true;

        if (!sfd->initialized()) {
            OKToSend = sfd->init();
        }

        if (OKToSend) {

            // only send if we have an implicit send token
            if (usethreads()) {
                gmState.localDevList[sfd->dev_m].Lock.lock();
            }
            
            if (gmState.localDevList[sfd->dev_m].sendTokens) {
                gmState.localDevList[sfd->dev_m].sendTokens--;
            }
            else {
                if (usethreads())
                    gmState.localDevList[sfd->dev_m].Lock.unlock();
                continue;
            }

            if (usethreads())
                gmState.localDevList[sfd->dev_m].Lock.unlock();

            void *addr = &(((gmFragBuffer *) sfd->currentSrcAddr_m)->header);

            if (usethreads())
                gmState.gmLock.lock();
            gm_send_with_callback(sfd->gmPort(),
                                  addr,
                                  gmState.log2Size,
                                  sfd->length_m + sizeof(gmHeader),
                                  GM_LOW_PRIORITY,
                                  sfd->gmDestNodeID(),
                                  sfd->gmDestPortID(),
                                  callback,
                                  (void *) sfd);
            if (usethreads())
                gmState.gmLock.unlock();

            if (0) { // debug
                ulm_warn(("%d sent frag %p: gmPort %p addr %p size %d length %ld node_id %d port_id %d\n",
                          myproc(), sfd,  sfd->gmPort(), addr, gmState.log2Size, 
                          (long)(sfd->length_m +sizeof(gmHeader)), sfd->gmDestNodeID(),
                          sfd->gmDestPortID())); 
            }

            // switch frag to ack list and remove from send list
            afd = sfd;
            afd->WhichQueue = GMFRAGSTOACK;
            sfd = (gmSendFragDesc *) message->FragsToSend.RemoveLinkNoLock(afd);
            message->FragsToAck.AppendNoLock(afd);
            (message->NumSent)++;

        }
    }

    if (message->messageDone == REQUEST_INCOMPLETE) {

        if (message->NumSent == message->numfrags &&
            message->sendType != ULM_SEND_SYNCHRONOUS) {

            message->messageDone = REQUEST_COMPLETE;
        }
    }

    if (message->NumSent == message->numfrags) {
        *incomplete = false;
    }

    return true;
}


bool gmPath::receive(double timeNow, int *errorCode, recvType recvTypeArg = ALL)
{
    int i, rc;

    *errorCode = ULM_SUCCESS;

    // check each Myrinet adapter
    for (i = 0; i < gmState.nDevsAllocated; i++) {
        bool keepChecking = true;
        gm_recv_event_t *event;
        gmRecvFragDesc *rf = 0;
        localDevInfo_t *devInfo = &(gmState.localDevList[i]);

        while (keepChecking) {

            // grab receive descriptor
            if (!rf) {
                rf = gmState.recvFragList.getElement(0, rc);
                if (rc != ULM_SUCCESS) {
                    if ((rc == ULM_ERR_OUT_OF_RESOURCE) || (rc == ULM_ERR_FATAL)) {
                        *errorCode = rc;
                        return false;
                    }
                    rf = 0;
                    break;
                }
            }

            if (usethreads())
                gmState.gmLock.lock();
            event = gm_receive(devInfo->gmPort);
            if (usethreads())
                gmState.gmLock.unlock();

            switch (GM_RECV_EVENT_TYPE(event)) {
            case GM_NO_RECV_EVENT:
                keepChecking = false;
                break;
            case GM_RECV_EVENT:
                // increment implicit # of receive tokens
                if (usethreads())
                    fetchNadd((volatile int *) &(devInfo->recvTokens), 1);
                else
                    (devInfo->recvTokens)++;

                rf->gmFragBuffer_m = *(gmFragBuffer **) ((unsigned char *)
                                                         gm_ntohp(event->recv.buffer) - sizeof(gmFragBuffer *));
                rf->gmHeader_m = (gmHeader *) gm_ntohp(event->recv.buffer);
                rf->addr_m = (void *) ((unsigned char *)
                                     gm_ntohp(event->recv.buffer) + sizeof(gmHeader));
                rf->dev_m = i;
                rf->length_m = gm_ntoh_u32(event->recv.length) - sizeof(gmHeader);

                switch (rf->gmHeader_m->common.ctlMsgType) {
                case MESSAGE_DATA:
                    rf->msgData(timeNow);
                    break;
                case MESSAGE_DATA_ACK:
                    rf->msgDataAck();
                    break;
                default:
                    ulm_warn(("gmPath::receive() received message of unknown type %d\n",
                              rf->gmHeader_m->common.ctlMsgType));
                    break;
                }
                rf = 0;
                break;
            default:
                if (0) {        // debug
                    ulm_warn(("%d received event of type %d\n",
                              myproc(), GM_RECV_EVENT_TYPE(event)));
                }
                if (usethreads())
                    gmState.gmLock.lock();
                gm_unknown(devInfo->gmPort, event);
                if (usethreads())
                    gmState.gmLock.unlock();
                break;
            }

        }                       // end while (keepChecking)...

        // free any additional unused gmRecvFragDesc
        if (rf) {
            rf->ReturnDescToPool(0);
            rf = 0;
        }
        // feed GM all the buffers it can take since we
        // won't be back for an indeterminate amount of time
        if (devInfo->recvTokens) {
            if (usethreads())
                devInfo->Lock.lock();

            while (devInfo->recvTokens) {
                // get another buffer
                gmFragBuffer *buf = devInfo->bufList.getElementNoLock(0, rc);
                if (rc != ULM_SUCCESS) {
                    break;
                }
                // give it to GM...
                buf->me = buf;
                void *addr = &(buf->header);
                if (usethreads())
                    gmState.gmLock.lock();
                gm_provide_receive_buffer_with_tag(devInfo->gmPort,
                                                   addr,
                                                   gmState.log2Size,
                                                   GM_LOW_PRIORITY, i);
                if (usethreads())
                    gmState.gmLock.unlock();

                // decrement recvTokens
                (devInfo->recvTokens)--;

            }

            if (usethreads())
                devInfo->Lock.unlock();
        }
    }                           // end for loop

    return true;
}


void gmPath::callback(struct gm_port *port,
                      void *context,
                      enum gm_status status)
{
    gmSendFragDesc *sfd = (gmSendFragDesc *) context;
    BaseSendDesc_t *bsd = (BaseSendDesc_t *) sfd->parentSendDesc_m;

    // reclaim send token
    if (port) {
        if (usethreads()) 
            gmState.localDevList[sfd->dev_m].Lock.lock();

        gmState.localDevList[sfd->dev_m].sendTokens++;

        if (usethreads()) 
            gmState.localDevList[sfd->dev_m].Lock.unlock();
    }

    // fail if there was a failure -- first cut, no fault tolerance!
    if (status != GM_SUCCESS) {
        ulm_exit((-1,
                  "Error: gmPath::callback called with error status (%d)\n",
                  (int) status));
    }

    // bsd could be NULL if an Ack was processed before GM invoked this callback via our call
    // to gm_unknown to complete the send logic.  This can occur when we have multiple GM devices,
    // since we only call gm_unknown() in our receive() logic.
    if ( NULL == bsd )
        return;
    
    if (usethreads())
        bsd->Lock.lock();

    // register frag as acked, if this is not a synchronous message first fragment
    // if it is, we need to wait for a fragment ACK upon matching the message
    //
    // Don't do this if port is NULL -- then we're being called by
    // gmRecvFragDesc::msgDataAck() to free send resources on reception of
    // a data ACK
    if (port) {
        if ((bsd->sendType == ULM_SEND_SYNCHRONOUS) && (sfd->seqOffset_m == 0)) {
            if (usethreads())
                bsd->Lock.unlock();
            return;
        }
    }

    bsd->clearToSend_m = true;
    (bsd->NumAcked)++;

    // remove frag descriptor from list of frags to be acked
    if (sfd->WhichQueue == GMFRAGSTOACK) {
        bsd->FragsToAck.RemoveLinkNoLock((Links_t *) sfd);
    }
    else if (OPT_RELIABILITY && sfd->WhichQueue == GMFRAGSTOSEND) {
        bsd->FragsToSend.RemoveLinkNoLock((Links_t *) sfd);
        // increment NumSent since we were going to send this again...
        (bsd->NumSent)++;
    }
    else {
        ulm_exit((-1,
                  "Error: gmPath::callback: Frag on %d queue\n",
                  sfd->WhichQueue));
    }

    if (OPT_RELIABILITY) {
        // set frag_seq value to 0/null/invalid to detect duplicate ACKs
        sfd->fragSeq_m = 0;
    }

    // reset send descriptor pointer
    sfd->parentSendDesc_m = 0;

    // return all sfd send resources
    sfd->WhichQueue = GMFRAGFREELIST;

    // return fragment buffer to free list
    if (usethreads()) {
        gmState.localDevList[sfd->dev_m].Lock.lock();
        gmState.localDevList[sfd->dev_m].bufList.returnElementNoLock((Links_t *) sfd->currentSrcAddr_m, 0);
        gmState.localDevList[sfd->dev_m].Lock.unlock();
    } else {
        gmState.localDevList[sfd->dev_m].bufList.returnElementNoLock((Links_t *) sfd->currentSrcAddr_m, 0);
    }

    // clear fields that will be initialized in init()
    sfd->currentSrcAddr_m = NULL;
    sfd->initialized_m = false;

    // return frag descriptor to free list
    if (usethreads()) {
        gmState.sendFragList.returnElement(sfd, 0);
    } else {
        gmState.sendFragList.returnElementNoLock(sfd, 0);
    }

    if (usethreads())
        bsd->Lock.unlock();
}
