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

#include <stdio.h>

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "client/ULMClient.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "internal/types.h"
#include "os/atomic.h"
#include "ulm/ulm.h"

//
// start posting receive
//
int Communicator::irecv_start(ULMRequest_t *request)
{
    RecvDesc_t *RecvDesc = (RecvDesc_t *) (*request);
    unsigned long long seq;
    int srcProc = 0;

    // make sure that he request object is in the inactive state
    if (RecvDesc->status == ULM_STATUS_INVALID) {
        ulm_err(("Error: irecv_start: invalid status (%d)\n",
                 RecvDesc->status));
        return ULM_ERR_REQUEST;
    }

    // increment reference count on datatype for non-persistent requests

    if (!RecvDesc->persistent) {
        ulm_type_retain(RecvDesc->datatype);
    }

    // set message completion to incomplete
    RecvDesc->messageDone = REQUEST_INCOMPLETE;

    if (usethreads()) {
        // Generate a new sequence number for this irecv.
        seq = fetchNaddLong((bigAtomicUnsignedInt *) &next_irecv_id_counter,
                            1);
    } else {
        // Generate a new sequence number for this irecv.
        seq = fetchNaddLongNoLock(&next_irecv_id_counter, 1);
    }
    RecvDesc->WhichQueue = ONNOLIST;

    // fill in descriptor
    RecvDesc->reslts_m.length_m = 0;
    RecvDesc->DataReceived = 0;
    RecvDesc->DataInBitBucket = 0;
    RecvDesc->ctx_m = contextID;
    RecvDesc->irecvSeq_m = seq;
    RecvDesc->status = ULM_STATUS_INCOMPLETE;

    // There are different algorithms if we were told a specific proc to
    // receive from or a wild processor to receive from.

    if (RecvDesc->posted_m.peer_m == ULM_ANY_PROC) {
        // lock to make sure that frag list and post receive lists
        // remain consistent, and frags don't get lost.  (*** Need to
        // see if there is a better way to do this for the
        // ULM_ANY_PROC case ***)
        if (usethreads()) {
            for (int i = 0; i < remoteGroup->groupSize; i++) {
                recvLock[i].lock();
            }
        }
        // check list for match - posted receive is put on a list at this stage
        checkFragListsForWildMatch((RecvDesc_t *) RecvDesc);

        // "free" lists
        if (usethreads()) {
            for (int i = 0; i < remoteGroup->groupSize; i++) {
                recvLock[i].unlock();
            }
        }

    } else {

        // lock for thread safety
        if (usethreads()) {
            srcProc = RecvDesc->posted_m.peer_m;
            recvLock[srcProc].lock();
        }

        //  check list for match - posted receive is put on a list at
        //  this stage
        checkFragListsForSpecificMatch((RecvDesc_t *) RecvDesc);

        // "free" lists
        if (usethreads()) {
            recvLock[srcProc].unlock();
        }

    }

    return ULM_SUCCESS;
}
