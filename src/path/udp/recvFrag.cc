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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
#include "path/udp/path.h"

#if ENABLE_RELIABILITY
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
        desc->Init();
	    desc->sockfd = shortsock;
	    desc->shortMsg = true;
	    desc->copyError = false;
	    desc->pt2ptNonContig = false;
	    desc->dataReadFromSocket = false;
	    /* we rely on the udp checksum, so if data is delivered,
	     *   we trust that it is ok
	     */
	    desc->DataOK = true;
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
        desc->Init();
        UDPGlobals::checkLongMessageSocket = false;	// will be reset as necessary
	    desc->sockfd = longsock;
	    desc->shortMsg = false;
	    desc->copyError = false;
	    desc->pt2ptNonContig = false;
	    desc->dataReadFromSocket = false;
	    /* we rely on the udp checksum, so if data is delivered,
	     *   we trust that it is ok
	     */
	    desc->DataOK = true;
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
    iov[0].iov_len = sizeof(udp_header);
    iov[1].iov_base = (char *) data;
    iov[1].iov_len = maxShortPayloadSize_g;

    do {
    count = recvmsg(sockfd, &msgHdr, 0);
    
	if (count > 0) {
#ifdef HEADER_ON
	    	header.msg.type=ulm_ntohi(header.msg.type);
#endif     
	    long type = header.msg.type;
       	    dataReadFromSocket = true;
            reread = false;


            switch (type) {
            case UDP_MESSAGETYPE_MESSAGE:
#ifdef HEADER_ON
	    	header.msg.ctxAndMsgType=ulm_ntohi(header.msg.ctxAndMsgType);
	    	header.msg.src_proc=ulm_ntohi(header.msg.src_proc);
	    	header.msg.dest_proc=ulm_ntohi(header.msg.dest_proc);
	    	header.msg.udpio=ulm_ntohp(header.msg.udpio);
		header.msg.length=ulm_ntohl(header.msg.length);
	    	header.msg.messageLength=ulm_ntohl(header.msg.messageLength);
	    	header.msg.tag_m=ulm_ntohi(header.msg.tag_m);
		header.msg.fragIndex_m=ulm_ntohi(header.msg.fragIndex_m);
	    	header.msg.isendSeq_m=ulm_ntohl(header.msg.isendSeq_m);
	    	header.msg.frag_seq=ulm_ntohl(header.msg.frag_seq);
	    	header.msg.refCnt=ulm_ntohi(header.msg.refCnt);
#endif     
                processMessage(header.msg);
                break;

            case UDP_MESSAGETYPE_ACK:
#ifdef HEADER_ON 
	    	header.ack.type=ulm_ntohi(header.ack.type);
		header.ack.ctxAndMsgType=ulm_ntohi(header.ack.ctxAndMsgType);
		header.ack.dest_proc=ulm_ntohi(header.ack.dest_proc);
		header.ack.src_proc=ulm_ntohi(header.ack.src_proc);
	    	header.ack.udpio=ulm_ntohp(header.ack.udpio);
	    	header.ack.thisFragSeq=ulm_ntohl(header.ack.thisFragSeq);
	    	header.ack.receivedFragSeq=ulm_ntohl(header.ack.receivedFragSeq);
	    	header.ack.deliveredFragSeq=ulm_ntohl(header.ack.deliveredFragSeq);
	    	header.ack.ackStatus=ulm_ntohi(header.ack.ackStatus);
#endif
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
	    	header.msg.type=ulm_ntohi(header.msg.type);
#endif
	    long type = header.msg.type;
	    dataReadFromSocket = false;
            reread = false;

            switch (type) {
            case UDP_MESSAGETYPE_MESSAGE:
#ifdef HEADER_ON
	    	header.msg.ctxAndMsgType=ulm_ntohi(header.msg.ctxAndMsgType);
	    	header.msg.src_proc=ulm_ntohi(header.msg.src_proc);
	    	header.msg.dest_proc=ulm_ntohi(header.msg.dest_proc);
	    	header.msg.udpio=ulm_ntohp(header.msg.udpio);
		header.msg.length=ulm_ntohl(header.msg.length);
	    	header.msg.messageLength=ulm_ntohl(header.msg.messageLength);
	    	header.msg.tag_m=ulm_ntohi(header.msg.tag_m);
		header.msg.fragIndex_m=ulm_ntohi(header.msg.fragIndex_m);
	    	header.msg.isendSeq_m=ulm_ntohl(header.msg.isendSeq_m);
	    	header.msg.frag_seq=ulm_ntohl(header.msg.frag_seq);
	    	header.msg.refCnt=ulm_ntohi(header.msg.refCnt);
#endif     
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

#if ENABLE_RELIABILITY
    isDuplicate_m = UNIQUE_FRAG;
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
    // setup pointer to fragment and send descriptor, so that after
    //  memory is freed, we still have valid pointers.
    //  volatile udpSendFragDesc* fragDesc = (udpSendFragDesc*) ack.udpio.ptr;
    udpSendFragDesc *Frag = (udpSendFragDesc *) ack.ptrToSendDesc.ptr;
    volatile SendDesc_t *sendDesc = (volatile SendDesc_t *) Frag->parentSendDesc_m;

	// lock frag through send descriptor to prevent two
	// ACKs from processing simultaneously

	if (sendDesc) {
	    ((SendDesc_t *)sendDesc)->Lock.lock();
	    if (sendDesc != Frag->parentSendDesc_m) {
		((SendDesc_t *)sendDesc)->Lock.unlock();
		ReturnDescToPool(getMemPoolIndex());
		return;
	    }
	} else {
	    ReturnDescToPool(getMemPoolIndex());
	    return;
	}

#if ENABLE_RELIABILITY
    if (checkForDuplicateAndNonSpecificAck(Frag)) {
	    ((SendDesc_t *)sendDesc)->Lock.unlock();
	    ReturnDescToPool(getMemPoolIndex());
	    return;
	}
#endif

	handlePt2PtMessageAck((SendDesc_t *) sendDesc, Frag, ack);
	((SendDesc_t *)sendDesc)->Lock.unlock();
    ReturnDescToPool(getMemPoolIndex());
    return;
}

void udpRecvFragDesc::handlePt2PtMessageAck(SendDesc_t *sendDesc, udpSendFragDesc * Frag, udp_ack_header & ack)
{
    if (ack.ackStatus == ACKSTATUS_DATAGOOD) {
        (sendDesc->NumAcked)++;
        Frag->freeResources(dclock(), sendDesc);
    } 
    else if (myproc() == (ulm_int32_t)(Frag->header.src_proc)) {
        /*
	 * only process negative acknowledgements if we are
	 * the process that sent the original message; otherwise,
	 * just rely on sender side retransmission
	 */
	ulm_warn(("Warning: Copy Error upon receipt.  Will retransmit. "
              "myproc() %d src_proc %d pid %d\n",myproc(),(ulm_int32_t)(Frag->header.src_proc),
              getpid()));

	// save and then reset WhichQueue flag
	short whichQueue = Frag->WhichQueue;
	// move Frag from FragsToAck list to FragsToSend list
	if (whichQueue == UDPFRAGSTOACK) {
        Frag->WhichQueue = UDPFRAGSTOSEND;
	    sendDesc->FragsToAck.RemoveLink((Links_t *) Frag);
	    sendDesc->FragsToSend.Append((Links_t *) Frag);
	}
	// move message to incomplete queue
	if ((unsigned)sendDesc->NumSent == sendDesc->numfrags) {
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
    Frag->setSendDidComplete(false);
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
    ssize_t fragOffset = (ssize_t) fragAddr;

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
    int sendSockfd, count, returnValue;
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
    ack.ptrToSendDesc = header.msg.udpio;

    /* set the sequence number information */
    ack.thisFragSeq = seq_m;

    /* process the deliverd sequence number range */
    Communicator *pg = communicators[ctx_m];
    unsigned int glSourceProcess =  pg->remoteGroup->
	    mapGroupProcIDToGlobalProcID[srcProcID_m];
    returnValue=processRecvDataSeqs(&(hd.ack), glSourceProcess,reliabilityInfo);
    if(returnValue != ULM_SUCCESS )
	    return false;

    // fill in src and dest_proc with proper global process ids
    if (msgType_m == MSGTYPE_PT2PT) {
	ack.dest_proc = communicators[ctx_m]->remoteGroup->
		mapGroupProcIDToGlobalProcID[srcProcID_m];
	ack.src_proc = communicators[ctx_m]->localGroup->
		mapGroupProcIDToGlobalProcID[dstProcID_m];
    }

    //
    // select send path
    //

    toAddr = UDPGlobals::UDPNet->getProcAddr(ack.dest_proc);
    toAddr.sin_port = UDPGlobals::UDPNet->getHostPort(ack.dest_proc, useShortMsg);
    sendSockfd = UDPGlobals::UDPNet->getLocalSocket(useShortMsg);

    // translate the ack header to network order
#ifdef HEADER_ON
    ack.type 		= ulm_htoni(ack.type);
    ack.ctxAndMsgType   = ulm_htoni(ack.ctxAndMsgType);
    ack.dest_proc 	= ulm_htoni(ack.dest_proc);
    ack.src_proc 	= ulm_htoni(ack.src_proc);
    ack.udpio 		= ulm_htonp(ack.udpio);
    ack.thisFragSeq 	= ulm_htonl(ack.thisFragSeq);
    ack.receivedFragSeq 	= ulm_htonl(ack.receivedFragSeq);
    ack.deliveredFragSeq 	= ulm_htonl(ack.deliveredFragSeq);
    ack.ackStatus 	= ulm_htoni(ack.ackStatus);
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
