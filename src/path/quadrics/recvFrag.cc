/*
 * Copyright 2002.  The Regents of the University of California. This material
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

#include <elan3/elan3.h>

#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/state.h"
#include "client/ULMClient.h"
#include "queue/globals.h"	// for getMemPoolIndex()
#include "path/sharedmem/SMPSharedMemGlobals.h"	// for SMPSharedMemDevs and alloc..
#include "path/quadrics/recvFrag.h"
#include "path/quadrics/sendFrag.h"
#include "path/quadrics/path.h"

bool quadricsRecvFragDesc::AckData(double timeNow)
{
    ProcessPrivateMemDblLinkList *list;
    quadricsSendFragDesc *sfd;
    quadricsCtlHdr_t *hdr;
    quadricsDataAck_t *p;
    int returnValue, errorCode;

    /* only send an ACK if quadricsDoAck is false, if
     * the message is a synchronous send, and this is the
     * first message frag...
     */

    if (refCnt_m > 0) { // multicast message 
        return true;
    }

    if (!quadricsDoAck) {
        /* free Elan-addressable memory now */
        if (length_m > CTLHDR_DATABYTES) {
            elan3_free(ctx, addr_m);
        }
        switch (msgType_m) {
        case MSGTYPE_PT2PT_SYNC:
        case MSGTYPE_COLL_SYNC:
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

    // grab a send descriptor for the request ACK
    // and a malloc'ed quadricsCtlHdr_t from our FIFO,
    // if at all possible...

    hdr = (quadricsCtlHdr_t *)quadricsHdrs.get();
    if (!hdr) {
        return false;
    }
    if (usethreads())
        sfd = quadricsSendFragDescs.getElement(rail, returnValue);
    else
        sfd = quadricsSendFragDescs.getElementNoLock(rail, returnValue);
    if (returnValue != ULM_SUCCESS) {
        int dummyCode;
        free(hdr);
        path->cleanCtlMsgs(rail, timeNow, 0, (NUMBER_CTLMSGTYPES - 1), &dummyCode);
        return false;
    }

    p = &(hdr->msgDataAck);
    p->thisFragSeq = seq_m;

#ifdef RELIABILITY_ON
    Communicator *pg = communicators[ctx_m];
    unsigned int glSourceProcess =  pg->remoteGroup->mapGroupProcIDToGlobalProcID[srcProcID_m];

    if (isDuplicate_m) {
        p->ackStatus = ACKSTATUS_DATAGOOD;
    } else {
        p->ackStatus = (DataOK) ? ACKSTATUS_DATAGOOD : ACKSTATUS_DATACORRUPT;
    }
#else
    p->ackStatus = (DataOK) ? ACKSTATUS_DATAGOOD : ACKSTATUS_DATACORRUPT;
#endif

#ifdef RELIABILITY_ON
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
            if (DataOK) {
                if (!recorded) {
                    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                    ulm_exit((-1, "quadricsRecvFragDesc::AckData(pt2pt) unable "
                              "to record deliv'd sequence number\n"));
                }
            } else {
                if (!(reliabilityInfo->receivedDataSeqs[glSourceProcess].erase(seq_m))) {
                    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                    ulm_exit((-1, "quadricsRecvFragDesc::AckData(pt2pt) unable "
                              "to erase rcv'd sequence number\n"));
                }
                if (!(reliabilityInfo->deliveredDataSeqs[glSourceProcess].erase(seq_m))) {
                    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                    ulm_exit((-1, "quadricsRecvFragDesc::AckData(pt2pt) unable "
                              "to erase deliv'd sequence number\n"));
                }
            }
        }
        else if (!send_specific_ack) {
            // if the frag is a duplicate but has not been delivered to the user process,
            // then set the field to 0 so the other side doesn't interpret
            // these fields (it will only use the received_fragseq and delivered_fragseq fields
            p->thisFragSeq = 0;
            p->ackStatus = ACKSTATUS_AGGINFO_ONLY;
            if (!(reliabilityInfo->deliveredDataSeqs[glSourceProcess].erase(seq_m))) {
                reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
                ulm_exit((-1, "quadricsRecvFragDesc::AckData(pt2pt) unable to erase duplicate deliv'd sequence number\n"));
            }
        }

        p->receivedFragSeq = reliabilityInfo->
            receivedDataSeqs[glSourceProcess].largestInOrder();
        p->deliveredFragSeq = reliabilityInfo->
            deliveredDataSeqs[glSourceProcess].largestInOrder();

        // unlock sequence tracking lists
        if (usethreads())
            reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
    } else if ((msgType_m == MSGTYPE_COLL) || (msgType_m == MSGTYPE_COLL_SYNC)) {
        int source_box = global_proc_to_host(srcProcID_m);
        // grab lock for sequence tracking lists
        reliabilityInfo->coll_dataSeqsLock[source_box].lock();

        // do we send a specific ACK...recordIfNotRecorded returns record status before attempting record
        bool recorded;
        bool send_specific_ack = reliabilityInfo->coll_deliveredDataSeqs[source_box].
            recordIfNotRecorded(seq_m, &recorded);

        // record this frag as successfully delivered or not even received, as appropriate...
        if (!(isDuplicate_m)) {
            if (DataOK) {
                if (!recorded) {
                    reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
                    ulm_exit((-1, "quadricsRecvFragDesc::AckData(collective) "
                              "unable to record deliv'd sequence number\n"));
                }
            } else {
                if (!(reliabilityInfo->coll_receivedDataSeqs[source_box].erase(seq_m))) {
                    reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
                    ulm_exit((-1, "quadricsRecvFragDesc::AckData(collective) "
                              "unable to erase rcv'd sequence number\n"));
                }
                if (!(reliabilityInfo->coll_deliveredDataSeqs[source_box].erase(seq_m))) {
                    reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
                    ulm_exit((-1, "quadricsRecvFragDesc::AckData(collective) "
                              "unable to erase deliv'd sequence number\n"));
                }
            }
        }
        else if (!send_specific_ack) {
            // if the frag is a duplicate but has not been delivered to the user process,
            // then set the field to 0 so the other side doesn't interpret
            // these fields (it will only use the received_fragseq and delivered_fragseq fields
            p->thisFragSeq = 0;
            p->ackStatus = ACKSTATUS_AGGINFO_ONLY;
            if (!(reliabilityInfo->coll_deliveredDataSeqs[source_box].erase(seq_m))) {
                reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
                ulm_exit((-1, "quadricsRecvFragDesc::AckData(collective) unable to erase duplicate deliv'd sequence number\n"));
            }
        }

        p->receivedFragSeq = reliabilityInfo->
            coll_receivedDataSeqs[source_box].largestInOrder();
        p->deliveredFragSeq = reliabilityInfo->
            coll_deliveredDataSeqs[source_box].largestInOrder();

        // unlock sequence tracking lists
        reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
    } else {
        // unknown communication type
        ulm_exit((-1, "quadricsRecvFragDesc::AckData() unknown communication "
                  "type %d\n", msgType_m));
    }
