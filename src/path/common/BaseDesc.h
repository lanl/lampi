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

#ifndef NET_COMMON_BASEDESC_H
#define NET_COMMON_BASEDESC_H

#include <stdio.h>
#include <string.h>             // for memcpy
#include <sys/types.h>          // For fd_set in BaseRecvFragDesc_t

#include "client/ULMClientTypes.h"
#include "util/DblLinkList.h"
#include "util/MemFunctions.h"
#include "internal/constants.h"
#include "internal/state.h"
#include "ulm/ulm.h"

// network path specific objects embbedded in BaseSendDesc_t
#include "path/udp/sendInfo.h"
#include "path/quadrics/sendInfo.h"
#include "path/sharedmem/sendInfo.h"

#define REQUEST_TYPE_SEND   1
#define REQUEST_TYPE_RECV   2

// forward declarations
class SMPFragDesc_t;
class SMPSecondFragDesc_t;
class BasePath_t;

// BaseSendDesc_t:
//
// A super class for descriptors to manage sent messages.
//
// It stores a pointer to the data structures actually used to send
// data, and to monitor the progress of this specific message.

class BaseSendDesc_t : public RequestDesc_t
{
public:

    // Data members

    BasePath_t *path_m;     // pointer to path object

    // path specific info/data
    union {
        udpSendInfo udp;
        quadricsSendInfo quadrics;
	sharedmemSendInfo sharedmem;
    } pathInfo;

    Locks Lock;                     // lock
    DoubleLinkList FragsToAck;      // double link list of frags that need to be acked
    DoubleLinkList FragsToSend;     // double link list of frags that need to be sent
    RequestDesc_t *requestDesc;     // pointer to associated request object
    ULMType_t *datatype;            // datatype for message (null means raw bytes)
    bool clearToSend_m;             // flag for flow control, frags are only sent when true
    int ctx_m;                      // context ID tag
    int maxOutstandingFrags;        // maximum outstanding frags - for flow control
    int numFragsCopiedIntoLibBufs_m;        // number of frags that have been copied into library buffers
    int sendDone;                   // is send done ?
    int sendType;                   // send type
    int tag_m;                      // user specified tag
    ssize_t bsendOffset;            // bsend buffer allocation offset
    int dstProcID_m;                // destination process ProcID
    unsigned long PostedLength;     // length of the data
    unsigned long isendSeq_m;        // library specified tag
    unsigned numfrags;              // number of frags that will be sent
    void *AppAddr;                  // pointer to the base of the data
    volatile unsigned NumAcked;     //  number of acks received
    volatile unsigned NumFragDescAllocated; // number of frag descriptors allocated
    volatile unsigned NumSent;      // the number that have had the 'action' applied

#ifdef RELIABILITY_ON
    double earliestTimeToResend;
#endif


    // Methods

    // constructor
    BaseSendDesc_t() : clearToSend_m(true)
        {
            // initialize desriptor queue location - assumed to start on the
            // free list
            WhichQueue = SENDDESCFREELIST;
            datatype = NULL;
            path_m = 0;
            AppAddr = 0;
#ifdef RELIABILITY_ON
            earliestTimeToResend = -1;
#endif
        }

    // Construct object and initialize locks
    BaseSendDesc_t(int plIndex) : clearToSend_m(true)
        {
            WhichQueue = SENDDESCFREELIST;
            datatype = NULL;
            Lock.init();
            FragsToSend.Lock.init();
            FragsToAck.Lock.init();
            path_m = 0;
            AppAddr = 0;
#ifdef RELIABILITY_ON
            earliestTimeToResend = -1;
#endif
        }

    // Returns true if okay to send frags
    bool clearToSend()
        {
            return clearToSend_m;
        }
};


// RecvDesc_t:
//
// Descriptor used to track posted receives.

class RecvDesc_t :  public RequestDesc_t
{
public:
    
    // Data members

    volatile unsigned long DataReceived;    // Amount of data received
    volatile unsigned long DataInBitBucket; // Amount of data ignored if PostedLength < ReceivedMessageLength
    unsigned long PostedLength;             // length of the data -posted on receive side
    unsigned long ReceivedMessageLength;    // total received message length
    unsigned long actualAmountToRecv_m;     // actual amount to receive (min of send and recv)
    int srcProcID_m;                        // sending process ProcID
    int tag_m;                              // user specified tag
    unsigned long long isendSeq_m;          // library specified isend tag
    unsigned long long irecvSeq_m;          // library specified irecv tag
    int messageType;                        // communication type ID (point-to-point, collective, etc.)
    int ctx_m;                              // context ID
    void *AppAddr;                          // pointer to the base of the data
    Locks Lock;                             // lock

#ifdef DEBUG_DESCRIPTORS
    double t0;
    double t1;
    double t2;
    double t3;
    double t4;
    double t5;
    double t6;
    double t7;
#endif

    // Methods

    // Copy to App buffers
    ssize_t CopyToApp(void *FrgDesc, bool *recvDone = 0);
    ssize_t CopyToAppLock(void *FrgDesc, bool *recvDone = 0);

#ifdef SHARED_MEMORY
    // SMP copy to app buffers
    int SMPCopyToApp(unsigned long sequentialOffset, unsigned long fragLen,
                     void *fragAddr, unsigned long sendMessageLength, int *recvDone);
#endif // SHARED_MEMORY

    // set request object and return descriptor to pool
    void setRequestReturnDesc()
        {
            reslts_m.UserTag_m = tag_m;
            reslts_m.length_m = ReceivedMessageLength;
            reslts_m.lengthProcessed_m = DataReceived;
            messageDone = MESSAGE_COMPLETE;
        }

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

    BaseSendDesc_t *parentSendDesc_m;       // pointer to "owning" message descriptor
    void *currentSrcAddr_m;                 // pointer into source buffer
    size_t seqOffset_m;                     // sequential offset into packed buffer
    size_t length_m;                        // length of fragment
    BasePath_t *failedPath_m;               // failed path pointer
    packType packed_m;                      // type of fragment
    int numTransmits_m;                     // count transmission attempts
    int tmapIndex_m;                        // type map index

    // methods
};


// BaseRecvFragDesc_t:
//
// A super class for path-specific receive fragment descriptors

class BaseRecvFragDesc_t : public Links_t
{
public:

    // Data members

    Locks lock_m;
    void *addr_m;                // base address - for data received off "net" this is the address to copy from
                                 //                for posted receives, this is the base address to copy to
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
    unsigned long isendSeq_m;     // message sequence number (assigned by the sender)
    bool isDuplicate_m;          // is this frag a duplicate?
    int msgType_m;               // comm type ID (point-to-point, collective, etc.)
    int refCnt_m;                // reference count for frag
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
            isDuplicate_m = false;
        }

    // constructor that initializes locks
    BaseRecvFragDesc_t(int plIndex)
        {
            lock_m.init();
            poolIndex_m = plIndex;
        }

    // send ack
    virtual bool AckData(double timeNow = -1.0)
        {
            return true;
        }

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
};


#endif // NET_COMMON_BASEDESC_H
