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

#include <strings.h>			// for bzero

#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/state.h"
#include "path/ib/path.h"
#include "client/ULMClient.h"

// all HCAs version
bool ibPath::sendCtlMsgs(double timeNow, int startIndex, int endIndex, int *errorCode)
{
    bool result = true;

    *errorCode = ULM_SUCCESS;

    if (timeNow == -1.0)
        timeNow = dclock();

    // try to send on all HCAs
    for (int i = 0; i < ib_state.num_active_hcas; i++) {
        int j = ib_state.active_hcas[i];
        if ((ib_state.hca[j].ctlMsgsToSendFlag & ( ((1 << (endIndex + 1)) - 1) &
                                                   ~((1 << startIndex) - 1) )) != 0)
            result = (sendCtlMsgs(j, timeNow, startIndex, endIndex, errorCode, true) && result);
    }
    return result;
}

bool ibPath::sendCtlMsgs(int hca_index, double timeNow, int startIndex, 
int endIndex, int *errorCode, bool skipCheck)
{
    ibSendFragDesc *sfd, *afd;
    ib_hca_state_t *p = &(ib_state.hca[hca_index]);
    ProcessPrivateMemDblLinkList *slist, *alist;
    int i;
    unsigned int mask;

    if (!skipCheck) {
        if ((p->ctlMsgsToSendFlag == 0) || ((p->ctlMsgsToSendFlag & ( ((1 << (endIndex + 1)) - 1) &
                                                                     ~((1 << startIndex) - 1))) == 0))
            return true;
    }

    if (timeNow == -1.0)
        timeNow = dclock();

    for (i = startIndex ; i <= endIndex; i++) {

        mask = (1 << i);

        if ((p->ctlMsgsToSendFlag & mask) == 0)
            continue;

        slist = &(p->ctlMsgsToSend[i]);
        alist = &(p->ctlMsgsToAck[i]);

        if (usethreads()) {
            ib_state.lock.lock();
        }

        for (sfd = (ibSendFragDesc *)slist->begin();
             sfd != (ibSendFragDesc *)slist->end();
             sfd = (ibSendFragDesc *)sfd->next) {

            bool OKToSend = true;

            if ((sfd->flags & IB_INIT_COMPLETE) == 0) {
                // we've already saved the frag desc.'s parameters,
                // call init() with default parameters
                OKToSend = sfd->init();
            }

            if (OKToSend) {
                /* post send request */
                if (sfd->enqueue(timeNow, errorCode)) {
#ifdef ENABLE_RELIABILITY
                    sfd->timeSent = timeNow;
                    (sfd->numTransmits)++;
#endif
                    // switch control message to ack list and remove from send list
                    afd = sfd;
                    afd->WhichQueue = IBFRAGSTOACK;
                    sfd = (ibSendFragDesc *)slist->RemoveLinkNoLock(afd);
                    alist->AppendNoLock(afd);
                }
                else if (*errorCode == ULM_ERR_BAD_SUBPATH) {
                    // mark this HCA port as bad, if it not already, and
                    // rebind this frag to another HCA if possible
                    // or simply return ULM_ERR_BAD_PATH to force path rebinding
                    // which is the default for now...
                    *errorCode == ULM_ERR_BAD_PATH;
                    if (usethreads()) {
                        ib_state.lock.unlock();
                    }
                    return false;
                }
            }
        }

        if (slist->size() == 0)
            p->ctlMsgsToSendFlag &= ~mask;
        else
            p->ctlMsgsToSendFlag |= mask;

        if (alist->size() == 0)
            p->ctlMsgsToAckFlag &= ~mask;
        else
            p->ctlMsgsToAckFlag |= mask;

        if (usethreads()) {
            ib_state.lock.unlock();
        }
    }
    return true;
}

