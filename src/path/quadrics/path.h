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



#ifndef QUADRICS_PATH_H
#define QUADRICS_PATH_H

#include "ulm/ulm.h"
#include "queue/globals.h" /* for getMemPoolIndex() */
#include "util/dclock.h"
#include "path/common/path.h"
#include "path/quadrics/state.h"
#include "path/quadrics/recvFrag.h"
#include "path/quadrics/sendFrag.h"
#include "path/quadrics/header.h"
#include <stdio.h>

#ifdef RELIABILITY_ON
#include "internal/constants.h"
#endif

class quadricsPath : public BasePath_t {
public:
    quadricsPath() {
        pathType_m = QUADRICSPATH;
    }

    ~quadricsPath() {}

    bool canReach(int globalDestProcessID) {
        // all rails reach all processes currently
        int destinationHostID = global_proc_to_host(globalDestProcessID);
        // return true only for processes not on our "box"
        if (myhost() != destinationHostID)
            return true;
        else
            return false;
    }

    bool bind(BaseSendDesc_t *message, int *globalDestProcessIDArray,
              int destArrayCount, int *errorCode) {
        if (isActive()) {
            *errorCode = ULM_SUCCESS;
            message->path_m = this;
            return true;
        }
        else {
            *errorCode = ULM_ERR_BAD_PATH;
            return false;
        }
    }

    void unbind(BaseSendDesc_t *message, int *globalDestProcessIDArray,
                int destArrayCount) {
        ulm_exit((-1, "quadricsPath::unbind - called, not implemented yet...\n"));
    }

    bool init(BaseSendDesc_t *message) {
        int lbufType, npkts;

        // initialize quadricsSendInfo values
        message->pathInfo.quadrics.lastMemReqSentTime = -1.0;
        message->pathInfo.quadrics.lastMemReqOffset = (size_t)(-1);

        // initialize numfrags
        lbufType = (message->multicastMessage) ? SHARED_LARGE_BUFFERS : PRIVATE_LARGE_BUFFERS;
        npkts = (message->PostedLength + quadricsBufSizes[lbufType] - 1)/ quadricsBufSizes[lbufType];
        if (npkts == 0)
            npkts = 1;
        message->numfrags = npkts;

        return true;
    }

    bool send(BaseSendDesc_t *message, bool *incomplete, int *errorCode);

    bool sendDone(BaseSendDesc_t *message, double timeNow, int *errorCode) {

        if (!quadricsDoAck) {
            quadricsSendFragDesc *sfd, *afd;

            if ((message->sendType != ULM_SEND_SYNCHRONOUS) || (message->NumSent > 1)) {
                if (timeNow < 0)
                    timeNow = dclock();

                for (sfd = (quadricsSendFragDesc *)message->FragsToAck.begin();
                    sfd != (quadricsSendFragDesc *)message->FragsToAck.end();
                    sfd = (quadricsSendFragDesc *)sfd->next) {
                    if (sfd->done(timeNow, errorCode)) {
                        // register frag as ACK'ed
                        message->clearToSend_m = true;
                        (message->NumAcked)++;
                        // remove frag from FragsToAck list
                        afd = sfd;
                        sfd = (quadricsSendFragDesc *)message->FragsToAck.RemoveLinkNoLock((Links_t *)afd);
                        // return all sfd send resources
                        afd->freeRscs();
                        afd->WhichQueue = QUADRICSFRAGFREELIST;
                        // return frag descriptor to free list
                        // the header holds the global proc id
                        if (usethreads()) {
                            quadricsSendFragDescs.returnElement(afd, afd->rail);
                        } else {
                            quadricsSendFragDescs.returnElementNoLock(afd, afd->rail);
                        }
                    }
                }
            }

            if (message->NumAcked >= message->numfrags) {
                /* something like the following should exist for utsend/utrecv
                if (message->multicastMessage) {
                    UtsendDesc_t *msd = message->multicastMessage;
                    msd->Lock.lock();
                    // increment the multicast send descriptor sub-message completed count
                    (msd->messageDoneCount)++;
                    if (message->WhichQueue == INCOMPLETEISENDQUEUE) {
                        msd->incompletePt2PtMessages.RemoveLinkNoLock(message);
                    } else if (message->WhichQueue == UNACKEDISENDQUEUE) {
                        msd->unackedPt2PtMessages.RemoveLinkNoLock(message);
                    } else {
                        ulm_exit((-1, "quadricsPath::sendDone base"
                          "send desc. on %d queue.\n",
                          message->WhichQueue));
                    }
                    // return the descriptor to the sending process' pool
                    message->ReturnDesc(getMemPoolIndex());
                    msd->Lock.unlock();
                }
                */
                return true;
            }
            else {
                return false;
            }
        }
        
        else {
            if (message->NumAcked >= message->numfrags) {
                return true;
            }
            else {
                return false;
            }
        }
    }

