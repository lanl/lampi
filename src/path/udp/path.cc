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

#include <strings.h>
#include <errno.h>

#include "queue/globals.h"	// for communicators
#include "client/ULMClient.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "path/udp/path.h"

int maxOutstandingUDPFrags = 8;
// only done for non-zero non-contiguous data
bool udpPath::packData(SendDesc_t *message, udpSendFragDesc *frag)
{
    unsigned char *src_addr, *dest_addr;
    size_t len_to_copy, len_copied, payloadSize;
    ULMType_t *dtype = message->datatype;
    ULMTypeMapElt_t *tmap = dtype->type_map;
    int tm_init = frag->tmapIndex_m;
    int init_cnt = frag->seqOffset_m / dtype->packed_size;
    int tot_cnt = message->posted_m.length_m / dtype->packed_size;
    unsigned char *start_addr = ((unsigned char *) message->addr_m)
	+ init_cnt * dtype->extent;
    int dtype_cnt, ti;

    payloadSize = frag->length_m - sizeof(udp_header);
    frag->nonContigData = (char *)ulm_malloc(payloadSize);
    if (!frag->nonContigData) {
	return false;
    }
    // set msgHdr to point to nonContigIOVecs
    frag->msgHdr.msg_iov = (struct iovec *) frag->ioVecs;
    frag->msgHdr.msg_iovlen = 2;
    frag->flags |= UDP_IO_IOVECSSETUP;
    dest_addr = (unsigned char *)frag->nonContigData;

    // handle first typemap pair
    src_addr = start_addr
	+ tmap[tm_init].offset
	- init_cnt * dtype->packed_size - tmap[tm_init].seq_offset + frag->seqOffset_m;
    len_to_copy = tmap[tm_init].size
	+ init_cnt * dtype->packed_size + tmap[tm_init].seq_offset - frag->seqOffset_m;
    len_to_copy = (len_to_copy > payloadSize) ? payloadSize : len_to_copy;

    bcopy(src_addr, dest_addr, len_to_copy);
    len_copied = len_to_copy;

    tm_init++;
    for (dtype_cnt = init_cnt; dtype_cnt < tot_cnt; dtype_cnt++) {
	for (ti = tm_init; ti < dtype->num_pairs; ti++) {
	    src_addr = start_addr + tmap[ti].offset;
	    dest_addr = (unsigned char *)frag->nonContigData + len_copied;
	    len_to_copy = (payloadSize - len_copied >= tmap[ti].size) ?
		tmap[ti].size : payloadSize - len_copied;
	    if (len_to_copy == 0) {
		frag->ioVecs[1].iov_base = frag->nonContigData;
		frag->ioVecs[1].iov_len = len_copied;
		message->pathInfo.udp.numFragsCopied++;
		return true;
	    }

	    bcopy(src_addr, dest_addr, len_to_copy);
	    len_copied += len_to_copy;
	}

	tm_init = 0;
	start_addr += dtype->extent;
    }

    frag->ioVecs[1].iov_base = frag->nonContigData;
    frag->ioVecs[1].iov_len = len_copied;
    message->pathInfo.udp.numFragsCopied++;

    return true;
}