// all HCAs version
bool ibPath::cleanCtlMsgs(double timeNow, int startIndex, int endIndex, int *errorCode)
{
    bool result = true;

    *errorCode = ULM_SUCCESS;

    if (timeNow == -1.0)
        timeNow = dclock();

    // try to clean all HCAs
    for (int i = 0; i < ib_state.num_active_hcas; i++) {
        int j = ib_state.active_hcas[i];
        if ((ib_state.hca[j].ctlMsgsToAckFlag & ( ((1 << (endIndex + 1)) - 1) &
                                                  ~((1 << startIndex) - 1) )) != 0)
            result = (cleanCtlMsgs(j, timeNow, startIndex, endIndex, errorCode, true) && result);
    }
    return result;
}

bool ibPath::cleanCtlMsgs(int hca_index, double timeNow, int startIndex, 
int endIndex, int *errorCode, bool skipCheck)
{
    ibSendFragDesc *sfd, *afd;
    ib_hca_state_t *p = &(ib_state.hca[hca_index]);
    ProcessPrivateMemDblLinkList *list;
    int i;
    unsigned int mask;

    if (!skipCheck) {
        if ((p->ctlMsgsToAckFlag == 0) || ((p->ctlMsgsToAckFlag & ( ((1 << (endIndex + 1)) - 1) &
                                                                   ~((1 << startIndex) - 1))) == 0))
            return true;
    }

    if (timeNow == -1.0)
        timeNow = dclock();

    for (i = startIndex ; i <= endIndex; i++) {

        mask = (1 << i);

        if ((p->ctlMsgsToAckFlag & mask) == 0)
            continue;

        list = &(p->ctlMsgsToAck[i]);

        if (usethreads()) {
            ib_state.lock.lock(); 
        }

        for (sfd = (ibSendFragDesc *)list->begin();
             sfd != (ibSendFragDesc *)list->end();
             sfd = (ibSendFragDesc *)sfd->next) {

            if (sfd->done(timeNow, errorCode)) {
                // remove from ack list and free...
                afd = sfd;
                // free resources associated with the frag send desc.
                afd->freeRscs();
                afd->WhichQueue = IBFRAGFREELIST;
                sfd = (ibSendFragDesc *)list->RemoveLinkNoLock(afd);
                ibSendFragDescs.returnElementNoLock(afd, afd->hca_index);
                }
            }
            else if (*errorCode == ULM_ERR_BAD_SUBPATH) {
                // mark this HCA as bad, if it not already, and
                // rebind this frag to another HCA if possible
                // or simply return ULM_ERR_BAD_PATH to force path rebinding
                // which is the default for now...
                *errorCode == ULM_ERR_BAD_PATH;
                if (usethreads()) {
                    ib_state.lock.unlock();
                }
                return false;
            }
        }

        if (list->size() == 0)
            p->ctlMsgsToAckFlag &= ~mask;
        else
            p->ctlMsgsToAckFlag |= mask;

        if (usethreads()) {
            ib_state.lock.unlock();
        }
    }
    return true;
}

bool ibPath::sendMemoryRequest(SendDesc_t *message, int gldestProc, size_t offset,
                                     size_t memNeeded, int *errorCode)
{
    bool result = true;
    return result;
}

