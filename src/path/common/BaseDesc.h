/*
 * Copyright 2002-2003. The Regents of the University of
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

#ifndef NET_COMMON_BASEDESC_H
#define NET_COMMON_BASEDESC_H

#include <stdio.h>
#include <string.h>             // for memcpy
#include <sys/types.h>          // For fd_set in BaseRecvFragDesc_t

#include "util/DblLinkList.h"
#include "util/MemFunctions.h"
#include "internal/constants.h"
#include "internal/state.h"
#include "os/atomic.h"
#include "ulm/ulm.h"
#include "queue/ReliabilityInfo.h"

// network path specific objects embbedded in SendDesc_t
#include "path/udp/sendInfo.h"
#include "path/quadrics/sendInfo.h"
#include "path/sharedmem/sendInfo.h"
#include "path/ib/sendInfo.h"
#include "queue/SharedMemForCollective.h"

enum RequestType_t {
    REQUEST_TYPE_SEND = 1,
    REQUEST_TYPE_RECV = 2,
    REQUEST_TYPE_BCAST = 3,
    REQUEST_TYPE_SHARE = 4 
};

enum RequestState_t {
    REQUEST_INCOMPLETE=0,
    REQUEST_COMPLETE,
    REQUEST_RELEASED
};

/* ack macros */
#define ACKSTATUS_DATAGOOD 0x1			//!< data arrived okay (value for ackStatus below)
#define ACKSTATUS_DATACORRUPT 0x2		//!< data arrived corrupted (value for ackStatus below)
#define ACKSTATUS_AGGINFO_ONLY 0x4		//!< ack fields for a specific frag are not valid (OR'ed with ackStatus)

// forward declarations
class SMPFragDesc_t;
class SMPSecondFragDesc_t;
class BasePath_t;
typedef struct BaseAck BaseAck_t;


// RequestDesc_t:
//
// A super class for both send and receive descriptors

struct RequestDesc_t : public Links_t {

    struct Status_t {
        size_t length_m;	// length of request
        int peer_m;	        // peer process (source for recv, destination for send)
        int tag_m;	        // user tag
    };

    ULMType_t *datatype;        // datatype (NULL => contiguous)
    int ctx_m;                  // communicator ID
    int requestType;            // request type - send or receive
    int status;                 // indicates request status
    bool persistent;            // persistence flag
    bool freeCalled;            // true when ulm_request_free is
                                // called before recv. request complete
    bool persistFreeCalled;     // used for tracking when user calls
                                // request_free on persistent requests
    volatile int messageDone;   // message completion flag

    // default constructor
    RequestDesc_t()
        {
        }

    // constructor
    RequestDesc_t(int poolIndex)
        {
            WhichQueue = REQUESTFREELIST;
        }

    virtual void shallowCopyTo(RequestDesc_t *request)
        {
            /*
             * Copies field values of current object to passed request.
             * This is mainly used for persistent requests where the
             * underlying send descriptor could change (refer to
             * ulm_start() logic).
             */
            request->datatype = datatype;
            request->ctx_m = ctx_m;
            request->requestType = requestType;
            request->persistent = persistent;

            ulm_type_retain(datatype);
        }
};


// SendDesc_t:
//
// A super class for descriptors to manage sent messages.
//
// It stores a pointer to the data structures actually used to send
// data, and to monitor the progress of this specific message.

class SendDesc_t : public RequestDesc_t
{
public:

    // Data members

    BasePath_t * path_m;        // pointer to path object

    // path specific info/data
    union {
        udpSendInfo udp;
        quadricsSendInfo quadrics;
        sharedmemSendInfo sharedmem;
        ibSendInfo ib;
    } pathInfo;

    Locks Lock;                 // lock
    DoubleLinkList FragsToAck;  // double link list of frags that need to be acked
    DoubleLinkList FragsToSend; // double link list of frags that need to be sent
    bool clearToSend_m;         // flag for flow control, frags are only sent when true
    unsigned long isendSeq_m;   // library specified tag
    void *addr_m;              // pointer to the base of the data
    unsigned numfrags;          // number of frags that will be sent
    volatile unsigned NumAcked; //  number of acks received
    volatile unsigned NumFragDescAllocated;     // number of frag descriptors allocated
    volatile int NumSent;       // the number that have had the 'action' applied
    int maxOutstandingFrags;    // maximum outstanding frags - for flow control
    Status_t posted_m;

#ifdef ENABLE_RELIABILITY
    double earliestTimeToResend;
#endif

