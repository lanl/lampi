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

#include "queue/globals.h"
#include "util/dclock.h"
#include "path/mcast/utsendInit.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "internal/types.h"
#include "path/common/path.h"
#include "path/common/pathContainer.h"
#include "ulm/ulm.h"

#ifdef UDP
# include "path/udp/UDPNetwork.h"
#endif // UDP

#ifdef SHARED_MEMORY
#include "path/sharedmem/SMPfns.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"

bool _ulm_checkSMPInMakeProgress = true;
#endif // SHARED_MEMORY


#ifdef RELIABILITY_ON
#include "util/DblLinkList.h"
void CheckForCollRetransmits(long mp);
#endif

static int incompleteReqObjCount = 0;

/*
 * variables for debug timing
 */

static long ptCnt = 0;
static double pt[7] = { 0, 0, 0, 0, 0, 0, 0 };
static double t[7] = { 0, 0, 0, 0, 0, 0, 0};

/*!
 *
 *
 *   make library progress
 *    - try and send frags
 *    - try and receive and match new frags
 *
 * \return		ULM return code
 *
 * The root receives bufsize bytes from each proc in the communicator
 * and collates them in communicator rank order in recvbuf which must be
 * of size nproc * bufsize
 *
 */
extern "C" int ulm_make_progress(void)
{
    int returnValue = ULM_SUCCESS;
    long mp = local_myproc();
    int i;

    enum {
        USE_MULTICAST_COMMUNICATIONS = 0,
        DEBUG_TIMES = 0
    };

    if (DEBUG_TIMES) {
        t[1] = dclock();
        ptCnt++;
    }

    if (incompleteReqObjCount++ == 100) {
        incompleteReqObjCount = 0;
        // check unsafe request object freelist
        if (_ulm_incompleteRequests.size() > 0) {
                DoubleLinkList *list = &(_ulm_incompleteRequests);
                RequestDesc_t *req = 0, *tmpreq = 0;
                if (usethreads())
                    list->Lock.lock();
                for (req = (RequestDesc_t *)list->begin(); 
                    req != (RequestDesc_t *)list->end();
                    req = tmpreq) {
                    tmpreq = (RequestDesc_t *)req->next;
                    if (req->messageDone) {
                        // safe to move to allocation freelist
                        list->RemoveLinkNoLock(req);
                        if (usethreads()) {
                            ulmRequestDescPool.returnElement(req);
                        }
                        else {
                            ulmRequestDescPool.returnElementNoLock(req);
                        }
                    }
                }
                if (usethreads())
                    list->Lock.unlock();
        }
    }

    //
    // On host traffic
    //
#ifdef SHARED_MEMORY
    if (_ulm_checkSMPInMakeProgress) {
        // try to make progress with on-host sends
        returnValue = sendOnHostMessages();
        if ((returnValue == ULM_ERR_OUT_OF_RESOURCE) ||
            (returnValue == ULM_ERR_FATAL)) {
            return returnValue;
        }
        // process arriving on host frags
        returnValue = processSMPFrags();
        if (returnValue != ULM_SUCCESS) {
            fprintf(stderr, " processSMPFrags returned %d\n",
                    returnValue);
            fflush(stderr);
            return returnValue;
        }
        // moves frags between SMPSendsToPost  and the fifo
        if (SMPSendsToPost[local_myproc()]->size() != 0) {
            returnValue = processUnsentSMPMessages();
            if (returnValue != ULM_SUCCESS) {
                fprintf(stderr, " processUnsentSMPMessages returned %d\n",
                        returnValue);
                fflush(stderr);
                return returnValue;
            }
        }
    }
#endif

    if (USE_MULTICAST_COMMUNICATIONS) {
        // send/resend incomplete collective messages
        UtsendDesc_t *desc = 0;
        UtsendDesc_t *tmpDesc = 0;
        if (IncompleteUtsendDescriptors[mp]->size() > 0) {
            IncompleteUtsendDescriptors[mp]->Lock.lock();
            for (desc =
                     (UtsendDesc_t *) IncompleteUtsendDescriptors[mp]->
                     begin(); desc != (UtsendDesc_t *)
                     IncompleteUtsendDescriptors[mp]->end();
                 desc = (UtsendDesc_t *) tmpDesc) {
                // save ptr to next descriptor before call to send()
                // to properly traverse IncompleteUtsendDescriptors only
                tmpDesc = (UtsendDesc_t *) desc->next;
                // send with lockIncompleteList set to false since
                // we already hold the lock to IncompleteUtsendDescriptors;
                // potential for deadlock with utsend_start lock order
                // so try to grab lock..if we can't, don't worry
                // just go on to the next descriptor
                if (desc->Lock.trylock() == 1) {
                    desc->send(false);
                    desc->Lock.unlock();
                }
            }
            IncompleteUtsendDescriptors[mp]->Lock.unlock();
        }
        if (UnackedUtsendDescriptors[mp]->size() > 0) {
            // free collective sends if completely acked
            UnackedUtsendDescriptors[mp]->Lock.lock();
            for (desc =
                     (UtsendDesc_t *) UnackedUtsendDescriptors[mp]->
                     begin();
                 desc !=
                     (UtsendDesc_t *) UnackedUtsendDescriptors[mp]->
                     end(); desc = (UtsendDesc_t *) desc->next) {
                if ((desc->unackedPt2PtMessages.size() == 0)
                    && (desc->incompletePt2PtMessages.size() == 0)) {
                    if (!desc->sendDone
                        && (desc->messageDoneCount ==
                            desc->numDescsToAllocate)) {
                        // the request is in process private memory, but we are in the right context...
                        desc->request->messageDone = true;
                        desc->sendDone = true;
                    }
                    tmpDesc = (UtsendDesc_t *)
                        UnackedUtsendDescriptors[mp]->
                        RemoveLinkNoLock(desc);
                    desc->freeDescriptor();
                    desc = tmpDesc;
                }
            }
            UnackedUtsendDescriptors[mp]->Lock.unlock();
        }
    }
    // check for completed sends
    double now = dclock();
    CheckForAckedMessages(now);

    if (USE_MULTICAST_COMMUNICATIONS) {
        // check for incomplete utrecv's
        for (i = 0; i < nCommunicatorInstancesInUse; i++) {
            returnValue =
                communicators[activeCommunicators[i]]->
                processCollectiveFrags();
            if ((returnValue == ULM_ERR_OUT_OF_RESOURCE)
                || (returnValue == ULM_ERR_FATAL)) {
                return returnValue;
            }
        }
    }
#ifdef SHARED_MEMORY
    // return - if only 1 host
    if (lampiState.nhosts == 1)
        return returnValue;
#endif                          // SHARED_MEMORY

    if (DEBUG_TIMES) {
        ulm_err(("before push_frags_into_network\n"));
        t[2] = dclock();
        pt[1] += (t[2] - t[1]);
    }

    // Check and see if there are any message traffic that needs to be
    // pushed along
    //
    // send messages if need be
    returnValue = push_frags_into_network(now);
    if ((returnValue == ULM_ERR_OUT_OF_RESOURCE) ||
        (returnValue == ULM_ERR_FATAL)) {
        return returnValue;
    }

    if (DEBUG_TIMES) {
        t[6] = dclock();
        pt[5] += (t[6] - t[2]);
    }

#ifdef RELIABILITY_ON

    // check for collective communication retransmits on both the
    // Incomplete and Unacked UtsendDescriptor lists...and send
    // anything on the Unacked list that needs to be retransmitted
    // using reSend() to put the descriptor back on the Incomplete
    // list

    if (0) {
        if ((now >= (reliabilityInfo->lastCheckForCollRetransmits +
                     MIN_RETRANS_TIME)) ||
            (reliabilityInfo->lastCheckForCollRetransmits < 0.0)) {
            reliabilityInfo->lastCheckForCollRetransmits = now;
            CheckForCollRetransmits(mp);
        }
    }
#endif


    if (DEBUG_TIMES) {
        t[3] = dclock();
        pt[2] += (t[3] - t[6]);
    }
#ifndef SHARED_MEMORY
    if (lampiState.nhosts > 1) {
#endif
        BasePath_t *pathArray[MAX_PATHS];
        int pathCount = pathContainer()->allPaths(pathArray, MAX_PATHS);
        for (i = 0; i < pathCount; i++) {
            if (!pathArray[i]->push(now, &returnValue)) {
                if (returnValue == ULM_ERR_OUT_OF_RESOURCE
                    || returnValue == ULM_ERR_FATAL)
                    return returnValue;
            }
            if (!pathArray[i]->receive(now, &returnValue)) {
                if (returnValue == ULM_ERR_OUT_OF_RESOURCE
                    || returnValue == ULM_ERR_FATAL)
                    return returnValue;
            }
        }
#ifndef SHARED_MEMORY
    }
#endif

    if (DEBUG_TIMES) {
        t[4] = dclock();
        pt[3] += (t[4] - t[3]);
    }
    //
    // send acks
    //
    SendUnsentAcks();

    if (DEBUG_TIMES) {
        t[5] = dclock();
        pt[4] += (t[5] - t[4]);
    }

    if (DEBUG_TIMES) {
        ulm_err((" leaving ulm_make_progress \n"));
    }

    return returnValue;
}
