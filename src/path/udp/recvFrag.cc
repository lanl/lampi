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



#include <stdio.h>
#include <sys/time.h>		// for timeval
#include <strings.h>		// for bzero and bcopy
#include <netinet/in.h>
#include <errno.h>

#include "internal/log.h"
#include "internal/malloc.h"
#include "path/udp/recvFrag.h"
#include "path/udp/UDPNetwork.h"
#include "path/udp/state.h"
#include "path/udp/UDPEarlySend.h"
#include "queue/globals.h"	// for RecvFrag queues
#include "client/ULMClient.h"
#include "queue/globals.h"	// for getMemPoolIndex()
#include "util/MemFunctions.h"
//#include "path/sharedmem/SMPSharedMemGlobals.h"	// for SMPSharedMemDevs and alloc..
#include "path/udp/path.h"

#ifdef ENABLE_RELIABILITY
#include "queue/ReliabilityInfo.h"
#endif

//
// Initialize some externs in globals.h
//
unsigned long maxFragSize_g = UDP_MAX_DATA;
unsigned long maxShortPayloadSize_g = MaxShortPayloadSize;
unsigned long maxPayloadSize_g = maxFragSize_g - sizeof(udp_header);


//-----------------------------------------------------------------------------
//! Check (using the select system call) to see if any data has arrived.
//! If so call the appropriate functions to process the data.
//! The data may arrive on either of two ports: short or long.
//!
//! Returns the number of bytes read (including headers).
//-----------------------------------------------------------------------------
int udpRecvFragDesc::pullFrags(int &retVal)
{
    fd_set readmask;
    struct timeval t = { 0, 0 };
    udpRecvFragDesc *desc;
    int shortsock = UDPGlobals::UDPNet->getLocalSocket(true);
    int maxfdp1 = 1 + shortsock;

    retVal = ULM_SUCCESS;

    //
    // Check the short message queue.  This should be done repeatedly to
    // flush the buffer, in case multiple messages are present.
    //

    int bytesRecvd = 0;
    int count = 0;
    bool keepChecking = true;

    while (keepChecking) {
	FD_ZERO(&readmask);
	FD_SET(shortsock, &readmask);

	count = select(maxfdp1, &readmask, (fd_set *) 0, (fd_set *) 0, &t);

	if (count > 0 && FD_ISSET(shortsock, &readmask)) {
	    desc = (udpRecvFragDesc *) UDPRecvFragDescs.getElement(getMemPoolIndex(), retVal);
	    if (retVal != ULM_SUCCESS)
		return bytesRecvd;
	    desc->sockfd = shortsock;
	    desc->shortMsg = true;
	    desc->copyError = false;
	    desc->pt2ptNonContig = false;
	    desc->dataReadFromSocket = false;
	    bytesRecvd += desc->handleShortSocket();
            /* if udp channel is being used, always check for data on this
             *   path
             */
	} else {
	    keepChecking = false;
	}
    }

    //
    // Check the long message queue.  This should NOT be done repeatedly as
    // handleLongSocket() only peeks at the header.  The complete frag is
    // not read until CopyFunction() is called on the frag.
    //

    int longsock = UDPGlobals::UDPNet->getLocalSocket(false);
    maxfdp1 = 1 + longsock;

    if (usethreads()) {
        UDPGlobals::longMessageLock.lock();
    }

    while (UDPGlobals::checkLongMessageSocket) {
	FD_ZERO(&readmask);
	FD_SET(longsock, &readmask);

	count = select(maxfdp1, &readmask, (fd_set *) 0, (fd_set *) 0, &t);

	if (count > 0 && FD_ISSET(longsock, &readmask)) {
	    desc = (udpRecvFragDesc *) UDPRecvFragDescs.getElement(getMemPoolIndex(), retVal);
	    if (retVal != ULM_SUCCESS)
        {
            if (usethreads()) {
                UDPGlobals::longMessageLock.unlock();
            }
            return bytesRecvd;
        }
        UDPGlobals::checkLongMessageSocket = false;	// will be reset as necessary
	    desc->sockfd = longsock;
	    desc->shortMsg = false;
	    desc->copyError = false;
	    desc->pt2ptNonContig = false;
	    desc->dataReadFromSocket = false;
	    bytesRecvd += desc->handleLongSocket();
	}
	else {
	    break;
	}
    }

    if (usethreads()) {
        UDPGlobals::longMessageLock.unlock();
    }

    return bytesRecvd;
}


