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


#ifndef UPD_RECV_DESC_H_
#define UPD_RECV_DESC_H_

#include "path/udp/sendFrag.h"		// needed for udpSendFragDesc and iovecs
#include "path/common/BaseDesc.h"	// needed for BaseRecvDesc_t

//
// The maximum payload size of short messages.  Size should be of the order
// of sizeof(udp_header).
//
const unsigned
long MaxShortPayloadSize = MaxShortFragSize_g - sizeof(udp_header);

class udpRecvFragDesc : public BaseRecvFragDesc_t
{
public:

    // static
    static udpRecvFragDesc* getRecvDesc();

    // constuctor
    udpRecvFragDesc() : sockfd(0), shortMsg(true), copyError(false),
	pt2ptNonContig(false)
        {
#ifdef _DEBUGQUEUES
            WhichQueue = UDPRECVFRAGSFREELIST;
#endif // _DEBUGQUEUES
            IOVecs = 0;
            IOVecsSize = 0;
            IOVecsCnt = 0;
        }

    // constructor which sets locks (if used)
    udpRecvFragDesc(int plIndex)
        : sockfd(0), shortMsg(true), copyError(false),
        pt2ptNonContig(false)
        {
#ifdef _DEBUGQUEUES
            WhichQueue = UDPRECVFRAGSFREELIST;
#endif // _DEBUGQUEUES
            IOVecs = 0;
            IOVecsSize = 0;
            IOVecsCnt = 0;
        }

    static int pullFrags(int& errorReturn);

    // Copy data to App buffers
    virtual unsigned int CopyFunction(void* appAddr,
                                      void* fragAddr, ssize_t length);

    // non-contiguous Copy data to App buffers (with hooks for partial cksum word return)
    virtual unsigned long nonContigCopyFunction(void* appAddr, void* fragAddr,
                                                ssize_t length, ssize_t cksumlength,
                                                unsigned int* cksum,
                                                unsigned int* partialInt,
                                                unsigned int* partialLength,
                                                bool firstCall,
                                                bool lastCall);

    virtual unsigned long dataOffset()
        {
            if (fragIndex_m == 0) {
                return 0;
            } else {
                return maxShortPayloadSize_g + maxPayloadSize_g*(fragIndex_m - 1);
            }
        }

    // check data
    virtual bool CheckData(unsigned int checkSum, ssize_t len);

    ssize_t handleShortSocket();
    ssize_t handleLongSocket();
    void processMessage(udp_message_header& hdr);
    void processAck(udp_ack_header& ack);
#ifdef ENABLE_RELIABILITY
    bool isDuplicateCollectiveFrag();
    bool checkForDuplicateAndNonSpecificAck(udpSendFragDesc *Frag, udp_ack_header &ack);
#endif
    void handlePt2PtMessageAck(BaseSendDesc_t *sendDesc,
                               udpSendFragDesc *Frag, udp_ack_header &ack);

    // function to copy data from library space to user space
    //   ssize_t CopyOut(void *FradDesc);

    // function to send ack
    bool AckData(double timeNow = -1.0);

    // function to return receive descriptor to appropriate free pool
    void ReturnDescToPool(int LocalRank);

    int sockfd;				// socket descriptor
    bool shortMsg;			// true if this is a short msg
    bool copyError;			// true if there is an error in copying the data out
    bool pt2ptNonContig;		// true if this frag is part of a non-contiguous data pt-to-pt msg.
    bool dataReadFromSocket;	// true if the data has been read from the associated socket
    ulm_iovec_t *IOVecs;		// pointer to process private memory for recvmsg iovecs if needed
    size_t IOVecsSize;		// number of recvmsg iovecs available in IOVecs array
    size_t IOVecsCnt;		// number of recvmsg iovecs currently used
    int smpPoolIndex;			// index of pool for multicast long message payload buffers
    udp_header header;			// header for this message frag
    char data[MaxShortPayloadSize];	// storage for short messages
};

#endif /* UPD_RECV_DESC_H_ */