    int sendType;               // send type - normal, buffered, synchronous, or ready
    ssize_t bsendOffset;        // bsend buffer allocation offset
    void *appBufferPointer;     // set to point to the original buffer (which
    // can be different from addr_m -- MPI_Bsend)
    ULMType_t *bsendDatatype;   // original datatype (NULL => contiguous)
    size_t bsendBufferSize;     // the size of the buffer needed to pack the bsend data

    int bsendDtypeCount;        // datatype element count of original bsend data

    // Methods

    // constructor
    SendDesc_t():clearToSend_m(true)
        {
            // initialize desriptor queue location - assumed to start on the
            // free list
            WhichQueue = SENDDESCFREELIST;
            datatype = NULL;
            bsendDatatype = NULL;
            path_m = 0;
            addr_m = 0;
            NumAcked = 0;
            NumSent = 0;
            NumFragDescAllocated = 0;
            numfrags = 0;
#ifdef ENABLE_RELIABILITY
            earliestTimeToResend = -1;
#endif
        }

    // Construct object and initialize locks
    SendDesc_t(int plIndex):clearToSend_m(true)
        {
            WhichQueue = SENDDESCFREELIST;
            datatype = NULL;
            bsendDatatype = NULL;
            Lock.init();
            FragsToSend.Lock.init();
            FragsToAck.Lock.init();
            NumAcked = 0;
            NumSent = 0;
            NumFragDescAllocated = 0;
            path_m = 0;
            addr_m = 0;
            numfrags = 0;
#ifdef ENABLE_RELIABILITY
            earliestTimeToResend = -1;
#endif
        }

    // Returns true if okay to send frags
    bool clearToSend()
        {
            return clearToSend_m;
        }

    virtual void shallowCopyTo(RequestDesc_t *request);
};

// RecvDesc_t:
//
// Descriptor used to track posted receives.

class RecvDesc_t : public RequestDesc_t
{
public:
    
    // Data members

    unsigned long long isendSeq_m;          // library specified isend tag
    unsigned long long irecvSeq_m;          // library specified irecv tag
    void *addr_m;                          // pointer to the base of the data
    Status_t posted_m;
    Status_t reslts_m;
    volatile unsigned long DataReceived;    // Amount of data received
    volatile unsigned long DataInBitBucket; // Amount of data ignored if PostedLength < ReceivedMessageLength

    Locks Lock;                             // lock

    // Methods

    // Copy to App buffers
    ssize_t CopyToApp(void *FrgDesc, bool *recvDone = 0);
    ssize_t CopyToAppLock(void *FrgDesc, bool *recvDone = 0);

    void DeliveredToApp(unsigned long bytesCopied, unsigned long bytesDiscarded, bool* recvDone = 0);
    void DeliveredToAppLock(unsigned long bytesCopied, unsigned long bytesDiscarded, bool* recvDone = 0);

#ifdef ENABLE_SHARED_MEMORY
    // SMP copy to app buffers
    int SMPCopyToApp(unsigned long sequentialOffset, unsigned long fragLen,
                     void *fragAddr, unsigned long sendMessageLength, int *recvDone);
#endif // SHARED_MEMORY

    // called by copy to app routine
    void requestFree(void);

    // default constructor
    RecvDesc_t()
        {
            WhichQueue = IRECVFREELIST;
            Lock.init();
        }

    // constructor that initializes locks
    RecvDesc_t(int plIndex)
        {
            WhichQueue = IRECVFREELIST;
            Lock.init();
        }

    // d'tor
    virtual ~RecvDesc_t()
        {
        }
};


// BaseSendFragDesc_t:
//
// A super class for path-specific send fragment descriptors

class BaseSendFragDesc_t : public Links_t
{
public:
    enum packType { PACKED, UNPACKED };

    // data members

    SendDesc_t *parentSendDesc_m; // pointer to "owning" message descriptor
    void *currentSrcAddr_m;     // pointer into source buffer
    size_t seqOffset_m;         // sequential offset into packed buffer
    size_t length_m;            // length of fragment
    BasePath_t *failedPath_m;   // failed path pointer
    packType packed_m;          // type of fragment
    int numTransmits_m;         // count transmission attempts
    int tmapIndex_m;            // type map index
    int globalDestProc_m;       // global destination process ID
    double timeSent_m;
    bool	sendDidComplete_m;      // true if the device successfully sent the frag
    unsigned long long fragSeq_m;   // duplicate/dropped detection sequence number

    // methods
    virtual bool init()
    {
        sendDidComplete_m = false;
        
        return true;
    }
    