//-----------------------------------------------------------------------------
//! Check short message socket for incoming messages.  If the message is a
//! internal library command, handle it and recur in case another command
//! or user message is on the socket.
//!
//! Returns the length of the message read (includes the header size).
//-----------------------------------------------------------------------------
ssize_t udpRecvFragDesc::handleShortSocket()
{
    struct sockaddr_in cli_addr;
    struct msghdr msgHdr;
    struct iovec iov[2];
    bool reread = false;
    ssize_t count = 0;

    bzero((char *) &msgHdr, sizeof(msgHdr));
    msgHdr.msg_name = (caddr_t) & cli_addr;
    msgHdr.msg_namelen = sizeof(cli_addr);
    msgHdr.msg_iov = iov;
    msgHdr.msg_iovlen = 2;

    iov[0].iov_base = (char *) &header;
    iov[0].iov_len = sizeof(udp_header);;
    iov[1].iov_base = (char *) data;
    iov[1].iov_len = maxShortPayloadSize_g;;

    do {
    count = recvmsg(sockfd, &msgHdr, 0);
    
	if (count > 0) {
#ifdef HEADER_ON
	 long type = ulm_ntohi(header.msg.type);
#else
	    long type = header.msg.type;
#endif     
       	    dataReadFromSocket = true;
            reread = false;


            switch (type) {
            case UDP_MESSAGETYPE_MESSAGE:
                processMessage(header.msg);
                break;

            case UDP_MESSAGETYPE_ACK:
                processAck(header.ack);
                break;

            default:
                break;
            }
	}
	else {
            if ((count < 0) && (errno == EINTR)) {
                reread = true;
            }
            else {
                reread = false;
                // free the descriptor
                ReturnDescToPool(getMemPoolIndex());
                count = 0;
            }
	}
    } while (reread);

    return count;
}


//-----------------------------------------------------------------------------
//! Peek at the header on the long message socket.  Will return contents
//! of the header in this->header if there is a message on the port.
//!
//! Returns the number of bytes read, which should be equal to the header
//! size if there is a message waiting on the port.
//-----------------------------------------------------------------------------
ssize_t udpRecvFragDesc::handleLongSocket()
{
    struct sockaddr_in cli_addr;
    struct msghdr msgHdr;
    struct iovec iov[1];
    bool reread = false;
    ssize_t count = 0;

    bzero((char *) &msgHdr, sizeof(msgHdr));
    msgHdr.msg_name = (caddr_t) & cli_addr;
    msgHdr.msg_namelen = sizeof(cli_addr);
    msgHdr.msg_iov = iov;
    msgHdr.msg_iovlen = 1;

    iov[0].iov_base = (char *) &header;
    iov[0].iov_len = sizeof(udp_header);;


    do {
    count = recvmsg(sockfd, &msgHdr, MSG_PEEK);
    
	if (count > 0) {
#ifdef HEADER_ON
            long type = ulm_ntohi(header.msg.type);
#else
	    long type = header.msg.type;
#endif
	    dataReadFromSocket = false;
            reread = false;

            switch (type) {
            case UDP_MESSAGETYPE_MESSAGE:
                processMessage(header.msg);
                break;

            default:
                break;
            }
	}
	else {
            if ((count < 0) && (errno == EINTR)) {
                reread = true;
            }
            else {
                reread = false;
                // free the descriptor
                ReturnDescToPool(getMemPoolIndex());
                count = 0;
            }
	}
    } while (reread);

    return count;
}

#ifdef ENABLE_RELIABILITY
bool udpRecvFragDesc::isDuplicateCollectiveFrag()
{
    int source_box = global_proc_to_host(srcProcID_m);

    // duplicate/dropped message frag support only for those
    // communication paths that overwrite the fragment sequence number
    // (seq_m) value from its default constructor value of 0 -- valid
    // sequence numbers are from (1 .. (2^64 - 1)))

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
	}			// end duplicate frag processing
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
#endif

