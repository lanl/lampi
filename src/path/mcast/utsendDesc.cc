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

#include <new.h>
#include <stdio.h>

#include "queue/globals.h"
#include "queue/sender_ackinfo.h"
#include "queue/globals.h"
#include "path/mcast/utsendInit.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "path/mcast/utsendDesc.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "path/common/InitSendDescriptors.h"
#include "path/common/path.h"

#ifdef RELIABILITY_ON
#include "util/dclock.h"
#endif

//#define DEBUGBPIO
#ifdef DEBUGBPIO
int bpiodebuglevel = 0;
#define DEBUG(lvl, x) if ((lvl) < bpiodebuglevel) x
#else
#define DEBUG(lvl, x)
#endif

/*
  static int StartingIndex[128] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };
*/

UtsendDesc_t::UtsendDesc_t()
{
    // zero out members
    request = 0;
    mcastInfo = 0;
    mcastInfoLen = 0;
    nextToAllocate = 0;
    localSourceRank = 0;
    contextID = 0;
    messageDoneCount = 0;
    numDescsToAllocate = 0;
    sendDone = false;

#ifdef RELIABILITY_ON
    earliestTimeToResend = -1.0;
#endif

    WhichQueue = UTSENDDESCFREELIST;
    datatype = NULL;
    AppAddr = 0;

    // initialize descriptor lock
    Lock.init();

    // initialize queue locks
    incompletePt2PtMessages.Lock.init();
    unackedPt2PtMessages.Lock.init();
}

UtsendDesc_t::UtsendDesc_t(int plIndex)
{
    // zero out members
    request = 0;
    mcastInfo = 0;
    mcastInfoLen = 0;
    nextToAllocate = 0;
    localSourceRank = 0;
    contextID = 0;
    messageDoneCount = 0;
    numDescsToAllocate = 0;
    sendDone = false;

#ifdef RELIABILITY_ON
    earliestTimeToResend = -1.0;
#endif

    WhichQueue = UTSENDDESCFREELIST;
    datatype = NULL;
    AppAddr = 0;

    // initialize descriptor lock
    Lock.init();

    // initialize queue locks
    incompletePt2PtMessages.Lock.init();
    unackedPt2PtMessages.Lock.init();
}

int UtsendDesc_t::Init(RequestDesc_t *req, ULMMcastInfo_t *ci,
                             int ciLen)
{
    // Initialize data members
    request = req;
    mcastInfo = ci;
    mcastInfoLen = ciLen;
    nextToAllocate = 0;
    AppAddr = req->pointerToData;
    datatype = req->datatype;
    contextID = req->ctx_m;
    messageDoneCount = 0;
    numDescsToAllocate = 0;
    sendDone = false;

    for (int i = 0; i < mcastInfoLen; i++) {
        if (mcastInfo[i].nGroupProcIDOnHost > 0)
            numDescsToAllocate++;
    }

#ifdef RELIABILITY_ON
    earliestTimeToResend = -1.0;
#endif

    // put on incomplete queue
    int mp = local_myproc();
    localSourceRank = mp;
    WhichQueue = INCOMUTSENDDESC;
    IncompleteUtsendDescriptors[mp]->Append(this);

    return ULM_SUCCESS;
}

