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
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "internal/types.h"
#include "path/common/path.h"
#include "path/common/pathContainer.h"
#include "ulm/ulm.h"

#ifdef ENABLE_UDP
# include "path/udp/UDPNetwork.h"
#endif // UDP

#ifdef ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPfns.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"

bool _ulm_checkSMPInMakeProgress = true;
#endif // SHARED_MEMORY


#ifdef ENABLE_RELIABILITY
#include "util/DblLinkList.h"
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
				        /* send requests only */
				        if (usethreads()) {
					        ulmRequestDescPool.returnElement(req);
				        } else {
		   			        ulmRequestDescPool.returnElementNoLock(req);
				        }
                    }
                }
                if (usethreads())
                    list->Lock.unlock();
        }
    }

    // check for completed sends
    double now = dclock();
    CheckForAckedMessages(now);

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

    if (DEBUG_TIMES) {
        t[3] = dclock();
        pt[2] += (t[3] - t[6]);
    }

    BasePath_t *pathArray[MAX_PATHS];
    int pathCount = pathContainer()->allPaths(pathArray, MAX_PATHS);
    for (i = 0; i < pathCount; i++) {
        if (pathArray[i]->needsPush()) {
            if (!pathArray[i]->push(now, &returnValue)) {
                if (returnValue == ULM_ERR_OUT_OF_RESOURCE
                    || returnValue == ULM_ERR_FATAL)
                    return returnValue;
            }
        }
        if (!pathArray[i]->receive(now, &returnValue)) {
            if (returnValue == ULM_ERR_OUT_OF_RESOURCE
                || returnValue == ULM_ERR_FATAL)
                return returnValue;
        }
    }

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
        ulm_err(("Error: leaving ulm_make_progress\n"));
    }

    return returnValue;
}
