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

#include <sys/pmo.h>
#include <fetchop.h>

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "queue/barrier.h"
#include "queue/globals.h"
#include "client/ULMClient.h"
#include "os/IRIX/barrierFunctions.h"
#include "os/IRIX/fetchAndOp.h"
#include "internal/constants.h"
#include "internal/types.h"
#include "os/IRIX/acquire.h"

/* debug */
#include <pthread.h>
extern pthread_key_t ptPrivate;
#define dbgFile (FILE *)(pthread_getspecific(ptPrivate))
/* end debug */

//
//  This function is used to define Origin specific barrier functions.
//

//
// This routine uses uncached atomic access HW residing on the hubs for
// the barrier counter
//
void fetchAndAddSMPHWBarrier(volatile void *barrierData)
{
    // cast data to real type
    barrierFetchOpData *barData = (barrierFetchOpData *) barrierData;
    int count = 0;

    // increment "release" count
    barData->releaseCnt += barData->commSize;

    // increment fetch and op variable
    // spin until fetch and op variable == release count
    barData->currentCounter =
        FETCHOP_LOAD_OP(barData->fetchOpVar, FETCHOP_INCREMENT);
    while (barData->currentCounter < barData->releaseCnt) {
        barData->currentCounter =
            FETCHOP_LOAD_OP(barData->fetchOpVar, FETCHOP_LOAD);
        count++;
        if (count == 10000) {
            ulm_make_progress();
            count = 0;
        }
    }
}


/*
 *  This routine is used to allocate the resources needed for the
 *  atomic HW on the SN0.  This resource is for use for an on-host
 *  shared memory barrier implementation
 */