//-----------------------------------------------------------------------------
//! Process a message that has just arrived.
//! Returns true if the long message socket is clear for reading the next
//! datagram (i.e., the frag is part of a multicast message).
//-----------------------------------------------------------------------------
void udpRecvFragDesc::processMessage(udp_message_header & msg)
{
    //
    // initialize frag descriptor
    //
#ifdef HEADER_ON
    length_m            = ulm_ntohl(msg.length);
    fragIndex_m         = ulm_ntohi(msg.fragIndex_m);
    maxSegSize_m        = maxPayloadSize_g;
    msgLength_m         = ulm_ntohl(msg.messageLength);
    addr_m              = (shortMsg) ? data : 0;
    srcProcID_m         = ulm_ntohi(msg.src_proc);
    dstProcID_m         = ulm_ntohi(msg.dest_proc);
    tag_m               = ulm_ntohi(msg.tag_m);
    ctx_m               = EXTRACT_CTX(ulm_ntohi(msg.ctxAndMsgType));
    msgType_m           = EXTRACT_MSG_TYPE(ulm_ntohi(msg.ctxAndMsgType));
    isendSeq_m          = ulm_ntohl(msg.isendSeq_m);
    seq_m               = ulm_ntohl(msg.frag_seq);
    seqOffset_m         = dataOffset();
#else 
    length_m            = msg.length;
    fragIndex_m         = msg.fragIndex_m;
    maxSegSize_m        = maxPayloadSize_g;
    msgLength_m         = msg.messageLength;
    addr_m              = (shortMsg) ? data : 0;
    srcProcID_m         = msg.src_proc;
    dstProcID_m         = msg.dest_proc;
    tag_m               = msg.tag_m;
    ctx_m               = EXTRACT_CTX((msg.ctxAndMsgType));
    msgType_m           = EXTRACT_MSGTYPE((msg.ctxAndMsgType));
    isendSeq_m          = msg.isendSeq_m;
    seq_m               = msg.frag_seq;
    seqOffset_m         = dataOffset();
#endif 

#ifdef ENABLE_RELIABILITY
    isDuplicate_m = false;
#endif

    // make sure destination process is on this host
    int dest_proc = dstProcID_m;
    if (dest_proc != myproc()) {
	ulm_exit((-1,
                  "UDP datagram not sent to me %d, destination %d\n",
                  myproc(), dest_proc));
    }
    //
    // push this frag onto the correct list
    //
    if (msgType_m == MSGTYPE_PT2PT) {	// pt2pt
        dstProcID_m = communicators[ctx_m]->
            localGroup->mapGlobalProcIDToGroupProcID[dstProcID_m];
        srcProcID_m = communicators[ctx_m]->
            remoteGroup->mapGlobalProcIDToGroupProcID[srcProcID_m]; 

	communicators[ctx_m]->handleReceivedFrag((BaseRecvFragDesc_t *)this);    
    } else {
	ulm_exit((-1,
                  "UDP datagram with invalid message type %d\n",
                  msgType_m));
    }
    return;
}


//-----------------------------------------------------------------------------
//! Process an ack that has just arrived.
//-----------------------------------------------------------------------------
void udpRecvFragDesc::processAck(udp_ack_header & ack)
{
    // translate the ack header to host order
#ifdef HEADER_ON 
    ack.type 		= ulm_ntohi(ack.type);
    ack.ctxAndMsgType = ulm_ntohi(ack.ctxAndMsgType);
    ack.dest_proc 	= ulm_ntohi(ack.dest_proc);
    ack.src_proc 	= ulm_ntohi(ack.src_proc);
    ack.udpio 		= ulm_ntohp(ack.udpio);
    ack.single_mseq 	= ulm_ntohl(ack.single_mseq);
    ack.single_fseq 	= ulm_ntohl(ack.single_fseq);
    ack.single_fragseq = ulm_ntohl(ack.single_fragseq);
    ack.received_fragseq = ulm_ntohl(ack.received_fragseq);
    ack.delivered_fragseq = ulm_ntohl(ack.delivered_fragseq);
    ack.ack_or_nack 	= ulm_ntohi(ack.ack_or_nack);
#endif
    // setup pointer to fragment and send descriptor, so that after
    //  memory is freed, we still have valid pointers.
    //  volatile udpSendFragDesc* fragDesc = (udpSendFragDesc*) ack.udpio.ptr;
    udpSendFragDesc *Frag = (udpSendFragDesc *) ack.udpio.ptr;
    volatile BaseSendDesc_t *sendDesc = (volatile BaseSendDesc_t *) Frag->parentSendDesc;

	// lock frag through send descriptor to prevent two
	// ACKs from processing simultaneously

	if (sendDesc) {
	    ((BaseSendDesc_t *)sendDesc)->Lock.lock();
	    if (sendDesc != Frag->parentSendDesc) {
		((BaseSendDesc_t *)sendDesc)->Lock.unlock();
		ReturnDescToPool(getMemPoolIndex());
		return;
	    }
	} else {
	    ReturnDescToPool(getMemPoolIndex());
	    return;
	}

#ifdef ENABLE_RELIABILITY
	if (checkForDuplicateAndNonSpecificAck(Frag, ack)) {
	    ((BaseSendDesc_t *)sendDesc)->Lock.unlock();
	    ReturnDescToPool(getMemPoolIndex());
	    return;
	}
#endif

	handlePt2PtMessageAck((BaseSendDesc_t *) sendDesc, Frag, ack);
	((BaseSendDesc_t *)sendDesc)->Lock.unlock();
    ReturnDescToPool(getMemPoolIndex());
    return;
}