    virtual int globalDestProc() {return globalDestProc_m;}
    virtual unsigned long isendSequence() {return parentSendDesc_m->isendSeq_m;}

    virtual unsigned long long fragSequence() {return fragSeq_m;}
    virtual void setFragSequence(unsigned long long seq) {fragSeq_m = seq;}
    
    virtual void freeResources(double timeNow, SendDesc_t *bsd) {}

    
    bool sendDidComplete() {return sendDidComplete_m;}
    void setSendDidComplete(bool tf) {sendDidComplete_m = tf;}    
    
    
#ifdef ENABLE_RELIABILITY

    
    virtual void initResendInfo(SendDesc_t *message, double timeNow)
    {
        unsigned long long max_multiple;
        double timeToResend;
        
        timeSent_m = timeNow;
        numTransmits_m++;
        max_multiple =
            (numTransmits_m < MAXRETRANS_POWEROFTWO_MULTIPLE) ?
            (1 << numTransmits_m) :
            (1 << MAXRETRANS_POWEROFTWO_MULTIPLE);
        timeToResend = timeSent_m + (RETRANS_TIME * max_multiple);
        if (message->earliestTimeToResend == -1) {
            message->earliestTimeToResend = timeToResend;
        } else if (timeToResend < message->earliestTimeToResend) {
            message->earliestTimeToResend = timeToResend;
        }
    }

#endif
    
};

enum { UNIQUE_FRAG=0, DUPLICATE_RECEIVED=1, DUPLICATE_DELIVERD=2 };



// BaseRecvFragDesc_t:
//
// A super class for path-specific receive fragment descriptors

class BaseRecvFragDesc_t : public Links_t
{
public:

    // Data members

    Locks lock_m;
    void *addr_m;                // base address - for data received
                                 // off "net" this is the address to
                                 // copy from for posted receives,
                                 // the base address to copy to
    size_t length_m;             // length of fragment filled so far (after matching)
    size_t seqOffset_m;          // offset into message data (as if it were contiguous)
    unsigned long msgLength_m;   // total message length
    unsigned int fragIndex_m;    // fragment index within the message
    unsigned int maxSegSize_m;   // maximum segment size - this is the stripe size used for all but
                                 // the last frag of a message.
    int srcProcID_m;             // sending process ProcID       
    int dstProcID_m;             // destination process ProcID
    int tag_m;                   // user specified tag
    int ctx_m;                   // context ID (communicator index)
    unsigned long long seq_m;    // sequence number of this frag
    unsigned long isendSeq_m;    // message sequence number (assigned by the sender)
    int isDuplicate_m;           // is this frag a duplicate?
    bool DataOK;                 // is the data that arrived ok ?
    int msgType_m;               // comm type ID (point-to-point, collective, etc.)
    int poolIndex_m;             //resource pool index

#ifdef DEBUG_DESCRIPTORS
    double t0;
    double t1;
    double t2;
    double t3;
    double t4;
#endif

    // Methods

    // default constructor
    BaseRecvFragDesc_t()
        {
            // set fragment sequence number to indicate it is not being used
            // unless overwritten
            seq_m = (unsigned long long) 0;
            isDuplicate_m = UNIQUE_FRAG;
        }

    // constructor that initializes locks
    BaseRecvFragDesc_t(int plIndex)
        {
            lock_m.init();
            poolIndex_m = plIndex;
        }

    virtual bool Init()
    {
        isDuplicate_m = UNIQUE_FRAG;
        return true;
    }
    
    // send ack
    virtual bool AckData(double timeNow = -1.0)
        {
            return true;
        }

    // process received fragment sequence range
    int BaseRecvFragDesc_t::processRecvDataSeqs(BaseAck_t *ackPtr, 
		    int glSourceProcess, ReliabilityInfo *reliabilityData);

    // return descriptor to descriptor pool
    virtual void ReturnDescToPool(int LocalRank) {}

    // copy function - specific to communications layer
    virtual unsigned int CopyFunction(void *fragAddr,
                                      void *appAddr,
                                      ssize_t length)
        {
            return (unsigned int) 0;
        }

    // copy function for non-contiguous data with hooks for partial
    // cksum word return
    virtual unsigned long nonContigCopyFunction(void *appAddr,
                                                void *fragAddr,
                                                ssize_t length,
                                                ssize_t cksumlength,
                                                unsigned int *cksum,
                                                unsigned int *partialInt,
                                                unsigned int *partialLength,
                                                bool firstCall,
                                                bool lastCall)
        {
            if (!cksum) {
                memcpy(appAddr, fragAddr, length);
            }
            else if (!usecrc()) {
                *cksum += bcopy_uicsum(fragAddr, appAddr, length, cksumlength,
                                       partialInt, partialLength);
            }
            else {
                if (firstCall) {
                    *cksum = CRC_INITIAL_REGISTER;
                }
                *cksum = bcopy_uicrc(fragAddr, appAddr, length, cksumlength, *cksum);
            }
            return length;
        }

