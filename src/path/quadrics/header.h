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



#ifndef _QUADRICS_MESSAGES
#define _QUADRICS_MESSAGES

#include "internal/types.h"
#include "path/common/BaseDesc.h"

/* value for struct quadricsCommon.ctlMsgType */
enum quadricsCtlMsgTypes {
    MESSAGE_DATA_ACK,
    MEMORY_REQUEST,
    MEMORY_REQUEST_ACK,
    MEMORY_RELEASE,
    MESSAGE_DATA,
    NUMBER_CTLMSGTYPES	/* must be last */
};

struct quadricsCommon {
    ulm_uint32_t type;		//!< message type used to select the proper structure definition
};

#define FULLCMN_PADDING 30

/* if checksum is a CRC, then it is saved in MSB -> LSB order so that a full bcopy and CRC
 * of the message and CRC should yield a final CRC of 0!
 */

struct quadricsFullCommon {
    ulm_uint32_t type;		//!< message type used to select the proper structure definition
    ulm_uint32_t padding[FULLCMN_PADDING];
    ulm_uint32_t checksum;			//!< additive checksum or CRC of all 128 - 4 bytes of the control message
};

#define CTLHDR_DATABYTES 52			//!< number of bytes of message data available in the msg. data message

/* if dataChecksum is a CRC, then it is stored in the native integer order so that it can be compared for
 * equality with a calculated CRC over the data region!
 */

struct quadricsDataHdr {
    struct quadricsCommon common;
    ulm_uint32_t ctxAndMsgType;		//!< context ID and message type (pt-to-pt v. multicast)
    ulm_int32_t tag_m;			//!< user tag value for this message
    ulm_int32_t senderID;			//!< global process id of the data sender
    ulm_int32_t destID;			//!< global process id of the data receiver
    ulm_uint32_t dataLength;		//!< the length in bytes of this frag's data
    ulm_uint64_t msgLength;			//!< the length in bytes of the total message data
    ulm_uint64_t frag_seq;		//!< unique frag sequence for retrans. (0 if ENABLE_RELIABILITY is not defined)
    ulm_uint64_t isendSeq_m;			//!< unique msg. sequence number for matching
    ulm_ptr_t sendFragDescPtr;		//!< a pointer to this frag's quadricsSendFragDesc (valid for sender only)
    ulm_uint64_t dataSeqOffset;		//!< the sequential offset in bytes of this frag's data
    ulm_uint32_t dataChecksum;		//!< additive checksum or CRC for this frag's data
    ulm_uint32_t dataElanAddr;		//!< E3_Addr destination address of this frag's data
    unsigned char data[CTLHDR_DATABYTES];	//!< data only if msgLength <= CTLHDR_DATABYTES
    ulm_uint32_t checksum;			//!< additive checksum or CRC of all 128 - 4 bytes of the control message
};

#define ACKSTATUS_DATAGOOD 0x1			//!< data arrived okay (value for ackStatus below)
#define ACKSTATUS_DATACORRUPT 0x2		//!< data arrived corrupted (value for ackStatus below)
#define ACKSTATUS_AGGINFO_ONLY 0x4		//!< ack fields for a specific frag are not valid (OR'ed with ackStatus)
#define DATAACK_PADDING 16

struct quadricsDataAck : public BaseAck_t {
    ulm_uint32_t padding[DATAACK_PADDING];
    ulm_uint32_t checksum;			//!< additive checksum or CRC of all 128 - 4 bytes of the control message
};

#define MEMRLS_MAX_WORDPTRS 23			//!< number of 32-bit pointers that can be released in one message
#define MEMRLS_MAX_LWORDPTRS 11			//!< number of 64-bit pointers that can be released in one message

/* idempotent...
 */
struct quadricsMemRls {
    struct quadricsCommon common;
    ulm_uint32_t memType;			//!< type and size of memory that is being released (see bufTrackingTypes)
    ulm_int32_t memBufCount;		//!< number of buffer pointers being released
    ulm_int32_t senderID;			//!< global process id of the release request sender
    ulm_int32_t destID;			//!< global process id of the release request receiver
    ulm_uint32_t padding;
    ulm_uint64_t releaseSeq;		//!< unique release request serial number
    union {
        ulm_uint32_t wordPtrs[MEMRLS_MAX_WORDPTRS];
        /*
        ulm_uint64_t lwordPtrs[MEMRLS_MAX_LWORDPTRS];
        */
    } memBufPtrs;
    ulm_uint32_t checksum;			//!< additive checksum or CRC of all 128 - 4 bytes of the control message
};