int UtsendDesc_t::allocateSendDescriptors()
{
    int errorCode = ULM_SUCCESS;
    Communicator *comm = communicators[contextID];
    BaseSendDesc_t *pt2ptSendDesc;

    // Loop over the mcastInfo array to allocate send
    // descriptors for each host's hostCommRoot process
    while (nextToAllocate < mcastInfoLen) {
        // skip any host entries w/o recipients
        if (mcastInfo[nextToAllocate].nGroupProcIDOnHost <= 0) {
            nextToAllocate++;
            continue;
        }
        // allocate a BaseSendDesc_t
        pt2ptSendDesc =
            _ulm_SendDescriptors.getElement(getMemPoolIndex(), errorCode);
        if (!pt2ptSendDesc) {
            return errorCode;
        }
        // set common fields
        pt2ptSendDesc->clearToSend_m = true;
        pt2ptSendDesc->requestDesc = 0;
        pt2ptSendDesc->dstProcID_m =
            comm->remoteGroup->groupHostData[comm->remoteGroup->
                                             groupHostDataLookup[mcastInfo
                                                                 [nextToAllocate].
                                                                 hostID]].
            hostCommRoot;
        pt2ptSendDesc->multicastRefCnt =
            mcastInfo[nextToAllocate].nGroupProcIDOnHost;
        pt2ptSendDesc->tag_m = request->posted_m.UserTag_m;
        pt2ptSendDesc->PostedLength = request->posted_m.length_m;
        pt2ptSendDesc->ctx_m = contextID;
        pt2ptSendDesc->sendType = request->sendType;
        pt2ptSendDesc->datatype = request->datatype;
        pt2ptSendDesc->NumAcked = 0;
        pt2ptSendDesc->NumSent = 0;
        pt2ptSendDesc->NumFragDescAllocated = 0;
        pt2ptSendDesc->numFragsCopiedIntoLibBufs_m = 0;
        pt2ptSendDesc->sendDone = 0;
        pt2ptSendDesc->path_m = 0;
        pt2ptSendDesc->multicastMessage = this;
        pt2ptSendDesc->AppAddr = AppAddr;
#ifdef RELIABILITY_ON
        pt2ptSendDesc->earliestTimeToResend = -1.0;
#endif

        // bind descriptor to path
        errorCode =
            (*(comm->multicastPathSelectionFunction)) (this,
                                                       pt2ptSendDesc);
        if (errorCode != ULM_SUCCESS) {
            pt2ptSendDesc->ReturnDesc();
            return errorCode;
        }
        // do path specific initialization
        pt2ptSendDesc->path_m->init(pt2ptSendDesc);

        // append descriptor to incompletePt2PtMessages
        // we don't need to lock since we have the UtsendDesc_t locked...
        pt2ptSendDesc->WhichQueue = INCOMPLETEISENDQUEUE;
        incompletePt2PtMessages.AppendNoLock(pt2ptSendDesc);

        nextToAllocate++;
    }

    return errorCode;
}

int UtsendDesc_t::send(bool lockIncompleteList)
{
    Communicator *comm = communicators[contextID];
    BaseSendDesc_t *tmpDesc;

    // make progress fragmenting message
    if (nextToAllocate < mcastInfoLen) {
        int retVal = allocateSendDescriptors();
        if (retVal != ULM_SUCCESS) {
            return ULM_ERROR;
        }
    }
    // iterate over incompletePt2PtMessages queue, and call path_m->send
    // for each one; rebinding to a new path, if necessary
    for (BaseSendDesc_t *desc =
             (BaseSendDesc_t *) incompletePt2PtMessages.begin();
         desc != (BaseSendDesc_t *) incompletePt2PtMessages.end();
         desc = (BaseSendDesc_t *) desc->next) {
        int sendReturn;
        bool incomplete;
        while (!desc->path_m->send(desc, &incomplete, &sendReturn)) {
            if (sendReturn == ULM_ERR_BAD_PATH) {
                // unbind from the current path
                desc->path_m->unbind(desc, (int *) 0, 0);
                // select a new path
                sendReturn =
                    (*(comm->multicastPathSelectionFunction)) (this, desc);
                if (sendReturn != ULM_SUCCESS) {
                    desc->ReturnDesc();
                    ulm_exit((-1, "Error: cannot find path for multicast "
                              "pt2pt host message.\n"));
                }
                // initialize the descriptor for this path
                desc->path_m->init(desc);
            } else {
                // unbind should empty desc of frag descriptors, etc.
                desc->path_m->unbind(desc, (int *) 0, 0);
                desc->ReturnDesc();
                return sendReturn;
            }
        }
        if (!incomplete) {
            // move descriptor to unackedPt2PtMessages
            tmpDesc =
                (BaseSendDesc_t *) incompletePt2PtMessages.
                RemoveLinkNoLock(desc);
            desc->WhichQueue = UNACKEDISENDQUEUE;
            unackedPt2PtMessages.AppendNoLock(desc);
            desc = tmpDesc;
        }
    }


    // check progress and move to the unacked queue if the send
    // is successful so far...
    if (nextToAllocate == mcastInfoLen
        && incompletePt2PtMessages.size() == 0) {
        int mp = localSourceRank;
        // lock the lists
        if (lockIncompleteList)
            IncompleteUtsendDescriptors[mp]->Lock.lock();
        UnackedUtsendDescriptors[mp]->Lock.lock();

        // move to unacked list
        IncompleteUtsendDescriptors[mp]->RemoveLinkNoLock(this);
        UnackedUtsendDescriptors[mp]->AppendNoLock(this);
        WhichQueue = UNACKEDUTSENDDESC;

        // unlock lists
        if (lockIncompleteList)
            IncompleteUtsendDescriptors[mp]->Lock.unlock();
        UnackedUtsendDescriptors[mp]->Lock.unlock();
    }

    return ULM_SUCCESS;
}