#ifdef ENABLE_RELIABILITY
bool udpRecvFragDesc::checkForDuplicateAndNonSpecificAck(udpSendFragDesc * Frag, udp_ack_header & ack)
{
    sender_ackinfo_control_t *sptr;
    sender_ackinfo_t *tptr;

    // update sender ACK info -- largest in-order delivered and received frag sequence
    // numbers received by our peers (or peer box, in the case of collective communication)
    // from ourself or another local process
    if (msgType_m == MSGTYPE_PT2PT) {
	sptr = &(reliabilityInfo->sender_ackinfo[global_to_local_proc(ack.dest_proc)]);
	tptr = &(sptr->process_array[ack.src_proc]);
    } else {
	ulm_err(("udpRecvFragDesc::check...Ack received ack of unknown communication type %d\n", msgType_m));
	return true;
    }

    sptr->Lock.lock();
    if (ack.delivered_fragseq > tptr->delivered_largest_inorder_seq) {
	tptr->delivered_largest_inorder_seq = ack.delivered_fragseq;
    }
    // received is not guaranteed to be a strictly increasing series...
    tptr->received_largest_inorder_seq = ack.received_fragseq;
    sptr->Lock.unlock();

    // check to see if this ACK is for a specific frag or not (== 0/null/invalid)
    // if not, then we don't need to do anything else...
    if (ack.single_fragseq == 0) {
	return true;
#ifdef HEADER_ON
    } else if ((Frag->frag_seq != ack.single_fragseq)
	       || ((ulm_int32_t)ulm_ntohi(Frag->header.src_proc) != ack.dest_proc)
	       || ((ulm_int32_t)ulm_ntohi(Frag->header.dest_proc) != ack.src_proc)
	       || ((ulm_int32_t)ulm_ntohi(Frag->header.ctxAndMsgType) != ack.ctxAndMsgType)) {
	// this ACK is a duplicate...or just screwed up...just ignore it...
#ifdef _DEBUG_RECVACK
	fprintf(stderr, "udpRecvFragDesc::checkForDuplicateAndNonSpecificAck() processed a bad/duplicate ACK...\n");
	fprintf(stderr,
		"Frag: seq=%lld, sproc=%d, dproc=%d, comm=%d\n",
		Frag->frag_seq,
		ulm_ntohi(Frag->header.src_proc), ulm_ntohi(Frag->header.dest_proc),
		ulm_ntohi(Frag->header.ctxAndMsgType));
	fprintf(stderr,
		"ACK: seq=%lld, sproc=%d, dproc=%d, comm=%d\n",
		ack.single_fragseq, ack.src_proc, ack.dest_proc, ack.ctxAndMsgType);
	fflush(stderr);
#endif				/* _DEBUG_RECVACK */
	return true;
    }
#else
  } else if ((Frag->frag_seq != ack.single_fragseq)
               || ((ulm_int32_t)Frag->header.src_proc != ack.dest_proc)
               || ((ulm_int32_t)Frag->header.dest_proc != ack.src_proc)
               || ((ulm_int32_t)Frag->header.ctxAndMsgType != ack.ctxAndMsgType)) {
        // this ACK is a duplicate...or just screwed up...just ignore it...
#ifdef _DEBUG_RECVACK
        fprintf(stderr, "udpRecvFragDesc::checkForDuplicateAndNonSpecificAck() processed a bad/duplicate ACK...\n");
        fprintf(stderr,
                "Frag: seq=%lld, sproc=%d, dproc=%d, comm=%d\n",
                Frag->frag_seq,
                Frag->header.src_proc, Frag->header.dest_proc,
                Frag->header.ctxAndMsgType);
        fprintf(stderr,
                "ACK: seq=%lld, sproc=%d, dproc=%d, comm=%d\n",
                ack.single_fragseq, ack.src_proc, ack.dest_proc, ack.ctxAndMsgType);
        fflush(stderr);
#endif                          /* _DEBUG_RECVACK */
        return true;
    }
#endif  	// endif for  HEADER_ON

    return false;
}
#endif

