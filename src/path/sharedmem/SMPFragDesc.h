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

#ifndef _SMPFRAGDESC
#define _SMPFRAGDESC

#include "internal/system.h"
#include "path/sharedmem/sendInfo.h"
#include "internal/constants.h"

// forward declaration
class RecvDesc_t;


class SMPFragDesc_t : public Links_t
{
public:

    // Data members

	//  flag indicating all fragments have been sent - optimization
	int allFragsSent;

    // total length of user data (this frag) after fragment is
    // matched, this becomes the counter for the amount of data that
    // has arrived.
    unsigned long length_m;

    // frag index within the message
    unsigned int fragIndex_m;

    // total message length
    unsigned long msgLength_m;

    // base address - for data received off "net" this is the address to copy from
    //                for posted receives, this is the base address to copy to
    void *addr_m;

    // sending process ProcID
    unsigned int srcProcID_m;

    // destination process ProcID
    unsigned int dstProcID_m;

    // user specified tag
    int tag_m;

    // communicator tag
    int ctx_m;

    // comm type ID (point-to-point, collective, etc.)
    int msgType_m;

    // lock for this frag
    Locks lock_m;

    // offset within message data (pretend we have
    // a contiguous buffer that holds all message data)
    size_t seqOffset_m;

    // Index into typemap, used for non-contiguous data
    int tmapIndex_m;

    //resource pool index
    int poolIndex_m;

    // matched recieve
    volatile RecvDesc_t *matchedRecv_m;

    // flags that indicate state of frag
    int flags_m;

    // needed so that ack can be put directly to his header
    union {
        sharedMemData_t *SMP;
    } SendingHeader_m;

    // ok to access data in library buffers
    volatile bool okToReadPayload_m;

#ifdef _DEBUGQUEUES
    // library specified send tag - assigned by the sender
    unsigned long long isendSeq_m;

    // library specified receive tag - assigned by the receiver, used
    //  to track ireceives
    unsigned long long irecvSeq_m;
#endif                          // _DEBUGQUEUES


    // Methods

    // default constructor
    SMPFragDesc_t ()
        {
            WhichQueue = SMPFREELIST;
        }

    SMPFragDesc_t (int plIndex)
        {
            WhichQueue = SMPFREELIST;
            lock_m.init();
            poolIndex_m = plIndex;
            // set pointer to data
            size_t offset = sizeof(SMPFragDesc_t);
            offset = ((offset + CACHE_ALIGNMENT - 1) / CACHE_ALIGNMENT) + 1;
            offset *= CACHE_ALIGNMENT;
            addr_m = ((char *) this) + offset;
        }
};



// frag descriptor for 2nd, 3rd,... frag - includes payload buffer
class SMPSecondFragDesc_t : public Links_t {

public:

    // Data members

    // total length of user data (this frag)
    //  after gragment is matched, this becomes the conter for the
    //  amount of data that has arrived.
    unsigned long length_m;

    // total message length
    unsigned long msgLength_m;

    // base address - for data received off "net" this is the address to copy from
    //                for posted receives, this is the base address to copy to
    void *addr_m;

    // offset within message data (pretend we have
    // a contiguous buffer that holds all message data)
    size_t seqOffset_m;

    // Index into typemap, used for non-contiguous data
    int tmapIndex_m;

    // needed so that ack can be put directly to his header
    union {
        BaseSendDesc_t *SMP;
    } SendingHeader_m;

    // matched recieve
    volatile RecvDesc_t *matchedRecv_m;

    // Methods

    // default constructor
    SMPSecondFragDesc_t()
        {
            WhichQueue = SMPFREELIST;
        }

    SMPSecondFragDesc_t(int plIndex)
        {
            WhichQueue = SMPFREELIST;
            // set pointer to data
            size_t offset = sizeof(SMPSecondFragDesc_t);
            offset = ((offset + CACHE_ALIGNMENT - 1) / CACHE_ALIGNMENT) + 1;
            offset *= CACHE_ALIGNMENT;
            addr_m = ((char *) this) + offset;
        }
};

#endif /* !_SMPFRAGDESC */
