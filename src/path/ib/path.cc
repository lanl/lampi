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

#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/state.h"
#include "path/ib/path.h"
#include "path/ib/sendFrag.h"
#include "path/ib/recvFrag.h"
#include "path/ib/header.h"
#include "client/ULMClient.h"

bool ibPath::canReach(int globalDestProcessID)
{
    bool result = false;
    ib_ud_peer_info_t *info = 
        &(ib_state.ud_peers.info[ib_state.ud_peers.proc_entries * globalDestProcessID]);

    for (int i = 0; i < ib_state.ud_peers.proc_entries; i++) {
        if (PEER_INFO_IS_VALID(info[i])) {
            result = true;
            break;
        }
    }

    return result;
}

void ibPath::checkSendCQs(void)
{
    VAPI_ret_t vapi_result;
    VAPI_wc_desc_t wc_desc;
    int i;
    bool locked = false;

    for (i = 0; i < ib_state.num_active_hcas; i++) {
        ib_hca_state_t *h = &(ib_state.hca[ib_state.active_hcas[i]]);

        do {

            // poll completion queue
            vapi_result = VAPI_poll_cq(h->handle, h->send_cq, &wc_desc);
            if (vapi_result == VAPI_CQ_EMPTY) {
                break;
            }
            else if (vapi_result != VAPI_OK) {
                ulm_exit((-1, "ibPath::checkSendCQs VAPI_poll_cq() for HCA %d returned %s\n",
                    ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            }

            if (usethreads() && !locked) {
                ib_state.lock.lock();
                locked = true;
            }

            // increment CQ tokens
            (h->send_cq_tokens)++;

            // check for errors from the interface...currently just log them...<memory leak?>
            if (wc_desc.status != VAPI_SUCCESS) {
                ulm_err(("ibPath::checkSendCQs HCA %d cq entry status %d syndrome %u\n",
                    ib_state.active_hcas[i], wc_desc.status, wc_desc.vendor_err_syndrome));
                continue;
            }

            // find send frag descriptor and mark desc. as locally acked
            sd = (ibSendFragDesc *)wc_desc.id;
            sd->state_m |= ibSendFragDesc::LOCALACKED; 
            
            // increment UD QP tokens
            (h->ud.sq_tokens)++;

        } while (1);
    }

    if (locked) {
        ib_state.lock.unlock();
    }
}

bool ibPath::sendDone(SendDesc_t *message, double timeNow, int *errorCode)
{
    // check send completion queues of all HCAs for local send completion notification
    checkSendCQs();

    if (ib_state.ack) {
        if ((message->posted_m.length == message->pathInfo.ib.allocated_offset_m) && 
            (message->NumAcked >= message->numfrags)) {
            return true;
        }
        else {
            return false;
        }
    }
    else {
        ibSendFragDesc *sfd, *afd;

        if ((message->sendType != ULM_SEND_SYNCHRONOUS) || (message->NumSent > 1)) {
            if (timeNow < 0)
                timeNow = dclock();

            for (sfd = (ibSendFragDesc *)message->FragsToAck.begin();
                sfd != (ibSendFragDesc *)message->FragsToAck.end();
                sfd = (ibSendFragDesc *)sfd->next) {
                if (sfd->done(timeNow, errorCode)) {
                    // register frag as ACK'ed
                    message->clearToSend_m = true;
                    (message->NumAcked)++;
                    // remove frag from FragsToAck list
                    afd = sfd;
                    sfd = (ibSendFragDesc *)message->FragsToAck.RemoveLinkNoLock((Links_t *)afd);
                    afd->WhichQueue = IBFRAGFREELIST;
                    // return frag descriptor to free list
                    // the header holds the global proc id
                    if (usethreads()) {
                        ib_state.hca[afd->hca_index_m].send_frag_list.returnElement(afd, 0);
                    } else {
                        ib_state.hca[afd->hca_index_m].send_frag_list.returnElementNoLock(afd, 0);
                    }
                }
            }
        }

        if ((message->posted_m.length == message->pathInfo.ib.allocated_offset_m) && 
            (message->NumAcked >= message->numfrags)) {
            return true;
        }
        else {
            return false;
        }
    }
}

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
    ibSendFragDesc *sfd = 0, *afd;
    int returnValue = ULM_SUCCESS;
    int tmap_index;
    double timeNow = -1;
    int rc, i, hca_index, port_index;

    *errorCode = ULM_SUCCESS;
    *incomplete = true;

    /* Do we have any completed frags that can be freed (if we are not waiting for ACKs)? */
    if (!ib_state.ack && message->FragsToAck.size()) {
        if (timeNow < 0)
            timeNow = dclock();
        sendDone(message, timeNow, errorCode);
    }

    if (usethreads()) {
        ib_state.lock.lock();
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

        // find HCA to get send fragment descriptor...use round-robin processing 
        // of active HCAs and ports
        hca_index = -1;
        port_index = -1;
        for (i = 0; i < ib_state.num_active_hcas; i++) {
            int hca, j, k;
            j = i + ib_state.next_send_hca;
            j = (j >= ib_state.num_active_hcas) ? j - ib_state.num_active_hcas : j; 
            hca = ib_state.active_hcas[j];
            if ((ib_state.hca[hca].sq_tokens >= 1) && (ib_state.hca[hca].send_cq_tokens >= 1)) {
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
                (ib_state.hca[hca_index].sq_tokens)--;
                (ib_state.hca[hca_index].send_cq_tokens)--;
                break; 
            }
        }
        
        // stop allocation, if we don't have a valid hca_index or port_index
        if ((hca_index < 0) || (port_index < 0)) {
            break;
        }

        sfd = ib_state.hca[hca_index].send_frag_list.getElementNoLock(0, returnValue);
        if (returnValue != ULM_SUCCESS) {
            // should currently be impossible with just UD QP service since
            // all of the descriptors should already exist and be available
            if (usethreads()) {
                ib_state.lock.unlock();
            }
            *errorCode = returnValue;
            return false;
        }

        // initialize descriptor...does almost everything if it can...
        sfd->init(message, hca_index, port_index);

        // put on the FragsToSend list -- may not be able to send immediately
        sfd->WhichQueue = IBFRAGSTOSEND;
        message->FragsToSend.AppendNoLock(sfd);
        (message->NumFragDescAllocated)++;
        (message->numfrags)++;
    } while (message->pathInfo.ib.allocated_offset_m < message->posted_m.length_m);

    /* send list -- finish initialization, post request, and move to frag ack list if successful */

    if ((timeNow < 0) && message->FragsToSend.size())
        timeNow = dclock();

    for (sfd = (ibSendFragDesc *) message->FragsToSend.begin();
         sfd != (ibSendFragDesc *) message->FragsToSend.end();
         sfd = (ibSendFragDesc *) sfd->next) {
        bool OKToSend = true;

        if ((sfd->flags & ibSendFragDesc::IBINITCOMPLETE) == 0) {
            // we've already saved the frag desc.'s parameters,
            // call init() with default parameters
            OKToSend = sfd->init();
        }

        if (OKToSend) {
            /* post IB request */
            if (sfd->post(timeNow, errorCode)) {
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
                afd->WhichQueue = IBFRAGSTOACK;
                sfd = (quadricsSendFragDesc *) message->FragsToSend.RemoveLinkNoLock(afd);
                message->FragsToAck.AppendNoLock(afd);
                (message->NumSent)++;
            }
            else if (*errorCode == ULM_ERR_BAD_SUBPATH) {
                // mark this HCA as bad, if it is not already, and
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

    /* is everything done? */
    if (!message->messageDone) {

        // check to see if send is done

        enum { SEND_COMPLETION_WAIT_FOR_ACK = 0 };
        bool send_done_now = false;
        
        if (ib_state.ack) {
            if (SEND_COMPLETION_WAIT_FOR_ACK) {
                // with UD QP buffering we can free right away...later...
                // only zero length messages can free the user data buffer now
                send_done_now =
                    (message->posted_m.length_m == message->pathInfo.ib.allocated_offset_m) &&
                    (message->NumSent == message->numfrags) &&
                    (message->sendType != ULM_SEND_SYNCHRONOUS);
            } else {
                // same remark as above...will be changing
                send_done_now =
                    (message->posted_m.length_m == message->pathInfo.ib.allocated_offset_m) &&
                    (message->NumSent == message->numfrags) &&
                    (message->sendType != ULM_SEND_SYNCHRONOUS);
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

    if ((message->posted_m.length_m == message->pathInfo.ib.allocated_offset_m) &&
        (message->NumSent == message->numfrags)) {
        *incomplete = false;
    }

    return true;
}

bool ibPath::push(double timeNow, int *errorCode)
{
    bool result = true;

    result = (sendCtlMsgs(timeNow, 0, (NUMBER_CTLMSGTYPES - 1), errorCode) && result);
    result = (cleanCtlMsgs(timeNow, 0, (NUMBER_CTLMSGTYPES - 1), errorCode) && result);

    return result;
}

bool ibPath::needsPush(void)
{
    bool result = false;

    for (int i = 0; i < ib_state.num_active_hcas; i++) {
        int j = ib_state.active_hcas[i];
        if (ib_state.hca[j].ctlMsgsToSendFlag || ib_state.hca[j].ctlMsgsToAckFlag) {
            result = true;
            break;
        }
    }

    return result;
}

#ifndef offsetof
#define offsetof(T,F)   ((int)&(((T *)0)->F))
#endif

bool ibPath::receive(double timeNow, int *errorCode, recvType recvTypeArg)
{
    VAPI_ret_t vapi_result;
    VAPI_wc_desc_t wc_desc;
    ibRecvFragDesc *rd;
    int returnValue, msg_type;
    unsigned int computed_chksum, recvd_chksum;
    void *addr;
    bool locked = false;

    *errorCode = ULM_SUCCESS;

    for (int i = 0; i < ib_state.num_active_hcas; i++) {
        ib_hca_state_t *h = &(ib_state.hca[ib_state.active_hcas[i]]);

        do {
            // poll completion queue for receive
            vapi_result = VAPI_poll_cq(h->handle, h->recv_cq, &wc_desc);
            if (vapi_result == VAPI_CQ_EMPTY) {
                break;
            } 
            else if (vapi_result != VAPI_OK) {
                ulm_exit((-1, "ibPath::receive VAPI_poll_cq() for HCA %d returned %s\n",
                    ib_state.active_hcas[i], VAPI_strerror(vapi_result)));
            }

            if (usethreads() && !locked) {
                ib_state.lock.lock();
            }

            // increment CQ tokens
            (h->recv_cq_tokens)++;

            // check for errors from the interface...currently just log them...<memory leak?>
            if (wc_desc.status != VAPI_SUCCESS) {
                ulm_err(("ibPath::receive HCA %d cq entry status %d syndrome %u\n",
                    ib_state.active_hcas[i], wc_desc.status, wc_desc.vendor_err_syndrome));
                continue;
            }

            // find pointer to receive fragment descriptor...
            // for UD QP service, it's in the id of the wc structure returned
            rd = (ibRecvFragDesc *)wc_desc.id;

            // increment receive UD QP tokens
            (h->ud.rq_tokens)++;

            // find receive buffer address
            addr = (ibCtlHdr_t *)rd->sg_m[0].addr;

            // calculate msg_type from first 32-bit integer
            msg_type = (int)*((ulm_uint32_t *)addr);

            // calculate checksum...if wanted...
            if (ib_state.checksum) {
                unsigned long cksum_len;
                
                switch(msg_type) {
                    case MESSAGE_DATA:
                        cksum_len = sizeof(ibDataHdr_t) - sizeof(ulm_uint32_t);
                        recvd_chksum = ((ibDataHdr_t *)addr->checksum);
                        break;
                    case MESSAGE_DATA_ACK:
                        cksum_len = sizeof(ibDataAck_t) - sizeof(ulm_uint32_t);
                        recvd_chksum = ((ibDataAck_t *)addr->checksum);
                        break;
                    default:
                        ulm_warn(("ibPath::receive bad message type %d\n",msg_type));
                        rd->ReturnDescToPool(getMemPoolIndex());
                        continue;
                }
                     
                if (usecrc()) {
                    computed_chksum = uicrc(addr, cksum_len);
                }
                else {
                    computed_chksum = uicsum(addr, cksum_len);
                }
            }

            // now process the received data
            if (!ib_state.checksum || (computed_chksum == recvd_chksum)) {
                // checksum OK (or irrelevant), process ibRecvFragDesc...
                rd->DataOK = false;
                rd->path = this;

                switch (msg_type) {
                    case MESSAGE_DATA:
                        rd->msgData(timeNow);
                        break;
                    case MESSAGE_DATA_ACK:
                        rd->msgDataAck(timeNow);
                        break;
                }
            }
            else {
                // checksum BAD, then abort or return descriptor to pool if
                // ENABLE_RELIABILITY is defined
#ifdef ENABLE_RELIABILITY
                if (ib_state.ack) {
                    if (usecrc()) {
                        ulm_warn(("ibPath::receive - bad envelope CRC %u (computed %u)\n",
                            recvd_chksum, computed_chksum));
                    }
                    else {
                        ulm_warn(("ibPath::receive - bad envelope checksum %u "
                            "(computed %u)\n", recvd_chksum, computed_chksum));
                    }
                    // will repost descriptor...
                    rd->ReturnDescToPool(getMemPoolIndex());
                    continue;
                }
                else {
                    if (usecrc()) {
                        ulm_exit((-1, "ibPath::receive - bad envelope CRC %u (computed %u)\n",
                            recvd_chksum, computed_chksum));
                    }
                    else {
                        ulm_exit((-1, "ibPath::receive - bad envelope checksum %u "
                            "(computed %u)\n", recvd_chksum, computed_chksum));
                    }
                }
#else
                if (usecrc()) {
                    ulm_exit((-1, "ibPath::receive - bad envelope CRC %u (computed %u)\n",
                        recvd_chksum, computed_chksum));
                }
                else {
                    ulm_exit((-1, "ibPath::receive - bad envelope checksum %u "
                          "(computed %u)\n", recvd_chksum, computed_chksum));
                }
#endif
            }
        } while (1);
    }

    if (locked) {
        ib_state.lock.unlock();
    }

    return true;
}

#ifdef ENABLE_RELIABILITY

bool ibPath::resend(SendDesc_t *message, int *errorCode)
{
    bool returnValue = false;

    // move the timed out frags from FragsToAck back to
    // FragsToSend
    ibSendFragDesc *FragDesc = 0;
    ibSendFragDesc *TmpDesc = 0;
    double curTime = 0;
    int hca_index;

    *errorCode = ULM_SUCCESS;

    // reset send descriptor earliestTimeToResend
    message->earliestTimeToResend = -1;

    for (FragDesc = (ibSendFragDesc *) message->FragsToAck.begin();
	 FragDesc != (ibSendFragDesc *) message->FragsToAck.end();
	 FragDesc = (ibSendFragDesc *) FragDesc->next) {

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
	    FragDesc->WhichQueue = IBFRAGFREELIST;
	    TmpDesc = (ibSendFragDesc *) message->FragsToAck.RemoveLinkNoLock(FragDesc);
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
		FragDesc->WhichQueue = IBFRAGSTOSEND;
		TmpDesc = (ibSendFragDesc *) message->FragsToAck.RemoveLinkNoLock(FragDesc);
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
        FragDesc->WhichQueue = IBFRAGFREELIST;

	    // return frag descriptor to free list
            //   the header holds the global proc id
	    if (usethreads()) {
            ib_state.lock.lock();
	    }
		ib_state.hca[FragDesc->hca_index_m].send_frag_list.returnElementNoLock(FragDesc, 0);
	    if (usethreads()) {
            ib_state.lock.unlock();
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