bool ibPath::send(SendDesc_t *message, bool *incomplete, int *errorCode)
{
    ssize_t offset = 0, leftToSend = 0, fragLength = 0;
    ibSendFragDesc *sfd = 0, *afd;
    void *dest = 0;
    int returnValue = ULM_SUCCESS;
    int tmap_index, rail;
    int gldestProc = -1;
    double timeNow = -1;
    int rc;

    *errorCode = ULM_SUCCESS;
    *incomplete = true;

    /* Do we have any completed frags that can be freed (if we are not waiting for ACKs)? */
    if (!ib_state.ack && message->FragsToAck.size()) {
        if (timeNow < 0)
            timeNow = dclock();
        sendDone(message, timeNow, errorCode);
    }

    if (message->NumFragDescAllocated == 0) {
        message->pathInfo.ib.allocated_offset_m = 0;
    }

    /* create and init frags with all the necessary resources */
    do {

        // slow down the send -- always initialize and send the first frag
        if (message->NumFragDescAllocated != 0) {
            if ((ib_state.max_outst_frags != -1) && 
                (message->NumFragDescAllocated -
                message->NumAcked >= (unsigned int)ib_state.max_outst_frags)) {
                message->clearToSend_m = false;
            }
            // Request To Send/Clear To Send - always send the first frag
            if (!message->clearToSend_m) {
                break;
            }
        }

        // get send fragment descriptor
        if (usethreads()) {
            sfd = ib_state.send_frag_list.getElement(0, returnValue);
        }
        else {
            sfd = ib_state.send_frag_list.getElementNoLock(0, returnValue);
        }
        if (returnValue != ULM_SUCCESS) {
            break;
        }

        // calculate tmap_index for non-zero non-contiguous data
        if (message->datatype == NULL || message->datatype->layout == CONTIGUOUS) {
            // an invalid array index is used to indicate that no packedData buffer
            // is needed
            tmap_index = -1;
        }
        else {
            int dtype_cnt = offset / message->datatype->packed_size;
            size_t data_copied = dtype_cnt * message->datatype->packed_size;
            ssize_t data_remaining = (ssize_t)(offset - data_copied);
            tmap_index = message->datatype->num_pairs - 1;
            for (int ti = 0; ti < message->datatype->num_pairs; ti++) {
                if (message->datatype->type_map[ti].seq_offset == data_remaining) {
                    tmap_index = ti;
                    break;
                } else if (message->datatype->type_map[ti].seq_offset > data_remaining) {
                    tmap_index = ti - 1;
                    break;
                }
            }
        }

        // initialize descriptor...does almost everything if it can...
        sfd->init(
            message,
            hca_index,
            MESSAGE_DATA,
            gldestProc,
            tmap_index,
            offset,
            fragLength,
            );

        // put on the FragsToSend list -- may not be able to send immediately
        sfd->WhichQueue = IBFRAGSTOSEND;
        message->FragsToSend.AppendNoLock(sfd);
        (message->NumFragDescAllocated)++;

    } while (message->pathInfo.ib.allocated_offset_m < message->posted_m.length_m);

    /* send list -- finish initialization, enqueue DMA, and move to frag ack list if successful */

    if ((timeNow < 0) && message->FragsToSend.size())
        timeNow = dclock();

    for (sfd = (quadricsSendFragDesc *) message->FragsToSend.begin();
         sfd != (quadricsSendFragDesc *) message->FragsToSend.end();
         sfd = (quadricsSendFragDesc *) sfd->next) {
        bool OKToSend = true;

        if ((sfd->flags & QSF_INIT_COMPLETE) == 0) {
            // we've already saved the frag desc.'s parameters,
            // call init() with default parameters
            OKToSend = sfd->init();
        }

        if (OKToSend) {
            /* enqueue DMA */
            if (sfd->enqueue(timeNow, errorCode)) {
#ifdef ENABLE_RELIABILITY
                sfd->timeSent = timeNow;
                (sfd->numTransmits)++;
                unsigned long long max_multiple =
                    (sfd->numTransmits < MAXRETRANS_POWEROFTWO_MULTIPLE) ?
                    (1 << sfd->numTransmits) :
                    (1 << MAXRETRANS_POWEROFTWO_MULTIPLE);
                double timeToResend = sfd->timeSent + (RETRANS_TIME * max_multiple);
                if (message->earliestTimeToResend == -1) {
                    message->earliestTimeToResend = timeToResend;
                } else if (timeToResend < message->earliestTimeToResend) {
                    message->earliestTimeToResend = timeToResend;
                }

#endif
                // switch frag to ack list and remove from send list
                afd = sfd;
                afd->WhichQueue = QUADRICSFRAGSTOACK;
                sfd = (quadricsSendFragDesc *) message->FragsToSend.RemoveLinkNoLock(afd);
                message->FragsToAck.AppendNoLock(afd);
                (message->NumSent)++;
            }
            else if (*errorCode == ULM_ERR_BAD_SUBPATH) {
                // mark this rail as bad, if it is not already, and
                // rebind this frag to another rail if possible
                // or simply return ULM_ERR_BAD_PATH to force path rebinding
                ELAN3_CTX *newctx;
                int newrail;
                void *newdest = 0;
                bool needDest = (sfd->destAddr) ? true : false;
                quadricsQueue[sfd->rail].railOK = false;
                if (getCtxRailAndDest(message, sfd->globalDestID, &newctx, &newrail,
                                      &newdest, sfd->destBufType, needDest, errorCode)) {
                    if (!needDest || (needDest && newdest)) {
                        sfd->reinit(newctx, newrail, newdest);
                    }
                }
                else if (*errorCode == ULM_ERR_BAD_PATH) {
                    return false;
                }
            }
        }
    }

    /* is everything done? */
    if (!message->messageDone) {

        // check to see if send is done

        enum { SEND_COMPLETION_WAIT_FOR_ACK = 0 };
        bool send_done_now = false;
        
        if (quadricsDoAck) {
            if (SEND_COMPLETION_WAIT_FOR_ACK) {
                // only zero length messages can free the user data buffer now
                send_done_now =
                    (message->posted_m.length_m == 0) &&
                    (message->NumSent == message->numfrags) &&
                    (message->sendType != ULM_SEND_SYNCHRONOUS);
            } else {
                // messages which fit in the control header can free the
                // user data buffer now as long as control header
                // initialization has been completed
                send_done_now =
                    (message->posted_m.length_m <= CTLHDR_DATABYTES) &&
                    (message->NumFragDescAllocated == message->numfrags) &&
                    (message->sendType != ULM_SEND_SYNCHRONOUS);
                if (send_done_now) {
                    // check that the control header has been initialized
                    if (message->FragsToSend.size()) {
                        sfd = (quadricsSendFragDesc *) message->FragsToSend.begin();
                        if (!(sfd->flags & QSF_INIT_COMPLETE)) {
                            send_done_now = false;
                        }
                    }
                }
            }
        }
        else {
            if (timeNow < 0)
                timeNow = dclock();
            send_done_now = sendDone(message, timeNow, errorCode);
        }

        if (send_done_now) {
    	    message->messageDone = REQUEST_COMPLETE;
        }
    }

    if (message->NumSent == message->numfrags) {
        *incomplete = false;
    }

    return true;
}