int UtsendDesc_t::freeDescriptor()
{
    // check to make sure queues are empty
    if (incompletePt2PtMessages.size() > 0 ||
        unackedPt2PtMessages.size() > 0) {
        return ULM_ERROR;
    }
    // return Descriptor to free list
    WhichQueue = UTSENDDESCFREELIST;
    UtsendDescriptors.returnElement(this);
    return ULM_SUCCESS;
}

#ifdef RELIABILITY_ON
int UtsendDesc_t::reSend()
{
    int errorCode = ULM_SUCCESS;
    BaseSendDesc_t *desc, *tmpDesc;
    double now = dclock();

    // reset UtsendDesc_t time to resend
    earliestTimeToResend = -1.0;

    // iterate over all unackedPt2PtMessages
    for (desc = (BaseSendDesc_t *) unackedPt2PtMessages.begin();
         desc != (BaseSendDesc_t *) unackedPt2PtMessages.end();
         desc = (BaseSendDesc_t *) desc->next) {
        if (now <= desc->earliestTimeToResend) {
            if (desc->path_m->resend(desc, &errorCode)) {
                // move descriptor to incompletePt2PtMessages for later retransmission
                tmpDesc =
                    (BaseSendDesc_t *) unackedPt2PtMessages.
                    RemoveLinkNoLock(desc);
                incompletePt2PtMessages.AppendNoLock(desc);
                desc = tmpDesc;
                continue;
            } else if (errorCode == ULM_SUCCESS) {
                // no need to do anything with this descriptor for the moment
                if (earliestTimeToResend == -1.0)
                    earliestTimeToResend = desc->earliestTimeToResend;
                else if (desc->earliestTimeToResend < earliestTimeToResend)
                    earliestTimeToResend = desc->earliestTimeToResend;
                continue;
            } else if (errorCode == ULM_ERR_BAD_PATH) {
                Communicator *comm = communicators[contextID];
                // unbind from old path
                desc->path_m->unbind(desc, (int *) 0, 0);

                // select new path
                errorCode =
                    (*(comm->multicastPathSelectionFunction)) ((void *)
                                                               this,
                                                               (void *)
                                                               desc);
                if (errorCode != ULM_SUCCESS) {
                    ulm_exit((-1, "UtsendDesc_t::reSend Error - "
                              "cannot find path for host message.\n"));
                }
                // do path specific initialization
                desc->path_m->init(desc);

                // move descriptor to incompletePt2PtMessages for later retransmission
                tmpDesc =
                    (BaseSendDesc_t *) unackedPt2PtMessages.
                    RemoveLinkNoLock(desc);
                incompletePt2PtMessages.AppendNoLock(desc);
                desc = tmpDesc;
            }
        } else {
            if (earliestTimeToResend == -1.0)
                earliestTimeToResend = desc->earliestTimeToResend;
            else if (desc->earliestTimeToResend < earliestTimeToResend)
                earliestTimeToResend = desc->earliestTimeToResend;
        }
    }

    return errorCode;
}
#endif
