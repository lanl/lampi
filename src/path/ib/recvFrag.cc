/*
 * Copyright 2002-2003.  The Regents of the University of
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

extern "C" {
#include <vapi_common.h>
}

#undef PAGESIZE
#include "queue/globals.h"
#include "path/ib/recvFrag.h"
#include "path/ib/sendFrag.h"

bool ibRecvFragDesc::AckData(double timeNow)
{
    ibSendFragDesc *sfd;
    ibDataAck_t *p;
    int returnValue = ULM_SUCCESS;
    int i, hca_index, port_index, errorCode;
    ProcessPrivateMemDblLinkList *list;

    /* only send an ACK if ib_state.ack is false, if
     * the message is a synchronous send, and this is the
     * first message frag...
     */

    if (!ib_state.ack) {
        switch(msgType_m) {
            case MSGTYPE_PT2PT_SYNC:
                if (dataOffset() != 0) {
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

    if (usethreads()) {
        ib_state.lock.lock();
    }

    // find HCA to get send fragment descriptor...use round-robin processing 
    // of active HCAs and ports
    hca_index = -1;
    port_index = -1;
    for (i = 0; i < ib_state.num_active_hcas; i++) {
        int hca, j, k;
        j = i + ib_state.next_send_hca;
        j = (j >= ib_state.num_active_hcas) ? j - ib_state.num_active_hcas : j; 
        hca = ib_state.active_hcas[j];
        if ((ib_state.hca[hca].ud.sq_tokens >= 1) && (ib_state.hca[hca].send_cq_tokens >= 1)) {
            k = ib_state.next_send_port;
            k = (k >= ib_state.hca[hca].num_active_ports) ? k - ib_state.hca[hca].num_active_ports : k;
            // set port and hca index values...
            port_index = ib_state.hca[hca].active_ports[k];
            hca_index = hca;
            // store values for next time through...
            ib_state.next_send_hca = (k == (ib_state.hca[hca].num_active_ports - 1)) ? 
                ((j + 1) % ib_state.num_active_hcas) : j;
            ib_state.next_send_port = (j == ib_state.next_send_hca) ? k : 0;
            // decrement tokens...
            (ib_state.hca[hca_index].ud.sq_tokens)--;
            (ib_state.hca[hca_index].send_cq_tokens)--;
            break; 
        }
    }

    if ((hca_index < 0) || (port_index < 0)) {
        int dummyCode;
        if (usethreads()) {
            ib_state.lock.unlock();
        }
        path->cleanCtlMsgs(hca_index_m, timeNow, 0, (NUMBER_CTLMSGTYPES - 1), &dummyCode);
        return false;
    }

    sfd = ib_state.hca[hca_index].send_frag_list.getElementNoLock(0, returnValue);
    if (returnValue != ULM_SUCCESS) {
        int dummyCode;
        (ib_state.hca[hca_index].ud.sq_tokens)++;
        (ib_state.hca[hca_index].send_cq_tokens)++;
        if (usethreads()) {
            ib_state.lock.unlock();
        }
        path->cleanCtlMsgs(hca_index_m, timeNow, 0, (NUMBER_CTLMSGTYPES - 1), &dummyCode);
        return false;
    }

    p = (ibDataAck_t *)((unsigned long)(sfd->sg_m[0].addr));
    p->thisFragSeq = seq_m;

#if ENABLE_RELIABILITY
    Communicator *pg = communicators[ctx_m];
    unsigned int glSourceProcess =  pg->remoteGroup->mapGroupProcIDToGlobalProcID[srcProcID_m];

    if (isDuplicate_m) {
        p->ackStatus = ACKSTATUS_DATAGOOD;
    } else {
        p->ackStatus = (DataOK_m) ? ACKSTATUS_DATAGOOD : ACKSTATUS_DATACORRUPT;
    }
#else
    p->ackStatus = (DataOK_m) ? ACKSTATUS_DATAGOOD : ACKSTATUS_DATACORRUPT;
#endif

#if ENABLE_RELIABILITY
    if ((msgType_m == MSGTYPE_PT2PT) || (msgType_m == MSGTYPE_PT2PT_SYNC)) {
        // grab lock for sequence tracking lists
        if (usethreads())
            reliabilityInfo->dataSeqsLock[glSourceProcess].lock();

        // do we send a specific ACK...recordIfNotRecorded returns record status before attempting record
        bool recorded;
        bool send_specific_ack = reliabilityInfo->
            deliveredDataSeqs[glSourceProcess].recordIfNotRecorded(seq_m, &recorded);

        // record this frag as successfully delivered or not even received, as appropriate...
        if (!(isDuplicate_m)) {
            if (DataOK_m) {
                if (!recorded) {
                    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                    ulm_exit((-1, "ibRecvFragDesc::AckData(pt2pt) unable "
                              "to record deliv'd sequence number\n"));
                }
            } else {
                if (!(reliabilityInfo->receivedDataSeqs[glSourceProcess].erase(seq_m))) {
                    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                    ulm_exit((-1, "ibRecvFragDesc::AckData(pt2pt) unable "
                              "to erase rcv'd sequence number\n"));
                }
                if (!(reliabilityInfo->deliveredDataSeqs[glSourceProcess].erase(seq_m))) {
                    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                    ulm_exit((-1, "ibRecvFragDesc::AckData(pt2pt) unable "
                              "to erase deliv'd sequence number\n"));
                }
            }
        }
        else if (!send_specific_ack) {
            // if the frag is a duplicate but has not been delivered to the user process,
            // then set the field to 0 so the other side doesn't interpret
            // these fields (it will only use the receivedFragSeq and deliveredFragSeq fields
            p->thisFragSeq = 0;
            p->ackStatus = ACKSTATUS_AGGINFO_ONLY;
            if (!(reliabilityInfo->deliveredDataSeqs[glSourceProcess].erase(seq_m))) {
                reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                ulm_exit((-1, "ibRecvFragDesc::AckData(pt2pt) unable to erase "
                    "duplicate deliv'd sequence number\n"));
            }
        }

        p->receivedFragSeq = reliabilityInfo->
            receivedDataSeqs[glSourceProcess].largestInOrder();
        p->deliveredFragSeq = reliabilityInfo->
            deliveredDataSeqs[glSourceProcess].largestInOrder();

        // unlock sequence tracking lists
        if (usethreads())
            reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
    } else {
        // unknown communication type
        ulm_exit((-1, "ibRecvFragDesc::AckData() unknown communication "
                  "type %d\n", msgType_m));
    }
#else
    p->receivedFragSeq = 0;
    p->deliveredFragSeq = 0;
#endif

    // fill in other fields
    p->type = MESSAGE_DATA_ACK;
    p->ctxAndMsgType = msg_m->header.ctxAndMsgType;
    p->dest_proc = msg_m->header.senderID;
    p->src_proc = msg_m->header.destID;
    p->ptrToSendDesc = msg_m->header.sendFragDescPtr;

    sfd->init(MESSAGE_DATA_ACK, p->dest_proc, hca_index, port_index);

    list = &(ib_state.hca[hca_index].ctlMsgsToSend[MESSAGE_DATA_ACK]);
    list->AppendNoLock((Links_t *)sfd);
    ib_state.hca[hca_index].ctlMsgsToSendFlag |= (1 << MESSAGE_DATA_ACK);

    if (usethreads()) {
        ib_state.lock.unlock();
    }

    path->sendCtlMsgs(hca_index, timeNow, MESSAGE_DATA_ACK, MESSAGE_DATA_ACK, &errorCode);
    path->cleanCtlMsgs(hca_index, timeNow, MESSAGE_DATA_ACK, MESSAGE_DATA_ACK, &errorCode);

    return true;
}

unsigned int ibRecvFragDesc::CopyFunction(void *fragAddr, 
void *appAddr, ssize_t length)
{
    if (!length) {
        return ((usecrc()) ? CRC_INITIAL_REGISTER : 0);
    }
    if (ib_state.checksum) {
        if (usecrc()) {
            return bcopy_uicrc(fragAddr, appAddr, length, length_m);
        }
        else {
            return bcopy_uicsum(fragAddr, appAddr, length, length_m);
        }
    }
    else {
        MEMCOPY_FUNC(fragAddr, appAddr, length);
        return 0;
    }
}


// copy function for non-contiguous data with hooks for partial cksum word return
unsigned long ibRecvFragDesc::nonContigCopyFunction(void *appAddr,
                                                           void *fragAddr,
                                                           ssize_t length,
                                                           ssize_t cksumlength,
                                                           unsigned int *cksum,
                                                           unsigned int *partialInt,
                                                           unsigned int *partialLength,
                                                           bool firstCall,
                                                           bool lastCall)
{
    if (!cksum || !ib_state.checksum) {
        memcpy(appAddr, fragAddr, length);
    } else if (!usecrc()) {
        *cksum += bcopy_uicsum(fragAddr, appAddr, length, cksumlength,
                               partialInt, partialLength);
    } else {
        if (firstCall) {
            *cksum = CRC_INITIAL_REGISTER;
        }
        *cksum = bcopy_uicrc(fragAddr, appAddr, length, cksumlength, *cksum);
    }
    return length;
}

// check data
bool ibRecvFragDesc::CheckData(unsigned int checkSum, ssize_t length)
{
    if (!length || !ib_state.checksum) {
        DataOK_m = true;
    } else if (checkSum == msg_m->header.dataChecksum) {
        DataOK_m = true;
    } else {
        DataOK_m = false;
        if (!ib_state.ack) {
            ulm_exit((-1, "ibRecvFragDesc::CheckData: - corrupt data received "
                      "(%s 0x%x calculated 0x%x)\n",
                      (usecrc()) ? "CRC" : "checksum",
                      msg_m->header.dataChecksum, checkSum));
        }
    }
    return DataOK_m;
}

void ibRecvFragDesc::ReturnDescToPool(int LocalRank)
{
    VAPI_ret_t vapi_result;
    ib_hca_state_t *h = &(ib_state.hca[hca_index_m]);

    // repost the descriptor 
    vapi_result = VAPI_post_rr(h->handle, h->ud.handle, &(rr_desc_m));
    if (vapi_result != VAPI_OK) {
        ulm_err(("ibRecvFragDesc::ReturnDescToPool: VAPI_post_rr() for HCA "
            "%d returned %s\n",
            hca_index_m, VAPI_strerror(vapi_result)));
        exit(1);
    }

    // decrement tokens...don't need to check values...
    if (usethreads()) {
        ib_state.lock.lock();
    }

    (h->recv_cq_tokens)--;
    (h->ud.rq_tokens)--;

    if (usethreads()) {
        ib_state.lock.unlock();
    }
}

void ibRecvFragDesc::msgData(double timeNow)
{
    addr_m = msg_m->data;
    srcProcID_m = msg_m->header.senderID;
    dstProcID_m = msg_m->header.destID;
    tag_m = msg_m->header.tag_m;
    ctx_m = EXTRACT_CTX(msg_m->header.ctxAndMsgType);
    msgType_m = EXTRACT_MSGTYPE(msg_m->header.ctxAndMsgType);
    isendSeq_m = msg_m->header.isendSeq_m;
    seq_m = msg_m->header.frag_seq;
    seqOffset_m = msg_m->header.dataSeqOffset;
    msgLength_m = msg_m->header.msgLength;
    length_m = msg_m->header.dataLength;

    // remap process IDs from global to group ProcID
    dstProcID_m = communicators[ctx_m]->
        localGroup->mapGlobalProcIDToGroupProcID[dstProcID_m];
    srcProcID_m = communicators[ctx_m]->
        remoteGroup->mapGlobalProcIDToGroupProcID[srcProcID_m];

    communicators[ctx_m]->handleReceivedFrag((BaseRecvFragDesc_t *)this, timeNow);
}

void ibRecvFragDesc::msgDataAck(double timeNow)
{
    ibDataAck_t *p = (ibDataAck_t *)addr_m;
    ibSendFragDesc *sfd = (ibSendFragDesc *)p->ptrToSendDesc.ptr;
    volatile SendDesc_t *bsd = (volatile SendDesc_t *)sfd->parentSendDesc_m;

    msgType_m = EXTRACT_MSGTYPE(p->ctxAndMsgType);

        // lock frag through send descriptor to prevent two
        // ACKs from processing simultaneously

        if (bsd) {
            ((SendDesc_t *)bsd)->Lock.lock();
            if (bsd != sfd->parentSendDesc_m) {
                ((SendDesc_t *)bsd)->Lock.unlock();
                ReturnDescToPool(getMemPoolIndex());
                return;
            }
        } else {
            ReturnDescToPool(getMemPoolIndex());
            return;
        }

#if ENABLE_RELIABILITY
        if (checkForDuplicateAndNonSpecificAck(sfd)) {
            ((SendDesc_t *)bsd)->Lock.unlock();
            ReturnDescToPool(getMemPoolIndex());
            return;
        }
#endif

        handlePt2PtMessageAck(timeNow, (SendDesc_t *)bsd, sfd);
        ((SendDesc_t *)bsd)->Lock.unlock();

    ReturnDescToPool(getMemPoolIndex());
    return;
}

#if ENABLE_RELIABILITY

inline bool ibRecvFragDesc::checkForDuplicateAndNonSpecificAck(ibSendFragDesc *sfd)
{
    ibDataAck_t *p = (ibDataAck_t *)addr_m;
    sender_ackinfo_control_t *sptr;
    sender_ackinfo_t *tptr;

    // update sender ACK info -- largest in-order delivered and received frag sequence
    // numbers received by our peers (or peer box, in the case of collective communication)
    // from ourself or another local process
    if ((msgType_m == MSGTYPE_PT2PT) || (msgType_m == MSGTYPE_PT2PT_SYNC)) {
        sptr = &(reliabilityInfo->sender_ackinfo[local_myproc()]);
        tptr = &(sptr->process_array[p->src_proc]);
    } else {
        ulm_err(("ibRecvFragDesc::check...Ack received ack of unknown "
                 "message type %d\n", msgType_m));
        return true;
    }

    sptr->Lock.lock();
    if (p->deliveredFragSeq > tptr->delivered_largest_inorder_seq) {
        tptr->delivered_largest_inorder_seq = p->deliveredFragSeq;
    }
    // received is not guaranteed to be a strictly increasing series...
    tptr->received_largest_inorder_seq = p->receivedFragSeq;
    sptr->Lock.unlock();

    // check to see if this ACK is for a specific frag or not
    // if not, then we don't need to do anything else...
    if (p->ackStatus == ACKSTATUS_AGGINFO_ONLY) {
        return true;
    } else if ((sfd->frag_seq_m != p->thisFragSeq)) {
        // this ACK is a duplicate...or just screwed up...just ignore it...
        return true;
    }

    return false;
}

#endif

inline void ibRecvFragDesc::handlePt2PtMessageAck(double timeNow, SendDesc_t *bsd,
                                                 ibSendFragDesc *sfd)
{
    short whichQueue = sfd->WhichQueue;
    ibDataAck_t *p = (ibDataAck_t *)addr_m;
    int errorCode;

    if (p->ackStatus == ACKSTATUS_DATAGOOD) {

        // register frag as acked
        bsd->clearToSend_m = true;
        (bsd->NumAcked)++;

        // remove frag descriptor from list of frags to be acked
        if (whichQueue == IBFRAGSTOACK) {
            bsd->FragsToAck.RemoveLinkNoLock((Links_t *)sfd);
        }
#if ENABLE_RELIABILITY
        else if (whichQueue == IBFRAGSTOSEND) {
            bsd->FragsToSend.RemoveLinkNoLock((Links_t *)sfd);
            // increment NumSent since we were going to send this again...
            (bsd->NumSent)++;
        }
#endif
        else {
            ulm_exit((-1, "ibRecvFragDesc::handlePt2PtMessageAck: Frag "
                      "on %d queue\n", whichQueue));
        }

        // reset WhichQueue flag
        sfd->WhichQueue = IBFRAGFREELIST;

#if ENABLE_RELIABILITY
        // set seq_m value to 0/null/invalid to detect duplicate ACKs
        sfd->frag_seq_m = 0;
#endif

        // reset send descriptor pointer
        sfd->parentSendDesc_m = 0;

        if (sfd->done(timeNow, &errorCode)) {
            sfd->free();
        }
        else {
            // we need to wait for local completion notification
            // before we free this descriptor...
            // put on ctlMsgsToAck list for later cleaning by push()
            if (usethreads()) {
                ib_state.lock.lock();
            }

            ProcessPrivateMemDblLinkList *list =
                &(ib_state.hca[sfd->hca_index_m].ctlMsgsToAck[MESSAGE_DATA]);
            list->AppendNoLock((Links_t *)sfd);
            ib_state.hca[sfd->hca_index_m].ctlMsgsToAckFlag |= (1 << MESSAGE_DATA);

            if (usethreads()) {
                ib_state.lock.unlock();
            }
        }

    } else {
        /*
         * only process negative acknowledgements if we are
         * the process that sent the original message; otherwise,
         * just rely on sender side retransmission
         */
        ulm_warn(("Warning: IB Data corrupt upon receipt. Will retransmit.\n"));

        // reset WhichQueue flag
        sfd->WhichQueue = IBFRAGSTOSEND;
        // move Frag from FragsToAck list to FragsToSend list
        if (whichQueue == IBFRAGSTOACK) {
            bsd->FragsToAck.RemoveLink((Links_t *)sfd);
            bsd->FragsToSend.Append((Links_t *)sfd);
        }
        // move message to incomplete queue
        if (bsd->NumSent == bsd->numfrags) {
            // sanity check, is frag really in UnackedPostedSends queue
            if (bsd->WhichQueue != UNACKEDISENDQUEUE) {
                ulm_exit((-1, "Error: Send descriptor not "
                          "in UnackedPostedSends"
                          " list, where it was expected.\n"));
            }
            bsd->WhichQueue = INCOMPLETEISENDQUEUE;
            UnackedPostedSends.RemoveLink(bsd);
            IncompletePostedSends.Append(bsd);
        }
        // reset send desc. NumSent as though this frag has not been sent
        (bsd->NumSent)--;
    } // end NACK/ACK processing
}
