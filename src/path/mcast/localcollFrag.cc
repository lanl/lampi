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

#include <stdio.h>
#include <unistd.h>
#include <strings.h>

#include "queue/globals.h"
#include "path/mcast/localcollFrag.h"
#include "path/mcast/state.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"

int maxOutstandingLCFDFrags = 20;

/*Copy data from library buffers into user buffers. */
unsigned int LCFD::CopyFunction(void *fragAddr, void *appAddr, ssize_t length)
{
    if( length == 0 ) {
	return 0;
    }

    // copy data out
    bcopy(fragAddr, appAddr, length);

    // return length
    return 0;
}

/* Ack frag */
bool LCFD::AckData(double timeNow)
{
    //! get pool index
    int plIndex;
#ifdef USE_DEST_MEM
    plIndex = getMemPoolIndex();
#else
    plIndex = poolIndex_m;
#endif

    // increment NumAcked for flow control purposes
    // we use the lower level BaseSendDesc_t lock
    // instead of the UtsendDesc_t lock as only
    // receivers touch NumAcked (sending threads use
    // the Utsend.. lock for access control)
    SendingHeader->Lock.lock();
    (SendingHeader->NumAcked)++;
    SendingHeader->clearToSend_m = true;
    // if this is the last frag, then we must remove
    // the BaseSendDesc_t *SendingHeader from the
    // multicast send message descriptor unackedPt2PtMessages queue
    if (SendingHeader->NumAcked == SendingHeader->numfrags) {
        UtsendDesc_t *msendDesc = SendingHeader->multicastMessage;
        msendDesc->Lock.lock();
        msendDesc->unackedPt2PtMessages.RemoveLinkNoLock(SendingHeader);
        SendingHeader->ReturnDesc(global_to_local_proc(srcProcID_m));
        msendDesc->Lock.unlock();
    }
    SendingHeader->Lock.unlock();

    // return frag memory to pool
    // addr may be zero if zero-length message
    if (addr_m) {
        SMPSharedMemDevs[plIndex].MemoryBuckets.ULMFree(addr_m);
        addr_m = 0;
    }

    return true;
}

/*
 * Return descriptor to pool
 */
void LCFD::ReturnDescToPool(int PoolIndex)
{
    // update WhichQueue pointer
    WhichQueue = SMPFREELIST;

    //! get pool index
    int plIndex;
#ifdef USE_DEST_MEM
    plIndex = PoolIndex;
#else
    plIndex = poolIndex_m;
#endif

    // return descriptor
    LocalCollFragDescriptors.returnElement(this, plIndex);
}