void udpRecvFragDesc::handlePt2PtMessageAck(BaseSendDesc_t *sendDesc, udpSendFragDesc * Frag, udp_ack_header & ack)
{
    int DescPoolIndex = 0;

    if (ack.ack_or_nack == UDP_ACK_GOODACK) {

	// register frag as acked
	sendDesc->clearToSend_m = true;
	(sendDesc->NumAcked)++;

	// find out which queue this frag is on
	short whichQueue = Frag->WhichQueue;

	// reset WhichQueue flag
	Frag->WhichQueue = UDPFRAGFREELIST;

	// remove frag descriptor from list of frags to be acked
	if (whichQueue == UDPFRAGSTOACK) {
	    sendDesc->FragsToAck.RemoveLinkNoLock((Links_t *) Frag);
	}
#ifdef ENABLE_RELIABILITY
	else if (whichQueue == UDPFRAGSTOSEND) {
	    sendDesc->FragsToSend.RemoveLinkNoLock((Links_t *) Frag);
	    // increment NumSent since we were going to send this again...
	    (sendDesc->NumSent)++;
	}
#endif
	else {
	    ulm_exit((-1, "udpRecvFragDesc::processAck: Frag on %d queue\n",
                      whichQueue));
	}

#ifdef ENABLE_RELIABILITY
	// set frag_seq value to 0/null/invalid to detect duplicate ACKs
	Frag->frag_seq = 0;
#endif

	// reset send descriptor pointer
	Frag->parentSendDesc = 0;

	//  the header holds the global proc id
#ifdef HEADER_ON	
	DescPoolIndex = global_to_local_proc(ulm_ntohi(Frag->header.src_proc));
#else
        DescPoolIndex = global_to_local_proc((Frag->header.src_proc));
#endif	
        // free iovecs array possibly associated with the send frag descriptor;
	if (Frag->nonContigData) {
	    free(Frag->nonContigData);
	    Frag->nonContigData = 0;
	}

	 if (Frag->earlySend != NULL && Frag->earlySend_type != -9 ) {
             int index = getMemPoolIndex();
             if (Frag->earlySend_type ==  EARLY_SEND_SMALL) { 
		     Frag->earlySend_type = -9; 
                 UDPEarlySendData_small.returnElement((Links_t*)(Frag->earlySend), index);
             } else if (Frag->earlySend_type ==  EARLY_SEND_MED) {
		     Frag->earlySend_type = -9; 
                 UDPEarlySendData_med.returnElement((Links_t*)(Frag->earlySend), index);
             } else if (Frag->earlySend_type ==  EARLY_SEND_LARGE) {
		     Frag->earlySend_type = -9; 
                 UDPEarlySendData_large.returnElement((Links_t*)(Frag->earlySend), index);
             } else {
                 ulm_warn(("no size match 2\n"));
                 return;
             }
        }

	// return frag descriptor to free list
	//   the header holds the global proc id
	udpSendFragDescs.returnElement(Frag, DescPoolIndex);

    } 
#ifdef HEADER_ON 
    else if (myproc() == (ulm_int32_t)ulm_ntohi(Frag->header.src_proc)) {
#else
    else if (myproc() == (ulm_int32_t)(Frag->header.src_proc)) {
#endif 
        /*
	 * only process negative acknowledgements if we are
	 * the process that sent the original message; otherwise,
	 * just rely on sender side retransmission
	 */
	ulm_warn(("Warning: Copy Error upon receipt.  Will retransmit.\n"));

	// save and then reset WhichQueue flag
	short whichQueue = Frag->WhichQueue;
	Frag->WhichQueue = UDPFRAGSTOSEND;
	// move Frag from FragsToAck list to FragsToSend list
	if (whichQueue == UDPFRAGSTOACK) {
	    sendDesc->FragsToAck.RemoveLink((Links_t *) Frag);
	    sendDesc->FragsToSend.Append((Links_t *) Frag);
	}
	// move message to incomplete queue
	if (sendDesc->NumSent == sendDesc->numfrags) {
	    // sanity check, is frag really in UnackedPostedSends queue
	    if (sendDesc->WhichQueue != UNACKEDISENDQUEUE) {
		ulm_exit((-1, "Error: :: Send descriptor not in "
                          "UnackedPostedSends list, where it was expected.\n"));
	    }
	    sendDesc->WhichQueue = INCOMPLETEISENDQUEUE;
	    UnackedPostedSends.RemoveLink(sendDesc);
	    IncompletePostedSends.Append(sendDesc);
	}
	// reset frag and send desc. NumSent as though this frag has not been sent
	(sendDesc->NumSent)--;
    }				// end NACK/ACK processing
}

//-----------------------------------------------------------------------------
// Copy data from library buffers to user buffers.  At this point, the header
// will already have been read so there are three choices:
// 1. a short message - the payload has been read, copy to user buffers.
// 2. a long pt-to-pt message - only the header has been read, copy data from socket.
// 3. a long multicast message - the payload has been read and stored in shared memory
//
// NOTE, no checksums are done as this is left up to the IP layer.
//
// Returns 0 (NOTE: does not return a checksum).
//-----------------------------------------------------------------------------
unsigned int udpRecvFragDesc::CopyFunction(void *fragAddr, void *appAddr, ssize_t length)
{
    // short messages are in data, and multicast frags are in shared memory
    if (shortMsg || addr_m) {
	if (length == 0)
	    return 0;
	bcopy(fragAddr, appAddr, length);
	return 0;
    }
    //
    // Read UDP frag, copying contiguous data into user buffers.
    //

    if (dataReadFromSocket) {
	ulm_exit((-1, "udpRecvFragDesc::CopyFunction error addr = 0x%llx\n",
                  addr_m));
    }

    struct sockaddr_in cli_addr;
    struct msghdr msgHdr;
    struct iovec iov[2];

    bzero((char *) &msgHdr, sizeof(msgHdr));
    msgHdr.msg_name = (caddr_t) & cli_addr;
    msgHdr.msg_namelen = sizeof(cli_addr);
    msgHdr.msg_iov = iov;
    msgHdr.msg_iovlen = length ? 2 : 1;

    iov[0].iov_base = (char *) &header;
    iov[0].iov_len = sizeof(udp_header);;
    iov[1].iov_base = (char *) appAddr;
    iov[1].iov_len = length;

    recvmsg(sockfd, &msgHdr, 0);
    dataReadFromSocket = true;
    if (!shortMsg) {
	UDPGlobals::checkLongMessageSocket = true;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// Copy data from library buffers to user buffers.  At this point, the header
// will already have been read so there are three choices:
// 1. a short message - the payload has been read, copy to user buffers.
// 2. a long pt-to-pt message - only the header has been read, copy data from socket.
// 3. a long multicast message - the payload has been read and stored in shared memory
//
// NOTE, no checksums are done as this is left up to the IP layer.
//
// Returns length copied.
//-----------------------------------------------------------------------------
unsigned long udpRecvFragDesc::nonContigCopyFunction(void *appAddr, void *fragAddr,
						 ssize_t length, ssize_t cksumlength,
						 unsigned int *cksum, unsigned int *partialInt,
						 unsigned int *partialLength, bool firstCall, bool lastCall)
{
    char *src;

// for 32-bit processors a void * is only an integer
#if defined(__i386)
    int fragOffset = (int)fragAddr;
#else
    long long fragOffset = (long long)fragAddr;
#endif

    if (shortMsg || addr_m) {
	if (length == 0)
	    return 0;
	if (pt2ptNonContig) {
	    // this is an offset
	    src = (char *) addr_m + fragOffset;
	    bcopy(src, appAddr, length);
	}
	else {
	    bcopy(fragAddr, appAddr, length);
	}
	return length;
    }

    if (!dataReadFromSocket && !copyError) {
	// allocate process private memory for this pt-to-pt non-contiguous
	// data and read the data off of the long message socket
	addr_m = (void *)ulm_malloc(length_m);
	if (!addr_m) {
	    copyError = true;
	    return 0;
	}
	struct sockaddr_in cli_addr;
	struct msghdr msgHdr;
	struct iovec iov[2];

	bzero((char *) &msgHdr, sizeof(msgHdr));
	msgHdr.msg_name = (caddr_t) & cli_addr;
	msgHdr.msg_namelen = sizeof(cli_addr);
	msgHdr.msg_iov = iov;
	msgHdr.msg_iovlen =  2;

	iov[0].iov_base = (char *) &header;
	iov[0].iov_len = sizeof(udp_header);
	iov[1].iov_base = (char *) addr_m;
	iov[1].iov_len = length_m;

	recvmsg(sockfd, &msgHdr, 0);
	dataReadFromSocket = true;
	if (!shortMsg)
	    UDPGlobals::checkLongMessageSocket = true;
	pt2ptNonContig = true;
	if (length == 0)
	    return 0;
	src = (char *)addr_m + fragOffset;
	bcopy(src, appAddr, length);
    }
    return length;
}

//-----------------------------------------------------------------------------
// Send Acks.
//-----------------------------------------------------------------------------
bool udpRecvFragDesc::AckData(double timeNow)
{
    int useShortMsg = true;
    int hostRank, sendSockfd, count;
    udp_header hd;
    udp_ack_header & ack = hd.ack;

    struct sockaddr_in toAddr;
    struct msghdr msgHdr;
    struct iovec iov;

    // release any shared memory buffers that were allocated
    if (addr_m && (addr_m != (void *) data)) {
	    free(addr_m);
	    addr_m = 0;
    }

    bzero((char *) &msgHdr, sizeof(msgHdr));
    msgHdr.msg_name = (caddr_t) & toAddr;
    msgHdr.msg_namelen = sizeof(toAddr);
    msgHdr.msg_iov = &iov;
    msgHdr.msg_iovlen = 1;

    iov.iov_base = (char *) &hd;
    iov.iov_len = sizeof(udp_header);

    //
    // fill in the ACK payload (first 5 elements already copied, except type)
    //

    ack.type = UDP_MESSAGETYPE_ACK;
    ack.ctxAndMsgType = GENERATE_CTX_AND_MSGTYPE(ctx_m, msgType_m);
    ack.udpio = header.msg.udpio;

#ifdef ENABLE_RELIABILITY
    Communicator *pg = communicators[ctx_m];

    // pt-2-pt acks must be processed by the frag destination process only to
    // properly access process private sequence tracking lists...
    if ((msgType_m == MSGTYPE_PT2PT) && (dstProcID_m != pg->localGroup->ProcID)) {
	return false;
    }
    //       record this frag as having been delivered, so that aggregate
    // ack info may get to the peer before this ack is resent possibly
    if (msgType_m == MSGTYPE_PT2PT) {
	unsigned int glSourceProcess = pg->remoteGroup->mapGroupProcIDToGlobalProcID[srcProcID_m];
	reliabilityInfo->dataSeqsLock[glSourceProcess].lock();
	if (!isDuplicate_m && !copyError && !reliabilityInfo->deliveredDataSeqs[glSourceProcess].record(seq_m)) {
	    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
	    ulm_exit((-1, "unable to record point-to-point %lld sequence "
                      "number for source (global procID) %d\n",
                      seq_m, glSourceProcess));
	}
	reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
    } else {			// collective communications
	int source_box = global_proc_to_host(srcProcID_m);
	reliabilityInfo->coll_dataSeqsLock[source_box].lock();
	if (!isDuplicate_m && !copyError && !reliabilityInfo->coll_deliveredDataSeqs[source_box].record(seq_m)) {
	    reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
	    ulm_exit((-1, "unable to record collective %lld sequence number "
                      "for source host (global procID) %d\n",
                      seq_m, source_box));
	}
	reliabilityInfo->coll_dataSeqsLock[source_box].unlock();
    }
#endif

    // fill in src and dest_proc with proper global process ids
    if (msgType_m == MSGTYPE_PT2PT) {
	ack.dest_proc = communicators[ctx_m]->remoteGroup->mapGroupProcIDToGlobalProcID[srcProcID_m];
	ack.src_proc = communicators[ctx_m]->localGroup->mapGroupProcIDToGlobalProcID[dstProcID_m];
    } else {			// collective communications
	ack.dest_proc = srcProcID_m;
	ack.src_proc = dstProcID_m;
    }

#ifdef ENABLE_RELIABILITY
    if (isDuplicate_m) {
	ack.ack_or_nack = UDP_ACK_GOODACK;
    } else {
	ack.ack_or_nack = (copyError) ? UDP_ACK_NACK : UDP_ACK_GOODACK;
    }
#else
    ack.ack_or_nack = (copyError) ? UDP_ACK_NACK : UDP_ACK_GOODACK;
#endif
    ack.single_mseq = isendSeq_m;
    ack.single_fseq = fragIndex_m;
    ack.single_fragseq = seq_m;

#ifdef ENABLE_RELIABILITY
    if (msgType_m == MSGTYPE_PT2PT) {
	unsigned int glSourceProcess = pg->remoteGroup->mapGroupProcIDToGlobalProcID[srcProcID_m];
	// grab lock for sequence tracking lists
	reliabilityInfo->dataSeqsLock[glSourceProcess].lock();

	// do we send a specific ACK...recordIfNotRecorded returns record status before attempting record
	bool recorded;
	bool send_specific_ack = reliabilityInfo->deliveredDataSeqs[glSourceProcess].recordIfNotRecorded(seq_m, &recorded);

	// if the frag is a duplicate but has not been delivered to the user process,
	// then set the field to 0 so the other side doesn't interpret
	// these fields (it will only use the received_fragseq and delivered_fragseq fields
	if (isDuplicate_m && !send_specific_ack) {
	    ack.single_fragseq = 0;
	    if (!(reliabilityInfo->deliveredDataSeqs[glSourceProcess].erase(seq_m))) {
		reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
		ulm_exit((-1, "udpRecvFragDesc::AckData(pt2pt) unable to erase "
                          "duplicate deliv'd sequence number\n"));
	    }
	}
	// record this frag as successfully delivered or not even received, as appropriate...
	if (!(isDuplicate_m)) {
	    if (!copyError) {
		if (!recorded) {
		    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
		    ulm_exit((-1, "udpRecvFragDesc::AckData(pt2pt) unable to "
                              "record deliv'd sequence number\n"));
		}
	    } else {
		if (!(reliabilityInfo->receivedDataSeqs[glSourceProcess].erase(seq_m))) {
		    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
		    ulm_exit((-1, "udpRecvFragDesc::AckData(pt2pt) unable to "
                              "erase rcv'd sequence number\n"));
		}
		if (!(reliabilityInfo->deliveredDataSeqs[glSourceProcess].erase(seq_m))) {
		    reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
		    ulm_exit((-1, "udpRecvFragDesc::AckData(pt2pt) unable to "
                              "erase deliv'd sequence number\n"));
		}
	    }
	}
	ack.received_fragseq = reliabilityInfo->receivedDataSeqs[glSourceProcess].largestInOrder();
	ack.delivered_fragseq = reliabilityInfo->deliveredDataSeqs[glSourceProcess].largestInOrder();

	// unlock sequence tracking lists
	reliabilityInfo->dataSeqsLock[glSourceProcess].unlock();
    } else {
	// unknown communication type
	ulm_exit((-1, "udpRecvFragDesc::AckData() unknown communication "
                  "type %d\n", msgType_m));
    }
#else
    ack.received_fragseq = 0;
    ack.delivered_fragseq = 0;
#endif

    //
    // select send path
    //

    hostRank = global_proc_to_host(ack.dest_proc);
    toAddr = UDPGlobals::UDPNet->getHostAddr(hostRank);
    toAddr.sin_port = UDPGlobals::UDPNet->getHostPort(ack.dest_proc, useShortMsg);
    sendSockfd = UDPGlobals::UDPNet->getLocalSocket(useShortMsg);

    // translate the ack header to network order
#ifdef HEADER_ON
    ack.type 		= ulm_htoni(ack.type);
    ack.ctxAndMsgType   = ulm_htoni(ack.ctxAndMsgType);
    ack.dest_proc 	= ulm_htoni(ack.dest_proc);
    ack.src_proc 	= ulm_htoni(ack.src_proc);
    ack.udpio 		= ulm_htonp(ack.udpio);
    ack.single_mseq 	= ulm_htonl(ack.single_mseq);
    ack.single_fseq 	= ulm_htonl(ack.single_fseq);
    ack.single_fragseq 	= ulm_htonl(ack.single_fragseq);
    ack.received_fragseq 	= ulm_htonl(ack.received_fragseq);
    ack.delivered_fragseq 	= ulm_htonl(ack.delivered_fragseq);
    ack.ack_or_nack 	= ulm_htoni(ack.ack_or_nack);
#endif

    // socket marked non-blocking...
    count = sendmsg(sendSockfd, &msgHdr, 0);
    if (count != sizeof(udp_header)) {
	ulm_err(("udpRecvFragDesc::AckData, ERROR sending ack, count = %d\n", count));
	return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
// Return the UDP specific descriptor to the descriptor pool.
//-----------------------------------------------------------------------------
void udpRecvFragDesc::ReturnDescToPool(int onHostProcID)
{
    if (addr_m && (addr_m != (void *) data)) {
	    free(addr_m);
	    addr_m = 0;
    }

    // this may be a duplicate on the long message socket, so
    // clear the socket by reading the socket, truncating to
    // the length of the message header, discarding the datagram,
    // and resetting the appropriate state
    if (!dataReadFromSocket) {
	struct sockaddr_in cli_addr;
	struct msghdr msgHdr;
	struct iovec iov;

	bzero((char *) &msgHdr, sizeof(msgHdr));
	msgHdr.msg_name = (caddr_t) & cli_addr;
	msgHdr.msg_namelen = sizeof(cli_addr);
	msgHdr.msg_iov = &iov;
	msgHdr.msg_iovlen = 1;

	iov.iov_base = (char *) &header;
	iov.iov_len = sizeof(udp_header);;
	// read message (header only...) and discard
	// might fail...
	recvmsg(sockfd, &msgHdr, 0);
	dataReadFromSocket = true;
	if (!shortMsg) {
	    // reset global flag so pullFrags() can find next
	    // long message...
	    UDPGlobals::checkLongMessageSocket = true;
	}
    }

    WhichQueue = UDPRECVFRAGSFREELIST;
    UDPRecvFragDescs.returnElement(this, onHostProcID);

    return;
}

//-----------------------------------------------------------------------------
// Called after CopyFunction or nonContigCopyFunction -- return false
// if we could not copy the data out
//-----------------------------------------------------------------------------
bool udpRecvFragDesc::CheckData(unsigned int checkSum, ssize_t len)
{
    if (!dataReadFromSocket) {
	// a zero length message
	struct sockaddr_in cli_addr;
	struct msghdr msgHdr;
	struct iovec iov;

	bzero((char *) &msgHdr, sizeof(msgHdr));
	msgHdr.msg_name = (caddr_t) & cli_addr;
	msgHdr.msg_namelen = sizeof(cli_addr);
	msgHdr.msg_iov = &iov;
	msgHdr.msg_iovlen = 1;

	iov.iov_base = (char *) &header;
	iov.iov_len = sizeof(udp_header);;
	// read message (header only...) and discard
	recvmsg(sockfd, &msgHdr, 0);
	dataReadFromSocket = true;
	if (!shortMsg) {
	    // reset global flag so pullFrags() can find next
	    // long message...
	    UDPGlobals::checkLongMessageSocket = true;
	}
	//
	// we will declare this frag "bad", if we had
	// trouble copying this frag out to the app
	// which will be the case if copyError has been set
	//
	if (copyError) {
	    if (IOVecs) {
		free(IOVecs);
		IOVecsSize = 0;
	    }
	    return false;
	} else {
	    // zero length message
	    return true;
	}
    }
    return true;
}