#else
    p->receivedFragSeq = 0;
    p->deliveredFragSeq = 0;
#endif

    // fill in other fields
    p->ctxAndMsgType = envelope.msgDataHdr.ctxAndMsgType;
    p->tag_m = envelope.msgDataHdr.tag_m;
    p->sendFragDescPtr = envelope.msgDataHdr.sendFragDescPtr;

    // use incoming ctx and rail!
    sfd->init(
        0,
        ctx,
        MESSAGE_DATA_ACK,
        rail,
        envelope.msgDataHdr.senderID,
        -1,
        0,
        0,
        0,
        0,
        0,
        false,
        hdr
        );

    list = &(quadricsQueue[rail].ctlMsgsToSend[MESSAGE_DATA_ACK]);
    if (usethreads()) {
        list->Lock.lock();
        list->AppendNoLock((Links_t *)sfd);
        quadricsQueue[rail].ctlFlagsLock.lock();
        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MESSAGE_DATA_ACK);
        quadricsQueue[rail].ctlFlagsLock.unlock();
        list->Lock.unlock();
    }
    else {
        list->AppendNoLock((Links_t *)sfd);
        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MESSAGE_DATA_ACK);
    }

    path->sendCtlMsgs(rail, timeNow, MESSAGE_DATA_ACK, MESSAGE_DATA_ACK, &errorCode);
    path->cleanCtlMsgs(rail, timeNow, MESSAGE_DATA_ACK, MESSAGE_DATA_ACK, &errorCode);
    return true;
}


#ifdef RELIABILITY_ON

