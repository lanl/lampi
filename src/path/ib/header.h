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

#ifndef _IB_HEADER_H
#define _IB_HEADER_H

#include "internal/types.h"
#include "path/common/BaseDesc.h"

/* value for struct ibCommon.ctlMsgType */
enum ibCtlMsgTypes {
    MESSAGE_DATA_ACK,
    MESSAGE_DATA,
    NUMBER_CTLMSGTYPES	/* must be last */
};

struct ibCommon {
    ulm_uint32_t type;		//!< message type used to select the proper structure definition
};

struct ibDataHdr {
    struct ibCommon common;
    ulm_uint32_t ctxAndMsgType;		//!< context ID and message type (pt-to-pt v. multicast)
    ulm_int32_t tag_m;			//!< user tag value for this message
    ulm_int32_t senderID;			//!< global process id of the data sender
    ulm_int32_t destID;			//!< global process id of the data receiver
    ulm_uint32_t dataLength;		//!< the length in bytes of this frag's data
    ulm_uint64_t msgLength;			//!< the length in bytes of the total message data
    ulm_uint64_t frag_seq;		//!< unique frag sequence for retrans. (0 if ENABLE_RELIABILITY is not defined)
    ulm_uint64_t isendSeq_m;			//!< unique msg. sequence number for matching
    ulm_ptr_t sendFragDescPtr;		//!< a pointer to this frag's ibSendFragDesc (valid for sender only)
    ulm_uint64_t dataSeqOffset;		//!< the sequential offset in bytes of this frag's data
    ulm_uint32_t dataChecksum;		//!< additive checksum or CRC for this frag's data
    ulm_uint32_t checksum;			//!< additive checksum or CRC of the message header
};

#define ACKSTATUS_DATAGOOD 0x1			//!< data arrived okay (value for ackStatus below)
#define ACKSTATUS_DATACORRUPT 0x2		//!< data arrived corrupted (value for ackStatus below)
#define ACKSTATUS_AGGINFO_ONLY 0x4		//!< ack fields for a specific frag are not valid (OR'ed with ackStatus)
#define DATAACK_PADDING 16

// note: sizeof(BaseAck) is not obvious because it may include
// a vtable even though there are no virtual functions
struct ibDataAck : public BaseAck_t {
    char padding[(sizeof(BaseAck_t) % sizeof(ulm_uint32_t)) + 4];
    ulm_uint32_t checksum;			//!< additive checksum or CRC of the message header
};

#define IB2KMSGDATABYTES (2048 - sizeof(struct ibDataHdr))

struct ibData2KMsg {
    struct ibDataHdr header;
    char data[IB2KMSGDATABYTES];
};

typedef struct ibCommon ibCommon_t;
typedef struct ibDataHdr ibDataHdr_t;
typedef struct ibDataAck ibDataAck_t;
typedef struct ibData2KMsg ibData2KMsg_t;

#endif
