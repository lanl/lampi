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
#include "internal/options.h"

#ifdef ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPSharedMemGlobals.h"
#endif

//!
//! check to see if queues are empty
//!
bool Communicator::areQueuesEmpty()
{
    // check to see if there is pending traffic

    bool empty;
    int i, gotLock;
    DoubleLinkList *list;

    // check this communicator's request reference count and its queues...
    empty = ((requestRefCount + privateQueues.PostedWildRecv.size()) == 0);
    for (i = 0; i < remoteGroup->groupSize; i++) {
        empty = empty && (privateQueues.PostedSpecificRecv[i]->size() == 0);
        empty = empty && (privateQueues.MatchedRecv[i]->size() == 0);
        empty = empty && (privateQueues.AheadOfSeqRecvFrags[i]->size() == 0);
        empty = empty && (privateQueues.OkToMatchRecvFrags[i]->size() == 0);
#ifdef ENABLE_SHARED_MEMORY
        empty = empty && (privateQueues.OkToMatchSMPFrags[i]->size() == 0);
#endif				// SHARED_MEMORY
        if (!empty)
            break;
    }
    if (!empty) {
        return empty;
    }

    // check global network path send queues for objects belonging to this communicator
    list = &UnackedPostedSends;
    if (list->size() > 0) {
        gotLock = (usethreads()) ? list->Lock.trylock() : 1;
        if (gotLock) {
            for (BaseSendDesc_t *j = (BaseSendDesc_t *)list->begin();
                 j != (BaseSendDesc_t *)list->end(); j = (BaseSendDesc_t *)j->next) {
                if (j->ctx_m == contextID) {
                    empty = false;
                    break;
                }
            }
            if (usethreads()) {
                list->Lock.unlock();
            }
        }
        else {
            empty = false;
        }
    }
    if (!empty) {
        return empty;
    }


    list = &IncompletePostedSends;
    if (list->size() > 0) {
        gotLock = (usethreads()) ? list->Lock.trylock() : 1;
        if (gotLock) {
            for (BaseSendDesc_t *j = (BaseSendDesc_t *)list->begin();
                 j != (BaseSendDesc_t *)list->end(); j = (BaseSendDesc_t *)j->next) {
                if (j->ctx_m == contextID) {
                    empty = false;
                    break;
                }
            }
            if (usethreads()) {
                list->Lock.unlock();
            }
        }
        else {
            empty = false;
        }
    }
    if (!empty) {
        return empty;
    }

    // check global unprocessed acknowledgements for objects belong to this communicator
    list = &UnprocessedAcks;
    if (list->size() > 0) {
        gotLock = (usethreads()) ? list->Lock.trylock() : 1;
        if (gotLock) {
            for (BaseRecvFragDesc_t *j = (BaseRecvFragDesc_t *)list->begin();
                 j != (BaseRecvFragDesc_t *)list->end(); j = (BaseRecvFragDesc_t *)j->next) {
                if (j->ctx_m == contextID) {
                    empty = false;
                    break;
                }
            }
            if (usethreads()) {
                list->Lock.unlock();
            }
        }
        else {
            empty = false;
        }
    }
    if (!empty) {
        return empty;
    }

    return empty;
}