bool quadricsRecvFragDesc::isDuplicateCollectiveFrag()
{
    int source_box = global_proc_to_host(srcProcID_m);
    // duplicate/dropped message frag support
    // only for those communication paths that overwrite
    // the seq_m value from its default constructor
    // value of 0 -- valid sequence numbers are from
    // (1 .. (2^64 - 1)))

    if (seq_m != 0) {
        bool sendthisack;
        bool recorded;
        reliabilityInfo->coll_dataSeqsLock[source_box].lock();
        if (reliabilityInfo->coll_receivedDataSeqs[source_box].recordIfNotRecorded(seq_m, &recorded)) {
            // this is a duplicate from a retransmission...maybe a previous ACK was lost?
            // unlock so we can lock while building the ACK...
            if (reliabilityInfo->coll_deliveredDataSeqs[source_box].isRecorded(seq_m)) {
                // send another ACK for this specific frag...
                isDuplicate_m = true;
                sendthisack = true;
            } else if (reliabilityInfo->coll_receivedDataSeqs[source_box].largestInOrder() >= seq_m) {
                // send a non-specific ACK that should prevent this frag from being retransmitted...
                isDuplicate_m = true;
                sendthisack = true;
            } else {
                // no acknowledgment is appropriate, just discard this frag and continue...
                sendthisack = false;
            }
            reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
            // do we send an ACK for this frag?
            if (!sendthisack) {
                return true;
            }
            // try our best to ACK otherwise..don't worry about outcome
            MarkDataOK(true);
            AckData();
            return true;
        }                       // end duplicate frag processing
        // record the fragment sequence number
        if (!recorded) {
            reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
            ulm_exit((-1, "unable to record frag sequence number(1)\n"));
        }
        reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
        // continue on...
    }
    return false;
}