/* not idempotent, sender must use timer control to
 * avoid resending a given request too quickly
 */
#define MEMREQ_PADDING 18
struct quadricsMemReq {
    struct quadricsCommon common;
    ulm_int32_t tag_m;			//!< user tag value for this message
    ulm_uint64_t msgLength;		//!< message length in bytes
    ulm_ptr_t sendMessagePtr;		//!< a pointer to this message's SendDesc_t (valid for sender only)
    ulm_uint64_t memNeededSeqOffset;	//!< sequential offset in bytes of msg. where memory is needed
    ulm_uint64_t memNeededBytes;	//!< number of bytes of memory buffer needed
    ulm_uint32_t senderID;		//!< global process id of the request sender
    ulm_uint32_t destID;		//!< global process id of the request receiver
    ulm_uint32_t ctxAndMsgType;		//!< context ID and message type (pt-to-pt v. multicast)
    ulm_uint32_t padding[MEMREQ_PADDING];
    ulm_uint32_t checksum;		//!< additive checksum or CRC of all 128 - 4 bytes of the control message
};

/* values used by requestStatus below */
#define MEMREQ_SUCCESS 0x0
#define MEMREQ_FAILURE_TMP 0x1
#define MEMREQ_FAILURE_FATAL 0x2

#define MEMREQ_MAX_WORDPTRS 19		//!< number of 32-bit pointers that can be obtained in one message
#define MEMREQ_MAX_LWORDPTRS 9 		//!< number of 64-bit pointers that can be obtained in one message

struct quadricsMemReqAck {
    struct quadricsCommon common;
    ulm_uint32_t requestStatus;		//!< request status success or failure code
    ulm_int32_t memBufCount;		//!< number of memory buffers returned
    ulm_uint32_t memBufSize;		//!< size in bytes of the memory buffers obtained
    ulm_uint32_t memType;		//!< type and size of memory that has been obtained
    ulm_uint32_t senderID;		//!< global process id of request ACK sender
    ulm_uint32_t destID;		//!< global process id of request ACK receiver
    ulm_uint32_t padding;	
    ulm_uint64_t memNeededSeqOffset;	//!< echoed value of sequential offset of needed memory
    ulm_ptr_t sendMessagePtr;	        //!< a pointer to this message's SendDesc_t (valid for receiver only)
    union {
        ulm_uint32_t wordPtrs[MEMREQ_MAX_WORDPTRS];
        /*
        ulm_uint64_t lwordPtrs[MEMREQ_MAX_LWORDPTRS];
        */
    } memBufPtrs;
    ulm_uint32_t checksum;		//!< additive checksum or CRC of all 128 - 4 bytes of the control message
};

#define CTLHDR_LWORDS 16
#define CTLHDR_WORDS 32
#define CTLHDR_FULLBYTES 128

/* the full control message header */
union quadricsCtlHdr {
    ulm_uint64_t lwords[CTLHDR_LWORDS];
    ulm_uint32_t words[CTLHDR_WORDS];
    unsigned char bytes[CTLHDR_FULLBYTES];
    struct quadricsFullCommon commonHdr;
    struct quadricsDataHdr msgDataHdr;
    struct quadricsDataAck msgDataAck;
    struct quadricsMemRls memRelease;
    struct quadricsMemReq	memRequest;
    struct quadricsMemReqAck memRequestAck;
};

typedef struct quadricsCommon quadricsCommon_t;
typedef struct quadricsFullCommon quadricsFullCommon_t;
typedef struct quadricsDataHdr quadricsDataHdr_t;
typedef struct quadricsDataAck quadricsDataAck_t;
typedef struct quadricsMemRls quadricsMemRls_t;
typedef struct quadricsMemReq quadricsMemReq_t;
typedef struct quadricsMemReqAck quadricsMemReqAck_t;
typedef union quadricsCtlHdr quadricsCtlHdr_t;

#endif