int Communicator::platformBarrierSetup(int *callGenericSMPSetup,
                                       int *callGenericNetworkSetup,
                                       int *gotSMPresouces,
                                       int *gotNetworkResources)
{
    int returnCode = ULM_SUCCESS;
    *gotSMPresouces = 0;
    *gotNetworkResources = 0;
    *callGenericSMPSetup = 1;
    *callGenericNetworkSetup = 1;

    if (ULMai->hwBarrier.nPools == 0) {
        return returnCode;
    }

    /* lock pool */
    ULMai->hwBarrier.Lock->lock();

    /* check to see if someone else has already allocated this */
    int firstInstanceOfContext = 1;
    for (int pl = 0; pl < ULMai->hwBarrier.nPools; pl++) {
        for (int ele = 0; ele < ULMai->hwBarrier.nElementsInPool[pl];
             ele++) {
            if (ULMai->hwBarrier.pool[pl][ele].inUse
                && (ULMai->hwBarrier.pool[pl][ele].contextID == contextID)
                && (ULMai->hwBarrier.pool[pl][ele].commRoot ==
                    localGroup->mapGroupProcIDToGlobalProcID[0])) {
                firstInstanceOfContext = 0;
                break;
            }
            if (!firstInstanceOfContext)
                break;
        }
    }

    /*
     *  first instance, resources not yet allocated for this communicator
     */
    if (firstInstanceOfContext) {

        bool foundFreeElement = false;

        // if no hw pool available, skip directly to sw barriers
        if (ULMai->hwBarrier.nPools > 0) {
            // see if can get fetch and op slot
            int poolIndex = *(ULMai->hwBarrier.lastPoolUsed);

            int poolCnt = 0;
            while ((!foundFreeElement)
                   && (poolCnt < ULMai->hwBarrier.nPools)) {

                poolIndex++;
                // wrap around
                if (poolIndex == ULMai->hwBarrier.nPools)
                    poolIndex = 0;

                // check pool for free elements
                for (int ele = 0;
                     ele < ULMai->hwBarrier.nElementsInPool[poolIndex];
                     ele++) {

                    // if element available - grab it
                    if (!ULMai->hwBarrier.pool[poolIndex][ele].inUse) {
                        foundFreeElement = true;
                        // set element as in use and list group infomation
                        ULMai->hwBarrier.pool[poolIndex][ele].commRoot =
                            localGroup->mapGroupProcIDToGlobalProcID[0];
                        ULMai->hwBarrier.pool[poolIndex][ele].contextID =
                            contextID;
                        ULMai->hwBarrier.pool[poolIndex][ele].nAllocated =
                            1;
                        ULMai->hwBarrier.pool[poolIndex][ele].nFreed = 0;
                        ULMai->hwBarrier.pool[poolIndex][ele].inUse = true;
                        // initialize barrier data
                        FETCHOP_STORE_OP(ULMai->hwBarrier.
                                         pool[poolIndex][ele].barrierData->
                                         fetchOpVar, FETCHOP_STORE, 0);
                        ULMai->hwBarrier.pool[poolIndex][ele].barrierData->
                            hubIndex = poolIndex;
                        ULMai->hwBarrier.pool[poolIndex][ele].barrierData->
                            commSize =
                            localGroup->groupHostData[localGroup->
                                                      hostIndexInGroup].
                            nGroupProcIDOnHost;
                        ULMai->hwBarrier.pool[poolIndex][ele].barrierData->
                            releaseCnt = 0;
                        // set barrier type
                        SMPBarrierType = HWSPECIFICBARRIER;
                        // set pointer to barrier data structure
                        barrierData =
                            (void *) ULMai->hwBarrier.pool[poolIndex][ele].
                            barrierData;
                        // set pointer to barrier control structure
                        barrierControl =
                            (void
                             *) (&(ULMai->hwBarrier.pool[poolIndex][ele]));
                        // set pointer to barrier function
                        smpBarrier = fetchAndAddSMPHWBarrier;
                        // set the pool index this is taken out of
                        *(ULMai->hwBarrier.lastPoolUsed) = poolIndex;
                        // set return code
                        returnCode = ULM_SUCCESS;

                        break;
                    }
                }

                poolCnt++;

            }                   // end while loop
        }                       // end ( ULMai->hwBarrier.nPools > 0 )

        /* resource found */
        if (foundFreeElement) {
            *gotSMPresouces = 1;
            *callGenericSMPSetup = 0;
        }

    }

    /* end if( firstInstanceOfContext ) */
    /*
     *  resource already found for this communicator, just find it, and fill in data
     */
    if (!firstInstanceOfContext) {
        // find resource in use
        bool foundElement = false;

        // find resource
        for (int poolIndex = 0; poolIndex < ULMai->hwBarrier.nPools;
             poolIndex++) {

            // check pool for free elements
            for (int ele = 0;
                 ele < ULMai->hwBarrier.nElementsInPool[poolIndex];
                 ele++) {

                // if element available - grab it
                if (ULMai->hwBarrier.pool[poolIndex][ele].inUse &&
                    (ULMai->hwBarrier.pool[poolIndex][ele].contextID ==
                     contextID)
                    && (ULMai->hwBarrier.pool[poolIndex][ele].commRoot ==
                        localGroup->mapGroupProcIDToGlobalProcID[0])
                    ) {
                    foundElement = true;
                    // set element as in use and list group infomation
                    ULMai->hwBarrier.pool[poolIndex][ele].nAllocated++;
                    // initialize barrier data
                    ULMai->hwBarrier.pool[poolIndex][ele].barrierData->
                        hubIndex = poolIndex;
                    ULMai->hwBarrier.pool[poolIndex][ele].barrierData->
                        commSize =
                        localGroup->groupHostData[localGroup->
                                                  hostIndexInGroup].
                        nGroupProcIDOnHost;
                    ULMai->hwBarrier.pool[poolIndex][ele].barrierData->
                        releaseCnt = 0;
                    // set barrier type
                    SMPBarrierType = HWSPECIFICBARRIER;
                    // set pointer to barrier data structure
                    barrierData =
                        (void *) ULMai->hwBarrier.pool[poolIndex][ele].
                        barrierData;
                    // set pointer to barrier control structure
                    barrierControl =
                        (void
                         *) (&(ULMai->hwBarrier.pool[poolIndex][ele]));
                    // set pointer to barrier function
                    smpBarrier = fetchAndAddSMPHWBarrier;
                    // set return code
                    returnCode = ULM_SUCCESS;

                    break;
                }
            }
            if (foundElement) {
                break;
            }

        }                       // end poolIndex loop

        /* resource found, if not found, will look in for "generic" resources */
        if (foundElement) {
            *gotSMPresouces = 1;
            *callGenericSMPSetup = 0;
        }

    }

    /* end if( !firstInstanceOfContext ) */
    /* unlock pool */
    ULMai->hwBarrier.Lock->unlock();

    return returnCode;
}

/*
 * free special atomic HW resources
 */
void Communicator::freePlatformSpecificBarrier()
{
    // increment nFreed
    barrierFetchOpDataCtlData *barCtl =
        (barrierFetchOpDataCtlData *) barrierControl;
    barCtl->nFreed++;

    // if all local process have freed the group - recyle the barrier object
    if (barCtl->nFreed == barCtl->barrierData->commSize) {
        barCtl->inUse = false;
    }
    // reset resouce pointers
    barrierControl = NULL;
    barrierData = NULL;
    smpBarrier = NULL;
}
