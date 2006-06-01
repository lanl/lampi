/*
 * Copyright 2002-2004. The Regents of the University of
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "internal/log.h"
#include "path/gm/recvFrag.h"
#include "util/dclock.h"


bool gmRecvFragDesc::AckData(double timeNow)
{
    gmHeaderDataAck *p;
    gmFragBuffer *buf;
    int rc;

    // If gmState.doAck is false, we still send and an ACK if the
    // message is a synchronous send, and this is the first fragment

    if (!gmState.doAck) {
        switch (msgType_m) {
        case MSGTYPE_PT2PT_SYNC:
            if (seqOffset_m != 0) {
                return true;
            }
            break;
        default:
            return true;
            break;
        }
    }

    if (timeNow == -1.0) {
        timeNow = dclock();
    }
    // grab a fragment buffer for the ACK message

    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.lock();
    }
    buf = gmState.localDevList[dev_m].bufList.getElementNoLock(0, rc);
    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.unlock();
    }
    if (rc != ULM_SUCCESS) {
        return false;
    }

    p = (gmHeaderDataAck *) & (buf->header.dataAck);
    p->thisFragSeq = seq_m;

    if (ENABLE_RELIABILITY) {

        Communicator *pg = communicators[ctx_m];
        unsigned int glSourceProcess = pg->remoteGroup->
            mapGroupProcIDToGlobalProcID[srcProcID_m];
        /* process the deliverd sequence number range */
        int returnValue =
            processRecvDataSeqs(p, glSourceProcess, reliabilityInfo);
        if (returnValue != ULM_SUCCESS) {
            if (usethreads()) {
                gmState.localDevList[dev_m].Lock.lock();
                gmState.localDevList[dev_m].bufList.
                    returnElementNoLock((Links_t *) buf, 0);
                gmState.localDevList[dev_m].Lock.unlock();
            } else {
                gmState.localDevList[dev_m].bufList.
                    returnElementNoLock((Links_t *) buf, 0);
            }
            return false;
        }
    }

    if (p->ackStatus == ACKSTATUS_DATACORRUPT) {
        ulm_warn(("Warning: Sending \"DATA CORRUPT\" acknowledgment [rank %d <-- rank %d (%s)]\n",
                  gmHeader_m->data.senderID, myproc(), mynodename()));
    }

    // fill in other fields of header

    p->type = MESSAGE_DATA_ACK;
    p->ctxAndMsgType = gmHeader_m->data.ctxAndMsgType;
    p->src_proc = myproc();
    p->dest_proc = gmHeader_m->data.senderID;
    p->ptrToSendDesc = gmHeader_m->data.sendFragDescPtr;
    p->checksum = 0;
    p->isendSeq_m = isendSeq_m;

    if (ENABLE_RELIABILITY && gmState.doChecksum) {
        p->checksum =
            BasePath_t::headerChecksum((gmHeader *) p,
                                       sizeof(gmHeader) -
                                       sizeof(ulm_uint32_t), GM_HDR_WORDS);
    }

    // only send if we have an implicit send token
    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.lock();
    }
    if (gmState.localDevList[dev_m].sendTokens) {
        gmState.localDevList[dev_m].sendTokens--;
    } else {
        gmState.localDevList[dev_m].bufList.
            returnElementNoLock((Links_t *) buf, 0);
        if (usethreads())
            gmState.localDevList[dev_m].Lock.unlock();
        return false;
    }
    if (usethreads()) {
        gmState.localDevList[dev_m].Lock.unlock();
    }
    // send the ACK
    gm_send_with_callback(gmState.localDevList[dev_m].gmPort,
                          p,
                          gmState.log2Size,
                          sizeof(gmHeader),
                          GM_LOW_PRIORITY,
                          gmState.localDevList[dev_m].
                          remoteDevList[p->dest_proc].node_id,
                          gmState.localDevList[dev_m].
                          remoteDevList[p->dest_proc].port_id,
                          ackCallback,
                          (void *) buf);

    return true;
}


// GM call back function to free resources for data ACKs

