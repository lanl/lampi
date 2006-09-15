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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "client/daemon.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "path/common/path.h"
#include "os/atomic.h"
#include "ulm/ulm.h"

#if ENABLE_RELIABILITY
#include "internal/constants.h"
#endif

int push_frags_into_network(double timeNow)
{
    int returnValue = ULM_SUCCESS;

#if ENABLE_RELIABILITY
    // check for retransmits
    if ((timeNow >= (lastCheckForRetransmits + MIN_RETRANS_TIME))
        || (lastCheckForRetransmits < 0.0)) {
        lastCheckForRetransmits = timeNow;
        returnValue = CheckForRetransmits();
        if (returnValue != ULM_SUCCESS) {
            return returnValue;
        }
    }
#endif

    //
    // Loop over the sends that have not yet been copied into pinned memory.
    //

    // nextDesc/preDesc used to make sure that the list remains consistent
    //  from one iteration to the next, thus enabling multiple threads to
    //  modify it.

    // lock list to make sure reads are atomic
    if (usethreads())
        IncompletePostedSends.Lock.lock();

    for (SendDesc_t * SendDesc = (SendDesc_t *)
         IncompletePostedSends.begin();
         SendDesc != (SendDesc_t *) IncompletePostedSends.end();
         SendDesc = (SendDesc_t *) SendDesc->next) {
        // lock descriptor - just incase bad data was transmitted, so that this frag can't
        //   be processed by the bad ack code while this is going on
        //
        // set nextDesc/prevDesc

        // process message only if lock can be obtained
        int LockReturn = 1;
        if (usethreads())
            LockReturn = SendDesc->Lock.trylock();

        if (LockReturn == 1) {

            assert(SendDesc->WhichQueue == INCOMPLETEISENDQUEUE);

            //
            // Push a send.
            //
            int sendReturn;
            bool incomplete;
            while (!SendDesc->path_m->
                   send(SendDesc, &incomplete, &sendReturn)) {
                if (sendReturn == ULM_ERR_BAD_PATH) {
                    // unbind from the current path
                    if (0) {    // ++++++++++++ REVISIT ++++++++++++
                        SendDesc->path_m->unbind(SendDesc, (int *) 0, 0);
                        // select a new path, if at all possible
                        Communicator *commPtr =
                            communicators[SendDesc->ctx_m];
                        sendReturn =
                            (*(commPtr->pt2ptPathSelectionFunction)) ((void
                                                                       **)
                                                                      SendDesc,
                                                                      0,
                                                                      0);
                        if (sendReturn != ULM_SUCCESS) {
                            if (usethreads()) {
                                SendDesc->Lock.unlock();
                                IncompletePostedSends.Lock.unlock();
                            }
                            ulm_exit(("Error: push_frags_into_network: "
                                      "cannot find path for message\n"));
                        }
                        // initialize the descriptor for this path
                        SendDesc->path_m->init(SendDesc);
                    }           // ----------- REVISIT -------------------
                } else {
                    // unbind should empty SendDesc of frag descriptors, etc...
                    SendDesc->path_m->unbind(SendDesc, (int *) 0, 0);
                    if (usethreads()) {
                        SendDesc->Lock.unlock();
                        IncompletePostedSends.Lock.unlock();
                    }
                    ulm_exit(("Error: push_frags_into_network: fatal "
                              "error in sending message\n"));
                }
            }

            //
            // Check and see if we finished this send.
            //
            if (!incomplete) {
                //
                // We've finished sending
                //
                unsigned int nAcked = SendDesc->NumAcked;
                if( SendDesc->path_m->pathType_m == SHAREDMEM ) {
                    /* 
                     * the shared memory send descriptor stores
                     * NumAcked in the shared memory variable
                     * SendDesc->pathInfo.sharedmem.sharedData->NumAcked
                     */
                    nAcked = SendDesc->pathInfo.sharedmem.sharedData->NumAcked;
                }
                if ( ( nAcked >= SendDesc->numfrags) &&
                	 (SendDesc->NumSent >= (int)SendDesc->numfrags) ) {
                    // message has been acked
                    SendDesc_t *TmpDesc = (SendDesc_t *)
                        IncompletePostedSends.RemoveLinkNoLock(SendDesc);
                    SendDesc->WhichQueue = ONNOLIST;

                    if (!SendDesc->messageDone) {
                        SendDesc->messageDone = REQUEST_COMPLETE;
                        if (!SendDesc->persistent) {
                            ulm_type_release(SendDesc->datatype);
                            ulm_type_release(SendDesc->bsendDatatype);
                        }
                    }

                    if ((SendDesc->freeCalled)
                        || (SendDesc->persistFreeCalled)) {
                        /* a call to free the mpi object has been made,
                        *   so ok to free this descriptor */
                        SendDesc->WhichQueue = ONNOLIST;
                        if ( SendDesc->persistFreeCalled ) {
                            ulm_type_release(SendDesc->datatype);
                            ulm_type_release(SendDesc->bsendDatatype);
                        }
                        if (usethreads())
                            SendDesc->Lock.unlock();
                        if (SendDesc->freeCalled)
                            SendDesc->path_m->ReturnDesc(SendDesc);
                    } else {
                        if (usethreads())
                            SendDesc->Lock.unlock();
                    }
                    SendDesc = TmpDesc;
                } else {
                    // message has not been acked - move to unacked_isends list
                    SendDesc_t *TmpDesc = (SendDesc_t *)
                        IncompletePostedSends.RemoveLinkNoLock(SendDesc);
                    if (usethreads())
                        SendDesc->Lock.unlock();
                    // reset WhichQueue flag
                    SendDesc->WhichQueue = UNACKEDISENDQUEUE;
                    UnackedPostedSends.Append(SendDesc);
                    SendDesc = TmpDesc;
                }
            } else {
                // release lock
                if (usethreads())
                    SendDesc->Lock.unlock();
            }
        }                       // end trylock() block

    }                           // end loop over incomplete isends

    // unlock list
    if (usethreads())
        IncompletePostedSends.Lock.unlock();

    return returnValue;
}
