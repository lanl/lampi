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

#include <strings.h>

#include "path/udp/sendFrag.h"
#include "path/udp/state.h"
#include "path/udp/UDPNetwork.h"
#include "path/udp/UDPEarlySend.h"
#include "client/daemon.h"
#include "queue/globals.h"	// for communicators

bool udpSendFragDesc::init()
{
    BaseSendFragDesc_t::init();
    bzero(&toAddr, sizeof(toAddr));		// filled in for each send
    bzero((char*) &msgHdr, sizeof(msgHdr));

    msgHdr.msg_name    = (caddr_t) &toAddr;
    msgHdr.msg_namelen = sizeof(struct sockaddr_in);
    // msgHdr.msg_iov     = ioVecs;
    // msgHdr.msg_iovlen  = 2;

    ioVecs[0].iov_base = (caddr_t) &header;	// starting address header
    ioVecs[0].iov_len  = sizeof(udp_header);		// length of frag header

#ifdef _DEBUGQUEUES
    WhichQueue = UDPFRAGFREELIST;
#endif
    timeSent_m = -1;
    numTransmits_m = 0;
    nonContigData = 0;
    
    return true;
}

void udpSendFragDesc::freeResources(double timeNow, SendDesc_t *bsd)
{
    int DescPoolIndex = 0;
    short whichQueue = WhichQueue;
    
	// register frag as acked
	bsd->clearToSend_m = true;
        
	// reset WhichQueue flag
	WhichQueue = UDPFRAGFREELIST;
    
	// remove frag descriptor from list of frags to be acked
	if (whichQueue == UDPFRAGSTOACK) {
	    bsd->FragsToAck.RemoveLinkNoLock((Links_t *) this);
	}
#if ENABLE_RELIABILITY
	else if (whichQueue == UDPFRAGSTOSEND) {
	    bsd->FragsToSend.RemoveLinkNoLock((Links_t *) this);
	    // increment NumSent since we were going to send this again...
	    (bsd->NumSent)++;
	}
#endif
	else {
	    ulm_exit(("udpRecvFragDesc::processAck: Frag on %d queue\n",
                  WhichQueue));
	}
    
#if ENABLE_RELIABILITY
	// set frag_seq value to 0/null/invalid to detect duplicate ACKs
	fragSeq_m = 0;
#endif
    
	// reset send descriptor pointer
	parentSendDesc_m = 0;
    
	//  the header holds the global proc id
    DescPoolIndex = global_to_local_proc((header.src_proc));
    
    // free iovecs array possibly associated with the send frag descriptor;
	if (nonContigData) {
	    free(nonContigData);
	    nonContigData = 0;
	}
    
    if (earlySend != NULL && earlySend_type != -9 ) {
        int index = getMemPoolIndex();
        if (earlySend_type ==  EARLY_SEND_SMALL) { 
            earlySend_type = -9; 
            UDPEarlySendData_small.returnElement((Links_t*)(earlySend), index);
        } else if (earlySend_type ==  EARLY_SEND_MED) {
            earlySend_type = -9; 
            UDPEarlySendData_med.returnElement((Links_t*)(earlySend), index);
        } else if (earlySend_type ==  EARLY_SEND_LARGE) {
            earlySend_type = -9; 
            UDPEarlySendData_large.returnElement((Links_t*)(earlySend), index);
        } else {
            ulm_warn(("no size match 2\n"));
            return;
        }
    }
    
	// return frag descriptor to free list
	//   the header holds the global proc id
	udpSendFragDescs.returnElement(this, DescPoolIndex);
}


//-----------------------------------------------------------------------------
//! Convert the header from host order to network byte order.  The udpio
//! pointer doesn't have to be converted because it is only valid on
//! originating host.
//-----------------------------------------------------------------------------
void udpSendFragDesc::copyHeader(udp_message_header& to)
{
    to.type	     	= ulm_htoni(header.type);
    to.length	     	= ulm_htonl(header.length);
    to.messageLength   	= ulm_htonl(header.messageLength);
    to.udpio	     	= ulm_htonp(header.udpio);
    to.dest_proc     	= ulm_htoni(header.dest_proc);
    to.src_proc	     	= ulm_htoni(header.src_proc);
    to.tag_m	     	= ulm_htoni(header.tag_m);
    to.ctxAndMsgType 	= ulm_htoni(header.ctxAndMsgType);
    to.fragIndex_m 	= ulm_htoni(header.fragIndex_m);
    to.isendSeq_m     	= ulm_htonl(header.isendSeq_m);
    to.frag_seq    	= ulm_htonl(header.frag_seq);
    to.refCnt	     	= ulm_htoni(header.refCnt);
}