bool quadricsPath::push(double timeNow, int *errorCode)
{
    bool result = true;

    if ((timeNow - quadricsLastMemRls) >= QUADRICS_MEMRLS_TIMEOUT) {
        quadricsLastMemRls = timeNow;
        result = releaseMemory(timeNow, errorCode);
    }

    result = (sendCtlMsgs(timeNow, 0, (NUMBER_CTLMSGTYPES - 1), errorCode) && result);
    result = (cleanCtlMsgs(timeNow, 0, (NUMBER_CTLMSGTYPES - 1), errorCode) && result);

    return result;
}

bool quadricsPath::needsPush(void)
{
    bool result = false;

    for (int i = 0; i < quadricsNRails; i++) {
        if (quadricsQueue[i].railOK &&
            (quadricsQueue[i].ctlMsgsToSendFlag || quadricsQueue[i].ctlMsgsToAckFlag)) {
            result = true;
            break;
        }
    }

    return result;
}

#ifndef offsetof
#define offsetof(T,F)   ((int)&(((T *)0)->F))
#endif

bool quadricsPath::receive(double timeNow, int *errorCode, recvType recvTypeArg)
{
    quadricsRecvFragDesc *rd;
    int returnValue;
    unsigned int chksum, qstate;
    ELAN3_CTX *ctx;

    *errorCode = ULM_SUCCESS;

    for (int i = 0; i < quadricsNRails; i++) {
        if (EVENT_BLK_READY(quadricsQueue[i].rcvBlk)) {
            // lock queue if necessary
            if (usethreads()) {
                quadricsQueue[i].rcvLock.lock();
                if (!EVENT_BLK_READY(quadricsQueue[i].rcvBlk)) {
                    // another thread has already done the work...move along...
                    quadricsQueue[i].rcvLock.unlock();
                    continue;
                }
            }

            ctx = quadricsQueue[i].ctx;

            do {
                // retrieve and process entries...if
                // 1) we can get a quadricsRecvFragDesc descriptor
                // 2) and the quadricsCtlHdr_t checksum is OK
                rd = quadricsRecvFragDescs.getElement(getMemPoolIndex(), returnValue);
                if (returnValue != ULM_SUCCESS) {
                    if ((returnValue == ULM_ERR_OUT_OF_RESOURCE) ||
                        (returnValue == ULM_ERR_FATAL)) {
                        *errorCode = returnValue;
                        if (usethreads())
                            quadricsQueue[i].rcvLock.unlock();

                        return false;
                    }
                    if (usethreads())
                        quadricsQueue[i].rcvLock.unlock();
                    return true;
                }

                // copy and calculate checksum...if wanted...
                if (!quadricsDoChecksum) {
                    MEMCOPY_FUNC(quadricsQueue[i].q_fptr, &(rd->envelope), sizeof(quadricsCtlHdr_t));
                }
                else if (usecrc()) {
                    chksum = bcopy_uicrc(quadricsQueue[i].q_fptr, &(rd->envelope),
                        sizeof(quadricsCtlHdr_t), sizeof(quadricsCtlHdr_t));
                }
                else {
                    chksum = bcopy_uicsum(quadricsQueue[i].q_fptr, &(rd->envelope),
                        sizeof(quadricsCtlHdr_t), sizeof(quadricsCtlHdr_t));
                }

                // update main memory q_fptr
                quadricsQueue[i].q_fptr += sizeof(quadricsCtlHdr_t);

                // wrap pointer if needed
                if (quadricsQueue[i].q_fptr > quadricsQueue[i].q_top)
                    quadricsQueue[i].q_fptr = quadricsQueue[i].q_base;

                // read q_state from ELAN
                if ( usethreads() )
                    quadricsState.quadricsLock.lock();

                qstate = elan3_read32_sdram(ctx->sdram, quadricsQueue[i].sdramQAddr +
                                            offsetof(E3_Queue, q_state));

                // write new q_fptr to ELAN
                elan3_write32_sdram(ctx->sdram, quadricsQueue[i].sdramQAddr +
                                    offsetof(E3_Queue, q_fptr),
                                    quadricsQueue[i].elanQueueSlots + (quadricsQueue[i].q_fptr -
                                                                       quadricsQueue[i].q_base));

                // clear queue full bit of q_state if previously set
                if (qstate & E3_QUEUE_FULL) {
                    elan3_write32_sdram(ctx->sdram, quadricsQueue[i].sdramQAddr +
                                        offsetof(E3_Queue, q_state), 0);
                }

                // reset event block
                E3_RESET_BCOPY_BLOCK(quadricsQueue[i].rcvBlk);
                    
                // reprime ELAN q_event with a wait
                elan3_waitevent(ctx, quadricsQueue[i].sdramQAddr +
                                offsetof(E3_Queue, q_event));

                if ( usethreads() )
                    quadricsState.quadricsLock.unlock();
                
                // now process the received data so that when we are
                // done the ELAN will have had the time to reset the
                // receive event block if more data has arrived...

                if (!quadricsDoChecksum || (!usecrc() && (chksum == (unsigned int)(rd->envelope.commonHdr.checksum +
                    rd->envelope.commonHdr.checksum))) || (usecrc() && (chksum == 0))) {
                    // checksum OK (or irrelevant), process quadricsRecvFragDesc...
                    rd->ctx = ctx;
                    rd->rail = i;
                    rd->DataOK = false;
                    rd->path = this;

                    switch (rd->envelope.commonHdr.type) {
                    case MESSAGE_DATA:
                        rd->msgData(timeNow);
                        break;
                    case MESSAGE_DATA_ACK:
                        rd->msgDataAck(timeNow);
                        break;
                    case MEMORY_RELEASE:
                        rd->memRel();
                        break;
                    case MEMORY_REQUEST:
                        rd->memReq(timeNow);
                        break;
                    case MEMORY_REQUEST_ACK:
                        rd->memReqAck();
                        break;
                    default:
                        ulm_exit((-1, "quadricsPath::receive bad type %d\n",
                                  (int)rd->envelope.commonHdr.type));
                        break;
                    }
                }
                else {
                    // checksum BAD, then abort or return descriptor to pool if
                    // ENABLE_RELIABILITY is defined
#ifdef ENABLE_RELIABILITY
                    if (quadricsDoAck) {
                        if (usecrc()) {
                            ulm_warn(("quadricsPath::receive - bad envelope CRC %u (envelope + CRC = %u)\n",
                                rd->envelope.commonHdr.checksum, chksum));
                        }
                        else {
                            ulm_warn(("quadricsPath::receive - bad envelope checksum %u, "
                                "calculated %u != 2*received %u\n",
                                rd->envelope.commonHdr.checksum,
                                chksum,
                                (rd->envelope.commonHdr.checksum +
                                rd->envelope.commonHdr.checksum)));
                        }
                        rd->ReturnDescToPool(getMemPoolIndex());
                        continue;
                    }
                    else {
                        if (usecrc()) {
                            ulm_exit((-1, "quadricsPath::receive - bad envelope CRC %u (envelope + CRC = %u)\n",
                                rd->envelope.commonHdr.checksum, chksum));
                        }
                        else {
                            ulm_exit((-1, "quadricsPath::receive - bad envelope checksum %u, "
                                "calculated %u != 2*received %u\n",
                                rd->envelope.commonHdr.checksum,
                                chksum,
                                (rd->envelope.commonHdr.checksum +
                                rd->envelope.commonHdr.checksum)));
                        }
                    }
#else
                    if (usecrc()) {
                        ulm_exit((-1, "quadricsPath::receive - bad envelope CRC %u (envelope + CRC = %u)\n",
                            rd->envelope.commonHdr.checksum, chksum));
                    }
                    else {
                        ulm_exit((-1, "quadricsPath::receive - bad envelope checksum %u, "
                              "calculated %u != 2*received %u\n",
                              rd->envelope.commonHdr.checksum,
                              chksum,
                              (rd->envelope.commonHdr.checksum +
                               rd->envelope.commonHdr.checksum)));
                    }
#endif
                }
            } while (EVENT_BLK_READY(quadricsQueue[i].rcvBlk));


            // unlock the queue...
            if (usethreads())
                quadricsQueue[i].rcvLock.unlock();
        }
    }
    return true;
}