static void gmPath_ack_timed_out_resume(struct gm_port *port,
                                        void *context,
                                        gm_status_t status)
{
    int dev;
    gmFragBuffer *buf = (gmFragBuffer *) context;
    gmHeaderDataAck *p = (gmHeaderDataAck *) &(buf->header.dataAck);

    if (status != GM_SUCCESS) {
        ulm_exit(("Error: Myrinet/GM: %s (%d): callback from gm_resume_sending() failed\n"));
    }

    ulm_warn(("Warning: Retrying timed-out ack: In callback for gm_resume_sending().\n"));

    // Find the device corresponding to this port
    for (dev = 0; dev < gmState.nDevsAllocated; dev++) {
        if (gmState.localDevList[dev].gmPort == port) {
            break;
        }
    }
    if (dev == gmState.nDevsAllocated) {
        ulm_err(("Error! Unable to match GM port.\n"));
        return;
    }

    // send the ACK again
    gm_send_with_callback(port,
                          p,
                          gmState.log2Size,
                          sizeof(gmHeader),
                          GM_LOW_PRIORITY,
                          gmState.localDevList[dev].remoteDevList[p->dest_proc].node_id,
                          gmState.localDevList[dev].remoteDevList[p->dest_proc].port_id,
                          gmRecvFragDesc::ackCallback,
                          context);
}


void gmRecvFragDesc::ackCallback(struct gm_port *port,
                                 void *context,
                                 gm_status_t status)
{
    int dev;
    gmFragBuffer *buf = (gmFragBuffer *) context;

    assert(buf);

    gmHeaderDataAck *p = (gmHeaderDataAck *) &(buf->header.dataAck);

    assert(p);

    // Find the device corresponding to this port
    for (dev = 0; dev < gmState.nDevsAllocated; dev++) {
        if (gmState.localDevList[dev].gmPort == port) {
            break;
        }
    }
    if (dev == gmState.nDevsAllocated) {
        ulm_err(("Error! Unable to match GM port.\n"));
        return;
    }

    switch (status) {
    case GM_TRY_AGAIN:
    case GM_BUSY:
        ulm_exit(("Error: Myrinet/GM: %s (%d). "
		  "Sending ack from rank %d (%s) to rank %d.\n",
		  gm_strerror(status), (int) status,
                  myproc(), mynodename(), p->dest_proc));
        break;
    case GM_SEND_TIMED_OUT:
        // it is also possible to use gm_drop_sends() instead of
	// gm_resume_sending() if minimizing disorder is more
	// important than avoiding requeuing/resending
        ulm_warn(("Warning: Myrinet/GM: %s (%d). Retrying timed-out ack: %d (%s) -> %d\n",
		  gm_strerror(status), (int) status,
		  myproc(), mynodename(), p->dest_proc));
        gm_resume_sending(port,
                          GM_LOW_PRIORITY,
                          gmState.localDevList[dev].remoteDevList[p->dest_proc].node_id,
                          gmState.localDevList[dev].remoteDevList[p->dest_proc].port_id,
                          gmPath_ack_timed_out_resume,
                          context);
        break;
    case GM_SEND_DROPPED:
        // try again if the receiver is busy 
        ulm_warn(("Warning: GM status is GM_SEND_DROPPED. Should not get here.\n"));
        break;
    case GM_SUCCESS: 
        // reclaim send token and return fragment buffer to free list
        if (usethreads()) {
            gmState.localDevList[dev].Lock.lock();
            gmState.localDevList[dev].sendTokens++;
            gmState.localDevList[dev].bufList.returnElementNoLock((Links_t *) buf, 0);
            gmState.localDevList[dev].Lock.unlock();
        } else {
            gmState.localDevList[dev].sendTokens++;
            gmState.localDevList[dev].bufList.returnElementNoLock((Links_t *) buf, 0);
        }
        break;
    default:
        // otherwise fail if there was a failure 
        ulm_exit(("Error: Myrinet/GM: %s (%d). "
                  "Trying to send ack from rank %d (%s).\n",
                  gm_strerror(status), (int) status, myproc(), mynodename()));
    }
}