    bool receive(double timeNow, int *errorCode, recvType recvTypeArg = ALL);

    bool push(double timeNow, int *errorCode);

    bool needsPush(void);

#ifdef RELIABILITY_ON
    bool retransmitP(BaseSendDesc_t *message) {
        if (!quadricsDoAck || (RETRANS_TIME == -1) || (message->earliestTimeToResend == -1)
            || (message->FragsToAck.size() == 0))
            return false;
        else if (dclock() >= message->earliestTimeToResend) {
            return true;
        } else {
            return false;
        }
    }

    bool resend(BaseSendDesc_t *message, int *errorCode);
#endif

    // lazy rail-balancer to even out the number of DMAs per rail
    bool getCtxAndRail(BaseSendDesc_t *message, int globalDestProcessID,
                       ELAN3_CTX **ctx, int *rail) {
        int i, r, lr = quadricsLastRail;

        for (i = 1; i <= quadricsNRails; i++) {
            r = (lr + i) % quadricsNRails;
            if (quadricsQueue[r].railOK) {
                *rail = r;
                *ctx = quadricsQueue[r].ctx;
                quadricsLastRail = r;
                return true;
            }
        }
        return false;
    }

    bool getCtxAndRail(ELAN3_CTX **ctx, int *rail) {
        int i, r, lr = quadricsLastRail;

        for (i = 1; i <= quadricsNRails; i++) {
            r = (lr + i) % quadricsNRails;
            if (quadricsQueue[r].railOK) {
                *rail = r;
                *ctx = quadricsQueue[r].ctx;
                quadricsLastRail = r;
                return true;
            }
        }
        return false;
    }

    bool getCtxRailAndDest(BaseSendDesc_t *message, int globalDestProcessID,
                           ELAN3_CTX **ctx, int *rail, void **dest, 
                           int destBufType, bool needDest,
                           int *errorCode) {
        int i, r, lr = quadricsLastRail;
        bool railAvail = false;
        int bufCounts[NUMBER_BUFTYPES];
        size_t sz;

        for (i = 1; i <= quadricsNRails; i++) {
            r = (lr + i) % quadricsNRails;
            if (quadricsQueue[r].railOK) {
                railAvail = true;
                if (message->sendType == ULM_SEND_MULTICAST) {
                    *dest = communicators[message->ctx_m]->getMcastBuf(r, &sz);
                    *rail = r;
                    *ctx = quadricsQueue[r].ctx;
                    quadricsLastRail = r;
                    return true;
                }
                else if (needDest) {
                    quadricsPeerMemory[r]->addrCounts(globalDestProcessID, bufCounts);
                    if (bufCounts[destBufType] == 0)
                        continue;
                    *dest = quadricsPeerMemory[r]->pop(globalDestProcessID, destBufType);
                    if (!*dest)
                        continue;
                    *rail = r;
                    *ctx = quadricsQueue[r].ctx;
                    quadricsLastRail = r;
                    return true;
                }
                else {
                    *rail = r;
                    *ctx = quadricsQueue[r].ctx;
                    quadricsLastRail = r;
                    return true;
                }
            }
        }
        *errorCode = (railAvail) ? ULM_SUCCESS : ULM_ERR_BAD_PATH;
        return false;
    }

    bool sendCtlMsgs(int rail, double timeNow, int startIndex, int endIndex,
                     int *errorCode, bool skipCheck = false);

    // all rails version
    bool sendCtlMsgs(double timeNow, int startIndex, int endIndex, int *errorCode);

    bool cleanCtlMsgs(int rail, double timeNow, int startIndex, int endIndex,
                      int *errorCode, bool skipCheck = false);

    // all rails version
    bool cleanCtlMsgs(double timeNow, int startIndex, int endIndex, int *errorCode);

    bool sendMemoryRequest(BaseSendDesc_t *message, int gldestProc, size_t offset,
                           size_t memNeeded, int *errorCode);

    bool releaseMemory(double timeNow, int *errorCode);
};

#endif