    // mark data as having passed or not the CheckData() call without
    // having to call CheckData() first
    virtual void MarkDataOK(bool okay) {}

    // check data
    virtual bool CheckData(unsigned int checkSum, ssize_t length)
        {
            return true;
        }

    // return the data offset for this frag
    virtual unsigned long dataOffset()
        {
            return fragIndex_m * maxSegSize_m;
        }
    
#ifdef ENABLE_RELIABILITY
    
    virtual unsigned long long ackFragSequence() {return 0;}
    
    virtual unsigned long long ackDeliveredFragSeq() {return 0;}
    
    virtual unsigned long long ackReceivedFragSeq() {return 0;}
        
    /* returns the global Proc ID of sending process */
    virtual int ackSourceProc() {return -1;}
        
    virtual bool checkForDuplicateAndNonSpecificAck(BaseSendFragDesc_t *sfd);
    
#endif      /* ENABLE_RELIABILITY */

    virtual int ackStatus() {return ACKSTATUS_DATAGOOD;}

    virtual void handlePt2PtMessageAck(double timeNow, SendDesc_t *bsd,
                                       BaseSendFragDesc_t *sfd);
    
};



/* base ack class */
struct BaseAck
{
  public:
    ulm_uint32_t type;		 //!< type of message
    ulm_int32_t ctxAndMsgType;	 //!< used for context of global ProcID ids
    ulm_int32_t dest_proc;	 //!< global ProcID of original sending process
    ulm_int32_t src_proc;	 //!< global ProcID of the acknowledging process;
    ulm_ptr_t ptrToSendDesc;	 //!< pointer to original send frag desc
    ulm_uint64_t thisFragSeq;    //!< frag sequence number

    // last consecutive in-order frag received but not necessarily
    // delivered to the src_proc - this sequence indicates frags that do not
    // need to be retransmitted CONDITIONALLY! Therefore this number may
    // decrease, if data is received in error.
    ulm_uint64_t receivedFragSeq; //!< largest in-order rec'd frag sequence

    // last consecutive in-order frag delivered to the src_proc (i.e., copied
    // out of ULM's library buffers successfully) - this sequence indicates
    // data that has been delivered; all send resources may be freed for frags
    // <= delivered_fragseq.
    ulm_uint64_t deliveredFragSeq; //!< largest in-order delivered frag seq

    ulm_int32_t ackStatus;	 //!< GOODACK/NACK of single message frag

};

typedef struct BaseAck BaseAck_t;

#ifdef USE_ELAN_COLL

typedef struct {
    size_t length_m;		// length of request
    union {
        int source_m;		// on a recv this is the source process
        int destination_m;	// on send this is the destination process
    } proc;
    int UserTag_m;		// user tag
} msgRequest;


/*structure that defines a broadcast request */
struct bcast_request_t : public Links_t {

    // Locks Lock;             // object lock

    int seqno;
    double time_started;
    /*sequence number, globally synchronized*/

    void *posted_buff;                // pointer to buffer
    /*void *orig_buff;           */
    // set to point to the original buffer
    CollectiveSMBuffer_t *coll_desc; // descriptor for SMBuffer

    ULMType_t *datatype;       // datatype (NULL => contiguous)
    int   datatypeCount;       // datatype element count

    int   messageType;         // message type - point-to-point or collective
    int   requestType;         // request type - send or receive

    int   sendType;            // normal, buffered, synchronous, or ready
    bool  persistent;          // persistence flag

    int   status;              // indicates request status
    volatile bool ready_toShare;   // Data ready for dispersion in a box
    volatile bool messageDone; // message completion flag

    int   root  ;
    int   comm_root  ;

    msgRequest posted_m;       // set when bound - never modified afterwards
    msgRequest reslts_m;       // values set as for each satisfied request

    void *channel ;            // Bcast Channel descriptor;
    int   ctx_m;               // communicator ID

    /*unsigned long long next_send_time; */

    // default constructor
    bcast_request_t()
    { WhichQueue = QUADRICS_BCAST_LIST; }

    // constructor
    bcast_request_t(int poolIndex)
    { WhichQueue = QUADRICS_BCAST_LIST; }
};
#endif


#endif // NET_COMMON_BASEDESC_H