// Free send resources on reception of a data ACK

void gmRecvFragDesc::msgDataAck(double timeNow)
{
    assert(gmHeader_m);

    gmSendFragDesc *sfd;
    SendDesc_t *bsd;
    gmHeaderDataAck *p = &(gmHeader_m->dataAck);

    assert(p);

    sfd = (gmSendFragDesc *) p->ptrToSendDesc.ptr;
    bsd = (SendDesc_t *) sfd->parentSendDesc_m;

    assert(sfd);
    assert(bsd);

    // if we receive duplicate acks - the send descriptor may have
    // already been returned to the freelist - or reused - before the
    // duplicate is received - so verify that this is the correct
    // descriptor
    if (NULL == bsd || bsd->isendSeq_m != p->isendSeq_m) {
        ReturnDescToPool(0);
        return;
    }
    // lock frag through send descriptor to prevent two ACKs from
    // processing simultaneously

    if (usethreads()) {
        bsd->Lock.lock();
        if (bsd != sfd->parentSendDesc_m) {
            bsd->Lock.unlock();
            ReturnDescToPool(0);
            return;
        }
    }
    if (ENABLE_RELIABILITY) {
        if (checkForDuplicateAndNonSpecificAck(sfd)) {
            if (usethreads()) {
                bsd->Lock.unlock();
            }
            ReturnDescToPool(0);
            return;
        }
    }

    if (ACKSTATUS_DATAGOOD == p->ackStatus) {
        sfd->setDidReceiveAck(true);
    }
    handlePt2PtMessageAck(timeNow, (SendDesc_t *) bsd, sfd);

    if (usethreads()) {
        bsd->Lock.unlock();
    }
    ReturnDescToPool(0);
}


// Initialize descriptor with data from fragment data header

void gmRecvFragDesc::msgData(double timeNow)
{
    assert(gmHeader_m);

    gmHeaderData *p = &(gmHeader_m->data);

    assert(p);

    DataOK = false;
    srcProcID_m = p->senderID;
    dstProcID_m = p->destID;
    tag_m = p->user_tag;
    ctx_m = EXTRACT_CTX(p->ctxAndMsgType);
    msgType_m = EXTRACT_MSGTYPE(p->ctxAndMsgType);
    isendSeq_m = p->isendSeq_m;
    seq_m = p->frag_seq;
    seqOffset_m = p->dataSeqOffset;
    msgLength_m = p->msgLength;
    fragIndex_m = seqOffset_m / gmState.bufSize;

    // check for valid communicator - if the communicator has already
    // been released this is a duplicate fragment that can be dropped
    if (communicators[ctx_m] == NULL) {
        ReturnDescToPool(0);
        return;
    }

    if (0) {                    // debug
        ulm_warn(("%d received frag from %d (length %ld)\n"
                  "%d\tgmFragBuffer_m %p gmHeader_m %p addr_m %p sizeof(gmHeader) %ld dev_m %d Dest %d\n"
                  "%d\ttag_m %d ctx_m %d msgType_m %d isendSeq_m %ld seq_m %lld seqOffset_m %ld\n"
                  "%d\tmsgLength_m %ld fragIndex_m %d\n",
                  myproc(), srcProcID_m, length_m,
                  myproc(), gmFragBuffer_m, gmHeader_m, addr_m,
                  (long) sizeof(gmHeader), dev_m, dstProcID_m, myproc(),
                  tag_m, ctx_m, msgType_m, isendSeq_m, seq_m, seqOffset_m,
                  myproc(), msgLength_m, fragIndex_m));
    }

    // remap process IDs from global to group ProcID
    dstProcID_m = communicators[ctx_m]->
        localGroup->mapGlobalProcIDToGroupProcID[dstProcID_m];
    srcProcID_m = communicators[ctx_m]->
        remoteGroup->mapGlobalProcIDToGroupProcID[srcProcID_m];

    communicators[ctx_m]->handleReceivedFrag((BaseRecvFragDesc_t *) this,
                                             timeNow);
}



