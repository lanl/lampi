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

#ifndef _UDP_SENDDESC_H_
#define _UDP_SENDDESC_H_

#include <stdio.h>
#include <sys/socket.h>		// needed for msghdr
#include <netinet/in.h>		// needed for struct sockaddr_in
#include <sys/uio.h>		// needed for iovecs

#include "queue/globals.h"	// needed for local_myproc()
#include "path/common/BaseDesc.h"
#include "path/udp/state.h"
#include "path/udp/header.h"
#include "util/DblLinkList.h"// needed for DoubleLinkList


// Send fragment descriptor for UDP path

class udpSendFragDesc : public Links_t
{
public:

    // Constructors

    udpSendFragDesc()
        {
            init();
        }

    udpSendFragDesc(int lockType)
        {
            init();
        }

    // initialize the send frag
    void init();

    // Pointer to the send descriptor owning this frag
    BaseSendDesc_t *parentSendDesc;

    int sendSockfd;		// socket descriptor for sendmsg
    struct msghdr msgHdr;		// udp message header for sendmsg
    ulm_iovec_t ioVecs[2];	// iovecs for header and data for contiguous and zero-length msgs.
    char *nonContigData;	// pointer to process private memory for packed non-contiguous data
    void* earlySend; 		//  point to process private memory for early send completion
    int  earlySend_type;		// what type of early send frag are we pointing to;
    struct sockaddr_in toAddr;	// sockaddr of receiver
    int fragIndex;		// frag frag index
    unsigned int len;		// length of entire frag (header + data)
    udp_message_header header;	// header for this message frag

    //
    // The following is related to message send, but has no relation
    // to the descriptor q. It is used to figure out whether a given
    // hpio entry can be deleted, however
    //
    int flags;

    double timeSent;		// time that frag was sent
    int numTransmits;		// number of times frag's been transmitted
    int ctx_m;			// index of communicator
    int msgType_m;		// type ID (point-to-point, collective, etc.)
    int tmap_index;	// typemap index for datatypes
    size_t seqOffset_m; // "contiguous" byte offset as if all data was packed
    unsigned long long frag_seq;
    int globalDestID;


    void copyHeader(udp_message_header& to);
} ;

#endif // _UDP_SENDDESC_H_