#ifdef ENABLE_RELIABILITY

bool quadricsPath::resend(SendDesc_t *message, int *errorCode)
{
    bool returnValue = false;

    // move the timed out frags from FragsToAck back to
    // FragsToSend
    quadricsSendFragDesc *FragDesc = 0;
    quadricsSendFragDesc *TmpDesc = 0;
    double curTime = 0;

    *errorCode = ULM_SUCCESS;

    // reset send descriptor earliestTimeToResend
    message->earliestTimeToResend = -1;

    for (FragDesc = (quadricsSendFragDesc *) message->FragsToAck.begin();
	 FragDesc != (quadricsSendFragDesc *) message->FragsToAck.end();
	 FragDesc = (quadricsSendFragDesc *) FragDesc->next) {

	// reset TmpDesc
	TmpDesc = 0;

	// obtain current time
	curTime = dclock();

	// retrieve received_largest_inorder_seq
	unsigned long long received_seq_no, delivered_seq_no;

	received_seq_no = reliabilityInfo->sender_ackinfo[getMemPoolIndex()].process_array
		[FragDesc->globalDestID].received_largest_inorder_seq;
	delivered_seq_no = reliabilityInfo->sender_ackinfo[getMemPoolIndex()].process_array
		[FragDesc->globalDestID].delivered_largest_inorder_seq;

	bool free_send_resources = false;

	// move frag if timed out and not sitting at the
	// receiver
	if (delivered_seq_no >= FragDesc->frag_seq) {
	    // an ACK must have been dropped somewhere along the way...or
	    // it hasn't been processed yet...
	    FragDesc->WhichQueue = QUADRICSFRAGFREELIST;
	    TmpDesc = (quadricsSendFragDesc *) message->FragsToAck.RemoveLinkNoLock(FragDesc);
	    // set frag_seq value to 0/null/invalid to detect duplicate ACKs
	    FragDesc->frag_seq = 0;
	    // reset send descriptor pointer
	    FragDesc->parentSendDesc = 0;
	    // free all of the other resources after we unlock the frag
	    free_send_resources = true;
	} else if (received_seq_no < FragDesc->frag_seq) {
	    unsigned long long max_multiple = (FragDesc->numTransmits < MAXRETRANS_POWEROFTWO_MULTIPLE) ?
		(1 << FragDesc->numTransmits) : (1 << MAXRETRANS_POWEROFTWO_MULTIPLE);
	    if ((curTime - FragDesc->timeSent) >= (RETRANS_TIME * max_multiple)) {
		// resend this frag...
		returnValue = true;
		FragDesc->WhichQueue = QUADRICSFRAGSTOSEND;
		TmpDesc = (quadricsSendFragDesc *) message->FragsToAck.RemoveLinkNoLock(FragDesc);
		message->FragsToSend.AppendNoLock(FragDesc);
                (message->NumSent)--;
                FragDesc=TmpDesc;
		continue;
	    }
	} else {
	    // simply recalculate the next time to look at this send descriptor for retransmission
	    unsigned long long max_multiple = (FragDesc->numTransmits < MAXRETRANS_POWEROFTWO_MULTIPLE) ?
		(1 << FragDesc->numTransmits) : (1 << MAXRETRANS_POWEROFTWO_MULTIPLE);
	    double timeToResend = FragDesc->timeSent + (RETRANS_TIME * max_multiple);
	    if (message->earliestTimeToResend == -1) {
                message->earliestTimeToResend = timeToResend;
            } else if (timeToResend < message->earliestTimeToResend) {
                message->earliestTimeToResend = timeToResend;
            }
	}

	if (free_send_resources) {
	    message->clearToSend_m=true;
	    (message->NumAcked)++;

	    // free send resources
	    FragDesc->freeRscs();
        FragDesc->WhichQueue = QUADRICSFRAGFREELIST;

	    // return frag descriptor to free list
            //   the header holds the global proc id
	    if (usethreads()) {
		quadricsSendFragDescs.returnElement(FragDesc, FragDesc->rail);
            } else {
		quadricsSendFragDescs.returnElementNoLock(FragDesc, FragDesc->rail);
	    }
        }
	// reset FragDesc to previous value, if appropriate, to iterate over list correctly...
	if (TmpDesc) {
	    FragDesc = TmpDesc;
	}
    } // end FragsToAck frag descriptor loop

    return returnValue;
}

