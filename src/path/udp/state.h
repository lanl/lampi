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



#ifndef _UDP_GLOBALS_H_
#define _UDP_GLOBALS_H_

#include "mem/FreeLists.h"
#include "internal/mmap_params.h"

// Constants

const int UDP_IO_IOVECSSETUP                 = 1;

// Types of UDP messages.

const int UDP_MESSAGETYPE_MESSAGE            = 1;
const int UDP_MESSAGETYPE_ACK                = 2;

// The maximum length of a UDP frag (data length).  This length is
// MTU - IP_header_length - UDP_header_length.  For MTU, netstat -i
//
// const long UDP_MAX_DATA = 1500 - 20 - 8;
const long UDP_MAX_DATA = 8192;

// The maximum size of a small frag.  This should be roughly twice
// the size of the UDP message header.

const unsigned long MaxShortFragSize_g = 256;

//
// Forward references
//

class udpSendFragDesc;
class udpRecvFragDesc;
class UDPEarlySend_large;
class UDPEarlySend_med; 
class UDPEarlySend_small; 


// Pools of free frag descriptors.  These lists have global scope.
extern FreeListShared_t <udpRecvFragDesc> UDPRecvFragDescs;
extern FreeListPrivate_t <udpSendFragDesc> udpSendFragDescs;

// Freelists for use in the early udp send completion 
extern FreeListPrivate_t <UDPEarlySend_large> UDPEarlySendData_large;
extern FreeListPrivate_t <UDPEarlySend_med> UDPEarlySendData_med;
extern FreeListPrivate_t <UDPEarlySend_small> UDPEarlySendData_small;

//
// Maximum frag and payload sizes of a ULM_UDP message.  This size is not
// necessarily set by the UDP_MAX_DATA constant.  For instance, it may be
// more efficient to send larger frags if the dropped frag rate is low.
// These variables are global.
//
extern unsigned long maxFragSize_g;
extern unsigned long maxShortPayloadSize_g;
extern unsigned long maxPayloadSize_g;

#endif /* _UDP_GLOBALS_H_ */
