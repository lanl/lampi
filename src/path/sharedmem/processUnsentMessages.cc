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

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "util/cbQueue.h"
#include "path/sharedmem/SMPFragDesc.h"
#include "path/sharedmem/SMPSendDesc.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "internal/state.h"
#include "mem/FreeLists.h"
#include "os/atomic.h"
#include "ulm/errors.h"

int processUnsentSMPMessages()
{
    int retVal = ULM_SUCCESS;
    int getSlot;
    int SortedRecvFragsIndex;
    SMPFragDesc_t *TmpDesc;
    SMPSendDesc_t *SendDesc;
    Communicator *commPtr;
    //
    if (SMPSendsToPost[local_myproc()]->size() == 0)
        return ULM_SUCCESS;
    // lock list to make sure reads are atomic
    SMPSendsToPost[local_myproc()]->Lock.lock();
    for (SMPFragDesc_t *
             fragDesc =
             (SMPFragDesc_t *) SMPSendsToPost[local_myproc()]->begin();
         fragDesc !=
             (SMPFragDesc_t *) SMPSendsToPost[local_myproc()]->end();
         fragDesc = (SMPFragDesc_t *) fragDesc->next) {
        SendDesc = fragDesc->SendingHeader_m.SMP;
        commPtr = (Communicator *) communicators[SendDesc->communicator];

        SortedRecvFragsIndex = commPtr->remoteGroup->
            mapGroupProcIDToGlobalProcID[SendDesc->dstProcID_m];
        SortedRecvFragsIndex = global_to_local_proc(SortedRecvFragsIndex);

        if (usethreads())
            getSlot = SharedMemIncomingFrags
                [local_myproc()][SortedRecvFragsIndex]->getSlot();
        else {
            mb();
            getSlot = SharedMemIncomingFrags
                [local_myproc()][SortedRecvFragsIndex]->getSlotNoLock();
            mb();
        }

        if (getSlot != -1) {
            TmpDesc =
                (SMPFragDesc_t *) SMPSendsToPost[local_myproc()]->
                RemoveLinkNoLock(fragDesc);
            // returns the value of the slot that was written too
            retVal =
                SharedMemIncomingFrags[local_myproc()]
                [SortedRecvFragsIndex]->writeToSlot(getSlot, &fragDesc);

            fragDesc = TmpDesc;
        }
    }
    // unlock queue
    SMPSendsToPost[local_myproc()]->Lock.unlock();

    if (retVal > 0)
        retVal = ULM_SUCCESS;

    return retVal;
}