bool quadricsRecvFragDesc::checkForDuplicateAndNonSpecificAck(quadricsSendFragDesc *sfd)
{
    quadricsDataAck_t *p = &(envelope.msgDataAck);
    sender_ackinfo_control_t *sptr;
    sender_ackinfo_t *tptr;

    // update sender ACK info -- largest in-order delivered and received frag sequence
    // numbers received by our peers (or peer box, in the case of collective communication)
    // from ourself or another local process
    if ((msgType_m == MSGTYPE_PT2PT) || (msgType_m == MSGTYPE_PT2PT_SYNC)) {
        sptr = &(reliabilityInfo->sender_ackinfo[local_myproc()]);
        tptr = &(sptr->process_array[p->senderID]);
    } else if ((msgType_m == MSGTYPE_COLL) || (msgType_m == MSGTYPE_COLL_SYNC)) {
        sptr = reliabilityInfo->coll_sender_ackinfo;
        tptr = &(sptr->process_array[global_proc_to_host(p->senderID)]);
    } else {
        ulm_err(("quadricsRecvFragDesc::check...Ack received ack of unknown "
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
    } else if ((sfd->frag_seq != p->thisFragSeq)) {
        // this ACK is a duplicate...or just screwed up...just ignore it...
        return true;
    }

    return false;
}

#endif


void quadricsRecvFragDesc::handleMulticastMessageAck(double timeNow, UtsendDesc_t *msd,
                                                     BaseSendDesc_t *bsd, quadricsSendFragDesc *sfd)
{
    quadricsDataAck_t *p = &(envelope.msgDataAck);
    int errorCode;

    if (p->ackStatus == ACKSTATUS_DATAGOOD) {
        // register Fragment as acked
        bsd->clearToSend_m = true;
        (bsd->NumAcked)++;

        // reset queues appropriately
        if (sfd->WhichQueue == QUADRICSFRAGSTOACK) {
            bsd->FragsToAck.RemoveLinkNoLock((Links_t *)sfd);
        }

#ifdef RELIABILITY_ON
        else if (sfd->WhichQueue == QUADRICSFRAGSTOSEND) {
            bsd->FragsToSend.RemoveLinkNoLock((Links_t *)sfd);
            // increment NumSent since we were going to send this again...
            (bsd->NumSent)++;
        }
#endif
        else {
            ulm_exit((-1,
                      "quadricsRecvFragDesc::handleMulticastMessageAck "
                      "Frag on %d queue\n",
                      sfd->WhichQueue));
        }

        sfd->WhichQueue = QUADRICSFRAGFREELIST;

#ifdef RELIABILITY_ON
        // set fragment sequence number to 0/null/invalid to detect duplicate ACKs
        sfd->frag_seq = 0;
#endif

        // re-initialize data members
        sfd->parentSendDesc = 0;

        if (sfd->done(timeNow, &errorCode)) {
            // return all sfd send resources
            sfd->freeRscs();
            sfd->WhichQueue = QUADRICSFRAGFREELIST;
            // return frag descriptor to free list
            // the header holds the global proc id
            if (usethreads()) {
                quadricsSendFragDescs.returnElement(sfd, sfd->rail);
            } else {
                quadricsSendFragDescs.returnElementNoLock(sfd, sfd->rail);
            }
        }
        else {
            // set a bool to make sure this is not retransmitted
            // when cleanCtlMsgs() removes it from ctlMsgsToAck...
            sfd->freeWhenDone = true;
            // put on ctlMsgsToAck list for later cleaning by push()
            // the ACK has come after rescheduling this frag to
            // be resent...
            ProcessPrivateMemDblLinkList *list =
                &(quadricsQueue[rail].ctlMsgsToAck[MESSAGE_DATA]);
            if (usethreads()) {
                list->Lock.lock();
                list->AppendNoLock((Links_t *)sfd);
                quadricsQueue[rail].ctlFlagsLock.lock();
                quadricsQueue[rail].ctlMsgsToAckFlag |= (1 << MESSAGE_DATA);
                quadricsQueue[rail].ctlFlagsLock.unlock();
                list->Lock.unlock();
            }
            else {
                list->AppendNoLock((Links_t *)sfd);
                quadricsQueue[rail].ctlMsgsToAckFlag |= (1 << MESSAGE_DATA);
            }
        }

        // for multicast messages we return BaseSendDesc_t's to the free pool
        if ((bsd->NumAcked == bsd->numfrags) &&
            (bsd->FragsToAck.size() == 0) && (bsd->FragsToSend.size() == 0)) {
            // increment the multicast send descriptor sub-message completed count
            (msd->messageDoneCount)++;
            if (bsd->WhichQueue == INCOMPLETEISENDQUEUE) {
                msd->incompletePt2PtMessages.RemoveLinkNoLock(bsd);
            } else if (bsd->WhichQueue == UNACKEDISENDQUEUE) {
                msd->unackedPt2PtMessages.RemoveLinkNoLock(bsd);
            } else {
                ulm_exit((-1, "quadricsRecvFragDesc::handleMulticastMessageAck base"
                          "send desc. on %d queue.\n",
                          bsd->WhichQueue));
            }
            // return the descriptor to the sending process' pool
            bsd->ReturnDesc(getMemPoolIndex());
        }
    } else  {
        // only process negative acknowledgements if we are
        // the process that sent the original message; otherwise,
        // just rely on sender side retransmission
        ulm_warn(("Warning: Quadrics Data corrupt on receipt.  Will retransmit.\n"));

        // save and then reset WhichQueue flag
        short whichQueue = sfd->WhichQueue;
        sfd->WhichQueue = QUADRICSFRAGSTOSEND;
        // move sfd from FragsToAck list to FragsToSend list
        if (whichQueue == QUADRICSFRAGSTOACK) {
            bsd->FragsToAck.RemoveLinkNoLock((Links_t *)sfd);
            bsd->FragsToSend.AppendNoLock((Links_t *)sfd);
        }
        // move message to incomplete queue
        if (bsd->NumSent == bsd->numfrags) {
            if (bsd->WhichQueue == UNACKEDISENDQUEUE) {
                bsd->WhichQueue = INCOMPLETEISENDQUEUE;
                msd->unackedPt2PtMessages.RemoveLinkNoLock(bsd);
                msd->incompletePt2PtMessages.AppendNoLock(bsd);
            }
        }
        // reset send desc. NumSent as though this frag has not been sent
        (bsd->NumSent)--;
    }
}

void quadricsRecvFragDesc::handlePt2PtMessageAck(double timeNow, BaseSendDesc_t *bsd,
                                                 quadricsSendFragDesc *sfd)
{
    short whichQueue = sfd->WhichQueue;
    quadricsDataAck_t *p = &(envelope.msgDataAck);
    int errorCode;

    if (p->ackStatus == ACKSTATUS_DATAGOOD) {

        // register frag as acked
        bsd->clearToSend_m = true;
        (bsd->NumAcked)++;

        // remove frag descriptor from list of frags to be acked
        if (whichQueue == QUADRICSFRAGSTOACK) {
            bsd->FragsToAck.RemoveLinkNoLock((Links_t *)sfd);
        }
#ifdef RELIABILITY_ON
        else if (whichQueue == QUADRICSFRAGSTOSEND) {
            bsd->FragsToSend.RemoveLinkNoLock((Links_t *)sfd);
            // increment NumSent since we were going to send this again...
            (bsd->NumSent)++;
        }
#endif
        else {
            ulm_exit((-1, "quadricsRecvFragDesc::processAck: Frag "
                      "on %d queue\n", whichQueue));
        }

        // reset WhichQueue flag
        sfd->WhichQueue = QUADRICSFRAGFREELIST;

#ifdef RELIABILITY_ON
        // set seq_m value to 0/null/invalid to detect duplicate ACKs
        sfd->frag_seq = 0;
#endif

        // reset send descriptor pointer
        sfd->parentSendDesc = 0;

        if (sfd->done(timeNow, &errorCode)) {
            // return all sfd send resources
            sfd->freeRscs();
            sfd->WhichQueue = QUADRICSFRAGFREELIST;
            // return frag descriptor to free list
            // the header holds the global proc id
            if (usethreads()) {
                quadricsSendFragDescs.returnElement(sfd, sfd->rail);
            } else {
                quadricsSendFragDescs.returnElementNoLock(sfd, sfd->rail);
            }
        }
        else {
            // set a bool to make sure this is not retransmitted
            // when cleanCtlMsgs() removes it from ctlMsgsToAck...
            sfd->freeWhenDone = true;
            // put on ctlMsgsToAck list for later cleaning by push()
            // the ACK has come after rescheduling this frag to
            // be resent...
            ProcessPrivateMemDblLinkList *list =
                &(quadricsQueue[rail].ctlMsgsToAck[MESSAGE_DATA]);
            if (usethreads()) {
                list->Lock.lock();
                list->AppendNoLock((Links_t *)sfd);
                quadricsQueue[rail].ctlFlagsLock.lock();
                quadricsQueue[rail].ctlMsgsToAckFlag |= (1 << MESSAGE_DATA);
                quadricsQueue[rail].ctlFlagsLock.unlock();
                list->Lock.unlock();
            }
            else {
                list->AppendNoLock((Links_t *)sfd);
                quadricsQueue[rail].ctlMsgsToAckFlag |= (1 << MESSAGE_DATA);
            }
        }

    } else {
        /*
         * only process negative acknowledgements if we are
         * the process that sent the original message; otherwise,
         * just rely on sender side retransmission
         */
        ulm_warn(("Warning: Quadrics Data corrupt upon receipt.  Will retransmit.\n"));

        // reset WhichQueue flag
        sfd->WhichQueue = QUADRICSFRAGSTOSEND;
        // move Frag from FragsToAck list to FragsToSend list
        if (whichQueue == QUADRICSFRAGSTOACK) {
            bsd->FragsToAck.RemoveLink((Links_t *)sfd);
            bsd->FragsToSend.Append((Links_t *)sfd);
        }
        // move message to incomplete queue
        if (bsd->NumSent == bsd->numfrags) {
            // sanity check, is frag really in UnackedPostedSends queue
            if (bsd->WhichQueue != UNACKEDISENDQUEUE) {
                ulm_exit((-1, "Error: :: Send descriptor not "
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


void quadricsRecvFragDesc::msgData(double timeNow)
{
    quadricsDataHdr_t *p = &(envelope.msgDataHdr);

    // fill in base info fields
    length_m = p->dataLength;
    msgLength_m = p->msgLength;
    addr_m = (length_m <= CTLHDR_DATABYTES) ? (void *)p->data :
        (void *)elan3_elan2main(ctx, (E3_Addr)p->dataElanAddr);
    srcProcID_m = p->senderID;
    dstProcID_m = p->destID;
    tag_m = p->tag_m;
    ctx_m = EXTRACT_CTX(p->ctxAndMsgType);
    msgType_m = EXTRACT_MSGTYPE(p->ctxAndMsgType);
    isendSeq_m = p->isendSeq_m;
    seq_m = p->frag_seq;
    refCnt_m = p->common.multicastRefCnt;
    seqOffset_m = dataOffset();
    fragIndex_m = seqOffset_m /
        quadricsBufSizes[(msgType_m == MSGTYPE_COLL) ?
                         SHARED_LARGE_BUFFERS : PRIVATE_LARGE_BUFFERS];
    poolIndex_m = getMemPoolIndex();

    if (refCnt_m > 0) { // multicast message
        // remap process IDs from global to group ProcID
        dstProcID_m = communicators[ctx_m]->
            localGroup->mapGlobalProcIDToGroupProcID[myproc()];
        srcProcID_m = communicators[ctx_m]->
            remoteGroup->mapGlobalProcIDToGroupProcID[srcProcID_m];

        // what do we do if the return value is not ULM_SUCCESS?
        communicators[ctx_m]->handleReceivedFrag((BaseRecvFragDesc_t *)this, timeNow);
        return;
    }

#ifdef RELIABILITY_ON
    isDuplicate_m = false;
#endif

    // make sure this was intended for this process...
    if (dstProcID_m != myproc()) {
        ulm_exit((-1, "Quadrics data frag on rail %d from process %d misdirected to process "
                  "%d, intended destination %d\n",
                  rail, srcProcID_m, myproc(), dstProcID_m));
    }

    if ((msgType_m == MSGTYPE_COLL) || (msgType_m == MSGTYPE_COLL_SYNC)) {
        // multicast message...
#ifdef RELIABILITY_ON
        if (isDuplicateCollectiveFrag()) {
            ReturnDescToPool(getMemPoolIndex());
            return;
        }
#endif
        WhichQueue = GROUPRECVFRAGS;
        _ulm_CommunicatorRecvFrags[global_to_local_proc(dstProcID_m)]->
            AppendAsWriter((Links_t *)this);
    }
    else if ((msgType_m == MSGTYPE_PT2PT) || (msgType_m == MSGTYPE_PT2PT_SYNC)) {
        // point to point message ...
        // call Communicator::handleReceivedFrag directly

        // remap process IDs from global to group ProcID
        dstProcID_m = communicators[ctx_m]->
            localGroup->mapGlobalProcIDToGroupProcID[dstProcID_m];
        srcProcID_m = communicators[ctx_m]->
            remoteGroup->mapGlobalProcIDToGroupProcID[srcProcID_m];

        // what do we do if the return value is not ULM_SUCCESS?
        communicators[ctx_m]->handleReceivedFrag((BaseRecvFragDesc_t *)this, timeNow);

    }
    else {
        ulm_exit((-1, "Quadrics data envelope with invalid message "
                  "type %d\n", msgType_m));
    }
    return;
}

void quadricsRecvFragDesc::msgDataAck(double timeNow)
{
    quadricsDataAck_t *p = &(envelope.msgDataAck);
    quadricsSendFragDesc *sfd = (quadricsSendFragDesc *)p->sendFragDescPtr;
    volatile BaseSendDesc_t *bsd = (volatile BaseSendDesc_t *)sfd->parentSendDesc;

    msgType_m = EXTRACT_MSGTYPE(p->ctxAndMsgType);

    // Check to see if this is an ack for a collective message,
    // if so, handle appropriately.  If not, continue handling as
    // a pt-2-pt ack.
    if ((msgType_m == MSGTYPE_COLL) || (msgType_m == MSGTYPE_COLL_SYNC)) {
        UtsendDesc_t *msd;

#ifdef RELIABILITY_ON
        // bsd may be 0, so check before trying to use it...
        if (bsd) {
            msd = bsd->multicastMessage;
            if (msd) {
                msd->Lock.lock();
                if ((msd != bsd->multicastMessage) ||
                    (bsd != (volatile BaseSendDesc_t *)sfd->parentSendDesc)) {
                    msd->Lock.unlock();
                    ReturnDescToPool(getMemPoolIndex());
                    return;
                }
            } else {
                ReturnDescToPool(getMemPoolIndex());
                return;
            }
        } else {
            ReturnDescToPool(getMemPoolIndex());
            return;
        }

        if (checkForDuplicateAndNonSpecificAck(sfd)) {
            msd->Lock.unlock();
            ReturnDescToPool(getMemPoolIndex());
            return;
        }
#else
        msd = bsd->multicastMessage;
        msd->Lock.lock();
#endif
        handleMulticastMessageAck(timeNow, msd, (BaseSendDesc_t *)bsd, sfd);
        msd->Lock.unlock();
        ReturnDescToPool(getMemPoolIndex());
        return;
    } else {

        // lock frag through send descriptor to prevent two
        // ACKs from processing simultaneously

        if (bsd) {
            ((BaseSendDesc_t *)bsd)->Lock.lock();
            if (bsd != sfd->parentSendDesc) {
                ((BaseSendDesc_t *)bsd)->Lock.unlock();
                ReturnDescToPool(getMemPoolIndex());
                return;
            }
        } else {
            ReturnDescToPool(getMemPoolIndex());
            return;
        }

#ifdef RELIABILITY_ON
        if (checkForDuplicateAndNonSpecificAck(sfd)) {
            ((BaseSendDesc_t *)bsd)->Lock.unlock();
            ReturnDescToPool(getMemPoolIndex());
            return;
        }
#endif

        handlePt2PtMessageAck(timeNow, (BaseSendDesc_t *)bsd, sfd);
        ((BaseSendDesc_t *)bsd)->Lock.unlock();
    }

    ReturnDescToPool(getMemPoolIndex());
    return;
}


void quadricsRecvFragDesc::memRel()
{
    quadricsMemRls_t *p = &(envelope.memRelease);

    // multiple threads are prevented from executing this by rcvLock...

    // is this a duplicate or misdirected?
    if ((p->destID != myproc()) || quadricsMemRlsSeqList[p->senderID].isRecorded(p->releaseSeq)) {
        ReturnDescToPool(getMemPoolIndex());
        return;
    }

    // NO, then free the memory
    switch (p->memType) {
    case PRIVATE_SMALL_BUFFERS:
    case PRIVATE_LARGE_BUFFERS:
        for (int i = 0; i < p->memBufCount; i++) {
            elan3_free(ctx, elan3_elan2main(ctx, (E3_Addr)(p->memBufPtrs.wordPtrs[i])));
        }
        break;
    case SHARED_SMALL_BUFFERS:
    case SHARED_LARGE_BUFFERS:
        for (int i = 0; i < p->memBufCount; i++) {
            SMPSharedMemDevs[getMemPoolIndex()].MemoryBuckets.
                ULMFree(elan3_elan2main(ctx, (E3_Addr)(p->memBufPtrs.wordPtrs[i])));
        }
        break;
    default:
        ulm_exit((-1, "quadricsRecvFragDesc::memRel: bad memory "
                  "type %d!\n", p->memType));
        break;
    }

    // record serial number
    if (!quadricsMemRlsSeqList[p->senderID].record(p->releaseSeq)) {
        ulm_exit((-1, "quadricsRecvFragDesc::memRel: unable to record "
                  "%lld in quadricsMemRlsSeqList[%d]\n",
                  p->releaseSeq, p->senderID));
    }

    ReturnDescToPool(getMemPoolIndex());
    return;
}

#define MEMREQ_PREFETCH_FACTOR ((int)3)

void quadricsRecvFragDesc::memReq(double timeNow)
{
    ProcessPrivateMemDblLinkList *list;
    quadricsMemReq_t *p = &(envelope.memRequest);
    quadricsSendFragDesc *sfd;
    quadricsCtlHdr_t *hdr;
    SMPSharedMem_logical_dev_t *dev = 0;
    void *addrs[MEMREQ_MAX_WORDPTRS];
    int naddrs = 0;
    int naddrsNeeded = 0;
    int returnValue = ULM_SUCCESS;
    int mtype = EXTRACT_MSGTYPE(p->ctxAndMsgType);
    int bufType;
    int status = MEMREQ_SUCCESS;
    int errorCode;
    unsigned long long neededBytes = p->memNeededBytes;

    // grab a send descriptor for the request ACK
    // and a malloc'ed quadricsCtlHdr_t

    hdr = (quadricsCtlHdr_t *)ulm_malloc(sizeof(quadricsCtlHdr_t));
    if (!hdr) {
        ReturnDescToPool(getMemPoolIndex());
        return;
    }
    if (usethreads())
        sfd = quadricsSendFragDescs.getElement(rail, returnValue);
    else
        sfd = quadricsSendFragDescs.getElementNoLock(rail, returnValue);
    if (returnValue != ULM_SUCCESS) {
        int dummyCode;
        free(hdr);
        path->cleanCtlMsgs(rail, timeNow, 0, (NUMBER_CTLMSGTYPES - 1), &dummyCode);
        ReturnDescToPool(getMemPoolIndex());
        return;
    }

    // how many buffers, of what type?
    switch (mtype) {
    case MSGTYPE_PT2PT:
    case MSGTYPE_PT2PT_SYNC:
        if (neededBytes <= quadricsBufSizes[PRIVATE_SMALL_BUFFERS]) {
            bufType = PRIVATE_SMALL_BUFFERS;
            naddrsNeeded = 1;
        }
        else {
            bufType = PRIVATE_LARGE_BUFFERS;
            naddrsNeeded = (neededBytes + quadricsBufSizes[bufType] - 1)/
                quadricsBufSizes[bufType];
        }
        break;
    case MSGTYPE_COLL:
    case MSGTYPE_COLL_SYNC:
        if (neededBytes <= quadricsBufSizes[SHARED_SMALL_BUFFERS]) {
            bufType = SHARED_SMALL_BUFFERS;
            naddrsNeeded = 1;
        }
        else {
            bufType = SHARED_LARGE_BUFFERS;
            naddrsNeeded = (neededBytes + quadricsBufSizes[bufType] - 1)/
                quadricsBufSizes[bufType];
        }
        dev = &(SMPSharedMemDevs[getMemPoolIndex()]);
        break;
    }

    /* prefetch destination buffers if we are not ACK'ing -- since we
     * can not reuse these buffers
     */
    if (!quadricsDoAck) {
        naddrsNeeded *= MEMREQ_PREFETCH_FACTOR;
    }

    if (naddrsNeeded > MEMREQ_MAX_WORDPTRS)
        naddrsNeeded = MEMREQ_MAX_WORDPTRS;

    // allocate the appropriate process-private
    // or shared memory (hopefully elan-addressable)
    for (int i = 0; i < naddrsNeeded; i++) {
        if ((mtype == MSGTYPE_PT2PT) || (mtype == MSGTYPE_PT2PT_SYNC)) {
            // elan-address process private memory
            addrs[i] = elan3_allocMain(ctx, E3_BLK_ALIGN,
                                       quadricsBufSizes[bufType]);
            if (addrs[i])
                naddrs++;
            else {
                status = (naddrs == 0) ? MEMREQ_FAILURE_TMP : status;
                break;
            }
        }
        else {
            // shared memory pool -- hopefully elan-addressable...could add check...
            addrs[i] = allocPayloadBuffer(dev, quadricsBufSizes[bufType],
                                          &returnValue, getMemPoolIndex());;
            if (addrs[i] != (void *) -1L) {
                naddrs++;
            }
            else {
                if ((returnValue == ULM_ERR_OUT_OF_RESOURCE) ||
                    (returnValue == ULM_ERR_FATAL)) {
                    status = MEMREQ_FAILURE_FATAL;
                }
                else if (naddrs == 0) {
                    status = MEMREQ_FAILURE_TMP;
                }
                break;
            }
        }
    }

    // send the memory request results
    hdr->memRequestAck.requestStatus = status;
    hdr->memRequestAck.memBufCount = naddrs;
    hdr->memRequestAck.memBufSize = quadricsBufSizes[bufType] * naddrs;
    hdr->memRequestAck.memType = bufType;
    hdr->memRequestAck.memNeededSeqOffset = p->memNeededSeqOffset;
    hdr->memRequestAck.sendMessagePtr = p->sendMessagePtr;
    for (int i = 0; i < naddrs; i++) {
        // this translation works because all process/Elan combos
        // have the same virtual memory mappings
        hdr->memRequestAck.memBufPtrs.wordPtrs[i] =
            (ulm_uint32_t)elan3_main2elan(ctx, addrs[i]);
    }

    // use incoming ctx and rail!
    sfd->init(0,
              ctx,
              MEMORY_REQUEST_ACK,
              rail,
              p->senderID,
              -1,
              0,
              0,
              0,
              0,
              0,
              false,
              hdr);
    // make sure that this message is not sent
    // over another rail...
    sfd->freeWhenDone = true;

    list = &(quadricsQueue[rail].ctlMsgsToSend[MEMORY_REQUEST_ACK]);
    if (usethreads()) {
        list->Lock.lock();
        list->AppendNoLock((Links_t *)sfd);
        quadricsQueue[rail].ctlFlagsLock.lock();
        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MEMORY_REQUEST_ACK);
        quadricsQueue[rail].ctlFlagsLock.unlock();
        list->Lock.unlock();
    }
    else {
        list->AppendNoLock((Links_t *)sfd);
        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MEMORY_REQUEST_ACK);
    }

    path->sendCtlMsgs(rail, timeNow, MEMORY_REQUEST_ACK, MEMORY_REQUEST_ACK, &errorCode);
    path->cleanCtlMsgs(rail, timeNow, MEMORY_REQUEST_ACK, MEMORY_REQUEST_ACK, &errorCode);

    ReturnDescToPool(getMemPoolIndex());
    return;
}


void quadricsRecvFragDesc::memReqAck()
{
    quadricsMemReqAck_t *p = &(envelope.memRequestAck);
    void *addrs[MEMREQ_MAX_WORDPTRS];

    switch (p->requestStatus) {
    case MEMREQ_SUCCESS:
        // copy the 32-bit Elan addresses into a
        // void *array; a (void *) may be larger...
        for (int i = 0; i < p->memBufCount; i++) {
            addrs[i] = (void *)(p->memBufPtrs.wordPtrs[i]);
        }
        // push the addresses into quadricsPeerMemory
        // tracking structure
        quadricsPeerMemory[rail]->push((int)p->senderID, (int)p->memType, addrs, (int)p->memBufCount);
        break;
    case MEMREQ_FAILURE_FATAL:
        ulm_exit((-1, "quadricsRecvFragDesc::memReqAck() fatal "
                  "memory request error from global"
                  " process %d\n", p->senderID));
        break;
    case MEMREQ_FAILURE_TMP:
    default:
        break;
    }

    ReturnDescToPool(getMemPoolIndex());
    return;
}
