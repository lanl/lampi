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

#ifndef GM_HEADER_H
#define GM_HEADER_H

#include "internal/types.h"

// GM message fragment types
enum gmMsgTypes {
    MESSAGE_DATA_ACK = 0,
    MESSAGE_DATA,
    NUMBER_CTLMSGTYPES	// must be last
};

// Other constants
enum {
    ACKSTATUS_DATAGOOD = 0x1,           // data arrived okay (value for ackStatus below)
    ACKSTATUS_DATACORRUPT = 0x2,        // data arrived corrupted (value for ackStatus below)
    ACKSTATUS_AGGINFO_ONLY = 0x4,       // ack fields for a specific frag are not valid (OR'ed with ackStatus)
    HEADER_SIZE = 18 * sizeof(ulm_uint32_t),
    DATAACK_PADDING = 2
};

struct gmHeaderCommon {
    unsigned short ctlMsgType;          // message type used to select the proper structure definition
};

// if dataChecksum is a CRC, then it is stored in the native integer
// order so that it can be compared for equality with a calculated CRC
// over the data region!

struct gmHeaderData {
    gmHeaderCommon common;
    ulm_uint32_t ctxAndMsgType;       // context ID and message type (pt-to-pt v. multicast)
    ulm_int32_t user_tag;               // user tag value for this message
    ulm_uint32_t senderID;              // global process id of the data sender
    ulm_uint32_t destID;                // global process id of the data receiver
    ulm_uint32_t dataLength;            // the length in bytes of this frag's data
    ulm_uint64_t msgLength;             // the length in bytes of the total message data
    ulm_uint64_t frag_seq;              // unique frag sequence for retrans. (0 if ENABLE_RELIABILITY is not defined)
    ulm_uint64_t isendSeq_m;             // unique msg. sequence number for matching
    ulm_ptr_t sendFragDescPtr;          // a pointer to this frag's quadricsSendFragDesc (valid for sender only)
    ulm_uint64_t dataSeqOffset;         // the sequential offset in bytes of this frag's data
    ulm_uint32_t dataChecksum;          // additive checksum or CRC for this frag's data
    ulm_uint32_t checksum;              // additive checksum or CRC of all 128 - 4 bytes of the control message
};

struct gmHeaderDataAck {
    gmHeaderCommon common;
    ulm_uint32_t ctxAndMsgType;       // context ID and message type (pt-to-pt v. multicast)
    ulm_int32_t  tag_m;               // user tag value for this message
    ulm_uint32_t senderID;              // global process id of the ACK sender
    ulm_uint32_t destID;                // global process id of the ACK receiver
    ulm_uint32_t ackStatus;             // ACK flags (see above definitions)
    ulm_ptr_t    sendFragDescPtr;       // a pointer to this frag's quadricsSendFragDesc (valid for receiver only)
    ulm_uint64_t thisFragSeq;           // the frag sequence value of the received frag being ACKed
    ulm_uint64_t deliveredFragSeq;      // the largest in-order frag seq. value of frags delivered to the app
    ulm_uint64_t receivedFragSeq;       // the largest in-order frag seq. vallue of frags received
    ulm_uint32_t padding[DATAACK_PADDING];
    ulm_uint32_t checksum;              // additive checksum or CRC of all 128 - 4 bytes of the control message
};

union gmHeader {
    unsigned char bytes[HEADER_SIZE];
    ulm_uint64_t lwords[HEADER_SIZE / sizeof(ulm_uint64_t)];
    ulm_uint32_t words[HEADER_SIZE / sizeof(ulm_uint32_t)];
    gmHeaderCommon common;
    gmHeaderData data;
    gmHeaderDataAck dataAck;
};

#endif // GM_HEADER_H
