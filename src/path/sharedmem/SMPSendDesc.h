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



#ifndef _SMPSENDDESC
#define _SMPSENDDESC

#include <new>

#include "util/Lock.h"
#include "util/DblLinkList.h"
#include "SMPFragDesc.h"

// forward declaration
class RecvDesc_t;

class SMPSendDesc_t : public Links_t
{
public:

    //! pointer to the base address of the data
    void *AppAddr;

    // function to set message length - this way can reuse most code,
    //   regardless of how data is passed in from the user app
    void SetAppAddr(void *Addr) { AppAddr=Addr; }

    // send function
    int Send();

    // cleanup function
    void ReturnDesc();

    // copyin function
    void CopyToULMBuffers(SMPFragDesc_t *descriptor);
    void CopyToULMBuffers(SMPSecondFragDesc_t *descriptor);

    //! list of frags that are ready to send
    DoubleLinkList fragsReadyToSend;

    //! double link list of free fragements
    DoubleLinkList freeFrags;

    //! pointer to matched receive descriptor
    volatile RecvDesc_t *matchedRecv;

    /*! Pointer to the ULMType_t datatype descriptor associated
     *  with this message... NULL if the user is sending raw
     *  bytes */
    ULMType_t *datatype;

    /*! Array containing the sequential offsets (i.e. packed
     *  offsets) of each pair in the datatype's typemap */
    size_t *tmap_seq_off;

    //! pointer to first frag descriptor
    SMPFragDesc_t *firstFrag;

    // number of frags that will be sent
    unsigned numfrags;

    // number of frag descriptors allocated
    volatile unsigned NumFragDescAllocated;

    // the number that have had the 'action' applied
    volatile unsigned NumSent;

    //  number of acks received - when this hits zero, you can free it
    volatile unsigned NumAcked;

    // lenght of the data
    unsigned long PostedLength;

    // destination process ProcID
    unsigned int dstProcID_m;

    // user specified tag
    int tag_m;

    // communicator tag
    short communicator;

    // library specified tag
    unsigned long isendSeq_m;

    // maximum outstanding frags - for flow control
    int maxOutstandingFrags;

    //! pointer to associated request object
    RequestDesc_t *requestDesc;

    //! send type
    int sendType;

    //! has messageDone been set upon send completion
    int sendDone;

    // lock for this IoVec
    Locks SenderLock;

    // Ctor
    SMPSendDesc_t() { WhichQueue=SMPFREELIST; }

    // Construct a new descriptor and initialize locks.
    SMPSendDesc_t(int plIndex);

    //! initialization function - first frag is posted on the receive
    //!   side in this routine
    int Init(int DestinationProcess, size_t len, int tag_m,
             int comm, ULMType_t *dtype);

    // flag for flow control, frags are only sent when true
    bool clearToSend_m;

    // bsend buffer allocation offset
    ssize_t bsendOffset;
};

#endif // !_SMPSENDDESC