#endif

bool quadricsPath::releaseMemory(double timeNow, int *errorCode) {
    quadricsSendFragDesc *sfd = 0;
    quadricsCtlHdr_t *hdr = 0;
    ProcessPrivateMemDblLinkList *list;
    ELAN3_CTX *ctx;
    bool needSndRscs = true;
    int procs = nprocs();
    int rail, returnValue, destID, addrCnt;
    int bufCounts[NUMBER_BUFTYPES];
    void *addrs[MEMRLS_MAX_WORDPTRS];

    for (int j = 1; j <= quadricsNRails; j++) {
        rail = (quadricsLastMemRlsRail + j) % quadricsNRails;
        ctx = quadricsQueue[rail].ctx;

        if (!quadricsQueue[rail].railOK)
            continue;

        for (int i = 1; i <= procs; i++) {
            destID = (quadricsLastMemRlsDest + i) % procs;
            quadricsPeerMemory[rail]->addrCounts(destID, bufCounts);
            for (int k = 0; k < NUMBER_BUFTYPES; k++) {

                if (bufCounts[k] == 0)
                    continue;

                if (needSndRscs) {
                    hdr = (quadricsCtlHdr_t *)quadricsHdrs.get();
                    if (!hdr) {
                        quadricsLastMemRlsRail = rail;
                        quadricsLastMemRlsDest = destID;
                        return true;
                    }

                    if (usethreads())
                        sfd = quadricsSendFragDescs.getElement(rail, returnValue);
                    else
                        sfd = quadricsSendFragDescs.getElementNoLock(rail, returnValue);
                    if (returnValue != ULM_SUCCESS) {
                        int dummyCode;
                        *errorCode = returnValue;
                        ulm_free(hdr);
                        quadricsLastMemRlsRail = rail;
                        quadricsLastMemRlsDest = destID;
                        cleanCtlMsgs(rail, timeNow, 0, (NUMBER_CTLMSGTYPES - 1), &dummyCode);
                        return ((returnValue == ULM_ERR_OUT_OF_RESOURCE) ||
                                (returnValue == ULM_ERR_FATAL)) ? false : true;
                    }

                    needSndRscs = false;
                }


                if ((addrCnt = quadricsPeerMemory[rail]->
                     popLRU(destID, k, MEMRLS_MAX_WORDPTRS, addrs)) > 0) {
                    hdr->memRelease.memType = k;
                    hdr->memRelease.memBufCount = addrCnt;
                    if (usethreads())
                        sndMemRlsSeqsLock.lock();
                    hdr->memRelease.releaseSeq = (quadricsMemRlsSeqs[destID])++;
                    if (usethreads())
                        sndMemRlsSeqsLock.unlock();
                    for (int m = 0; m < addrCnt; m++) {
                        hdr->memRelease.memBufPtrs.wordPtrs[m] =
                            (ulm_uint32_t)addrs[m];
                    }

                    sfd->init(
                        0,
                        ctx,
                        MEMORY_RELEASE,
                        rail,
                        destID,
                        -1,
                        0,
                        0,
                        0,
                        0,
                        0,
                        false,
                        hdr
                        );
                    // make sure this message is not sent over
                    // another rail...
                    sfd->freeWhenDone = true;

                    list = &(quadricsQueue[rail].ctlMsgsToSend[MEMORY_RELEASE]);

                    if (usethreads()) {
                        list->Lock.lock();
                        list->AppendNoLock((Links_t *)sfd);
                        quadricsQueue[rail].ctlFlagsLock.lock();
                        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MEMORY_RELEASE);
                        quadricsQueue[rail].ctlFlagsLock.unlock();
                        list->Lock.unlock();
                    }
                    else {
                        list->AppendNoLock((Links_t *)sfd);
                        quadricsQueue[rail].ctlMsgsToSendFlag |= (1 << MEMORY_RELEASE);
                    }

                    hdr = 0;
                    sfd = 0;
                    needSndRscs = true;


                }

            } // end iteration over memory types
        } // end iteration over destination processes

        if (hdr || sfd) {
            if (hdr) {
                ulm_free(hdr);
                hdr = 0;
            }
            if (sfd) {
                sfd->freeRscs();
                sfd->WhichQueue = QUADRICSFRAGFREELIST;
                if (usethreads()) {
                    quadricsSendFragDescs.returnElement(sfd, rail);
                } else {
                    quadricsSendFragDescs.returnElementNoLock(sfd, rail);
                }
                sfd = 0;
            }
            needSndRscs = true;
        }

        if (!sendCtlMsgs(rail, timeNow, MEMORY_RELEASE, MEMORY_RELEASE, errorCode)) {
            return false;
        } else if (!cleanCtlMsgs(rail, timeNow, MEMORY_RELEASE, MEMORY_RELEASE, errorCode)) {
            return false;
        }

    } // end iteration over rails

    quadricsLastMemRlsRail = rail;
    quadricsLastMemRlsDest = destID;
    return true;
}