bool udpPath::send(SendDesc_t *message, bool *incomplete, int *errorCode)
{
    bool shortMsg; 
    int gldestProc;  
    int nDesc; 
    int nDescToAllocate;
    int returnValue;
    size_t leftToSend;
    size_t  payloadSize;
    unsigned long offset = 0 ;
    unsigned char *src_addr;
    unsigned char *dest_addr;
    udpSendFragDesc *sendFragDesc;

    *incomplete 	= true;
    nDescToAllocate 	= 0;
    offset 		= 0;
    returnValue 	= ULM_SUCCESS; 
    gldestProc 		= communicators[message->ctx_m]->remoteGroup->
        mapGroupProcIDToGlobalProcID[message->posted_m.peer_m];

    // always allocate and try to send the first frag
    if (message->NumFragDescAllocated == 0)
	message->clearToSend_m = true;
    //
    // Allocate frag descriptors (as needed).
    //
    nDescToAllocate = message->numfrags - message->NumFragDescAllocated;
    for (nDesc = 0; (nDesc < nDescToAllocate) && message->clearToSend_m; nDesc++) {
	// slow down the send
	if ((maxOutstandingUDPFrags != -1) && (message->NumFragDescAllocated - message->NumAcked >=
					       (unsigned int)maxOutstandingUDPFrags)) {
	    message->clearToSend_m = false;
	}
	// Request To Send/Clear To Send
	if (!message->clearToSend_m) {
	    break;
	}

	//
	// Get an unused frag descriptor.  If no more available, new ones
	// will be constructed but move on to send frags
	// (return later to finish).
	//

	sendFragDesc = udpSendFragDescs.getElement(getMemPoolIndex(), returnValue);
	if (returnValue != ULM_SUCCESS) {
	    if ((returnValue == ULM_ERR_OUT_OF_RESOURCE) || (returnValue == ULM_ERR_FATAL)) {
		*errorCode = returnValue;
		return false;
	    } else {
		continue;
	    }
	}

	// sanity check
#ifdef _DEBUGQUEUES
	if (sendFragDesc->WhichQueue != UDPFRAGFREELIST) {
	    ulm_exit((-1, "sendFragDesc->WhichQueue %d\n",
                      sendFragDesc->WhichQueue));
	}
#endif	// _DEBUGQUEUES

	//
	// initialize send frag
	//

	sendFragDesc->parentSendDesc_m = message;
	sendFragDesc->numTransmits_m = 0;
	sendFragDesc->timeSent_m = -1;
	sendFragDesc->fragIndex = message->NumFragDescAllocated;
	sendFragDesc->flags = 0;
	sendFragDesc->ctx_m = message->ctx_m;
	sendFragDesc->msgType_m = MSGTYPE_PT2PT;

	if (message->NumFragDescAllocated == 0) {
	    leftToSend = message->posted_m.length_m;
	    offset = 0;
	    if (leftToSend < maxShortPayloadSize_g) {
		payloadSize = leftToSend;
	    } else {
		payloadSize = maxShortPayloadSize_g;
		message->clearToSend_m = false;	// wait for clear to send ack
	    }
	    shortMsg = true;
	} else {
	    offset = maxShortPayloadSize_g + maxPayloadSize_g * (message->NumFragDescAllocated - 1);
	    leftToSend = message->posted_m.length_m - offset;
	    payloadSize = (leftToSend < maxPayloadSize_g) ? leftToSend : maxPayloadSize_g;
	    shortMsg = (payloadSize <= maxShortPayloadSize_g);
	}

	sendFragDesc->seqOffset_m = offset;

	//
	// select send path
	//

	sendFragDesc->toAddr = UDPGlobals::UDPNet->getProcAddr(gldestProc);
	sendFragDesc->toAddr.sin_port = UDPGlobals::UDPNet->getHostPort(gldestProc, shortMsg);
	sendFragDesc->sendSockfd = UDPGlobals::UDPNet->getLocalSocket(shortMsg);
	// We attempt to get the UDPEarlySend_ descriptor.
	// if we are temporarily out of resources we will return true (which
	// means .. come back later -- big boy.).  if its a hard failure
	// return false.
	if (payloadSize) {
	    if (message->datatype == NULL || message->datatype->layout == CONTIGUOUS) {
		if (payloadSize <= EARLY_SEND_SMALL){
                    sendFragDesc->earlySend = (void*)UDPEarlySendData_small.getElement(getMemPoolIndex(), returnValue);
                    if (returnValue != ULM_SUCCESS) {
			*errorCode = returnValue;
		 	ulm_warn(("UDPEarlySendData_small is null"));	
			if (returnValue == ULM_ERR_TEMP_OUT_OF_RESOURCE)
				return true;
			else 	
				return false;  
		   } 
		    sendFragDesc->earlySend_type = EARLY_SEND_SMALL; 	
                    dest_addr = (unsigned char*)(((UDPEarlySend_small*)sendFragDesc->earlySend)->data);	
                } 
		else if (payloadSize <= EARLY_SEND_MED){	
                    sendFragDesc->earlySend = (void*)UDPEarlySendData_med.getElement(getMemPoolIndex(), returnValue);
               	       if (returnValue != ULM_SUCCESS) {
                        *errorCode = returnValue;
                        ulm_warn(("UDPEarlySendData_med is null"));
                        if (returnValue == ULM_ERR_TEMP_OUT_OF_RESOURCE)
                                return true;
                        else    
                                return false;
    			} 
			sendFragDesc->earlySend_type = EARLY_SEND_MED; 
                    	dest_addr = (unsigned char*)(((UDPEarlySend_med*)sendFragDesc->earlySend)->data);	
                }	
		else if (payloadSize <= EARLY_SEND_LARGE) {
                    sendFragDesc->earlySend =  UDPEarlySendData_large.getElement(getMemPoolIndex(), returnValue);
		     if (returnValue != ULM_SUCCESS) {
                        *errorCode = returnValue;
                        ulm_warn(("UDPEarlySendData_ large is null"));
                        if (returnValue == ULM_ERR_TEMP_OUT_OF_RESOURCE)
                                return true;
                        else
                                return false;
                        }
		    sendFragDesc->earlySend_type = EARLY_SEND_LARGE;
		    dest_addr = (unsigned char*)(((UDPEarlySend_large*)sendFragDesc->earlySend)->data);	
                }
		else {
                    returnValue = ULM_ERR_FATAL;
                    *errorCode = returnValue;
                    return false; 
		}	
		
		// Let's make sure that we have the dest_addr.  If we didn't get an element successfully
		// we abort (for now).  but in the future we could just go on the send 
		// sans the early send completion..
		if (returnValue != ULM_SUCCESS || dest_addr == NULL) {
                    if ((returnValue == ULM_ERR_OUT_OF_RESOURCE) || (returnValue == ULM_ERR_FATAL)) {
                        *errorCode = returnValue;
                        return false;
                    }
	      	} 
                src_addr = (unsigned char *)message->addr_m + offset;
                memcpy(dest_addr,src_addr,payloadSize);
                message->pathInfo.udp.numFragsCopied++;
                sendFragDesc->ioVecs[1].iov_base = (char *)dest_addr;
                sendFragDesc->ioVecs[1].iov_len = payloadSize;
                sendFragDesc->msgHdr.msg_iov = (struct iovec *)sendFragDesc->ioVecs;
                sendFragDesc->msgHdr.msg_iovlen = 2;    // message header + contiguous data
                sendFragDesc->flags |= UDP_IO_IOVECSSETUP;
                sendFragDesc->tmapIndex_m = 0;	    
            }
	    // calculate tmapIndex_m for non-zero non-contiguous data
	    else {
		int dtype_cnt = offset / message->datatype->packed_size;
		size_t data_copied = dtype_cnt * message->datatype->packed_size;
		ssize_t data_remaining = (ssize_t)(offset - data_copied);
		sendFragDesc->tmapIndex_m = message->datatype->num_pairs - 1;
		for (int ti = 0; ti < message->datatype->num_pairs; ti++) {
		    if (message->datatype->type_map[ti].seq_offset == data_remaining) {
			sendFragDesc->tmapIndex_m = ti;
			break;
		    } else if (message->datatype->type_map[ti].seq_offset > data_remaining) {
			sendFragDesc->tmapIndex_m = ti - 1;
			break;
		    }
		}
	    }
	} else {
	    sendFragDesc->msgHdr.msg_iov = (struct iovec *)sendFragDesc->ioVecs;
	    sendFragDesc->msgHdr.msg_iovlen = 1;	// just the message header
	    sendFragDesc->flags |= UDP_IO_IOVECSSETUP;
	    sendFragDesc->tmapIndex_m = 0;
	}

	sendFragDesc->length_m = payloadSize + sizeof(udp_header);

	//
	// fill in header
	//

	int myRank = communicators[message->ctx_m]->localGroup->ProcID;
	udp_message_header & header = sendFragDesc->header;

	header.type 		= UDP_MESSAGETYPE_MESSAGE;
	header.length 		= payloadSize;
	header.messageLength 	= message->posted_m.length_m;
	header.udpio.ptr 	= sendFragDesc;
	header.src_proc 	= communicators[message->ctx_m]->localGroup->mapGroupProcIDToGlobalProcID[myRank];
	header.dest_proc 	= gldestProc;
	header.tag_m 	= message->posted_m.tag_m;
	header.ctxAndMsgType =
	    GENERATE_CTX_AND_MSGTYPE(message->ctx_m, sendFragDesc->msgType_m);
	header.fragIndex_m = message->NumFragDescAllocated;
	header.isendSeq_m = message->isendSeq_m;
	header.refCnt = 0;

	// save global destination process ID for later use (resend, etc.)
	sendFragDesc->globalDestProc_m = header.dest_proc;

	// thread-safe allocation of frag sequence number in header

#ifdef ENABLE_RELIABILITY    
    sendFragDesc->parentSendDesc_m->path_m->initFragSeq(sendFragDesc);
#else
    sendFragDesc->setFragSequence(0);
#endif
    
	sendFragDesc->header.frag_seq = sendFragDesc->fragSequence();

#ifdef HEADER_ON
        sendFragDesc->copyHeader(sendFragDesc->header);
#endif
	sendFragDesc->WhichQueue = UDPFRAGSTOSEND;
	message->FragsToSend.AppendNoLock(sendFragDesc);
	(message->NumFragDescAllocated)++;

    }				// end loop over descriptor allocation and initialization

    //
    // Attempt to send all frags in the to send list.
    //
    for (sendFragDesc = (udpSendFragDesc *) message->FragsToSend.begin();
	 sendFragDesc != (udpSendFragDesc *) message->FragsToSend.end();
	 sendFragDesc = (udpSendFragDesc *) sendFragDesc->next) {

	// if we have non-contiguous data, we need to pack the data for sendmsg
	if ((sendFragDesc->flags & UDP_IO_IOVECSSETUP) == 0) {
	    if (!packData(message, sendFragDesc))
		continue;
	}

	// socket marked non-blocking...
	int count = sendmsg(sendFragDesc->sendSockfd,
			    &(sendFragDesc->msgHdr),
			    0);
	if (count < 0) {
	    if (errno != EAGAIN)
                ulm_warn(("UDPSendDesc::Send, ERROR sending frag, error = %d, count = %d\n", errno, count));
	    if (errno == EMSGSIZE) {
		ulm_warn(("UDPSendDesc:: EMSGSIZE returned by sendmsg() for %d bytes\n", sendFragDesc->length_m));
	    }
	    continue;
	}
    sendFragDesc->setSendDidComplete(true);
    
#ifdef ENABLE_RELIABILITY
	sendFragDesc->timeSent_m = dclock();
	unsigned long long max_multiple =
	    (sendFragDesc->numTransmits_m <
	     MAXRETRANS_POWEROFTWO_MULTIPLE) ? (1 << sendFragDesc->
						numTransmits_m) : (1 <<
								 MAXRETRANS_POWEROFTWO_MULTIPLE);
	(sendFragDesc->numTransmits_m)++;

	double timeToResend = sendFragDesc->timeSent_m + (RETRANS_TIME * max_multiple);
	if (message->earliestTimeToResend == -1) {
	    message->earliestTimeToResend = timeToResend;
	} else if (timeToResend < message->earliestTimeToResend) {
	    message->earliestTimeToResend = timeToResend;
	}
#endif

	// switch frag to ack list and remove from send list

	udpSendFragDesc *fragToAck = sendFragDesc;
	fragToAck->WhichQueue = UDPFRAGSTOACK;
	sendFragDesc = (udpSendFragDesc *) message->FragsToSend.RemoveLinkNoLock(fragToAck);
	message->FragsToAck.AppendNoLock(fragToAck);

	++(message->NumSent);

    }	// end loop over frags to send

    if (
	(message->messageDone==REQUEST_INCOMPLETE) &&
	(message->sendType != ULM_SEND_SYNCHRONOUS) && 
	(message->pathInfo.udp.numFragsCopied == (int)message->numfrags)
        )
    {
	message->messageDone = REQUEST_COMPLETE;
    }

    if (message->NumSent == message->numfrags)
	*incomplete = false;

    return true;
}

