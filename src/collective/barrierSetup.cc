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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "client/ULMClient.h"
#include "collective/barrier.h"
#include "internal/constants.h"
#include "internal/types.h"
#include "queue/Communicator.h"
#include "queue/barrier.h"
#include "queue/globals.h"

/*
 *  barrier initialization routines.  These include a call to set up
 *  platform specific "stuff", as well as generic setup code.
 *
 *  The firstInstanceOfContext is used for allocation of process
 *  shared data stuctures.
 */
int Communicator::barrierInit(bool firstInstanceOfContext)
{
    int returnCode = ULM_SUCCESS;

    int callGenericSMPSetup = 1;
    int callGenericNetworkSetup, gotSMPresources, gotNetworkResources;

    /* special case for process private "shared queues" */
    if (!useSharedMemForCollectives) {
        goto SWBarrierTag;
    }

    /* plaform specific setup code */
    returnCode = platformBarrierSetup(&callGenericSMPSetup,
                                      &callGenericNetworkSetup,
                                      &gotSMPresources,
                                      &gotNetworkResources);
    if (returnCode != ULM_SUCCESS) {
        ulm_err(("Error: error returned from platformBarrierSetup\n"));
        return returnCode;
    }

    /*
     * generic code
     */
SWBarrierTag:
    /* SMP code */
    if (callGenericSMPSetup) {
        returnCode =
            genericSMPBarrierSetup(firstInstanceOfContext,
                                   &gotSMPresources);
        if (returnCode != ULM_SUCCESS) {
            ulm_err(("Error: error returned from genericSMPBarrierSetup\n"));
            return returnCode;
        }
    }

    return returnCode;
}

/*
 * code for setting up resources for a shared memory implementation of
 * the barrier
 */
int Communicator::genericSMPBarrierSetup(int firstInstanceOfContext,
                                         int *gotSMPResources)
{
    int returnCode = ULM_ERROR;
    bool foundFreeElement = false;
    *gotSMPResources = 1;

    /* lock pool */
    swBarrier.Lock->lock();

    /* if firstInstanceOfContext, then scan pool to make sure first */
    if (firstInstanceOfContext) {
        for (int pl = 0; pl < swBarrier.nPools; pl++) {
            for (int ele = 0; ele < swBarrier.nElementsInPool[pl]; ele++) {
                if (swBarrier.pool[pl][ele].inUse &&
                    (swBarrier.pool[pl][ele].contextID == contextID) &&
                    (swBarrier.pool[pl][ele].commRoot ==
                     localGroup->mapGroupProcIDToGlobalProcID[0])) {
                    firstInstanceOfContext = 0;
                    break;
                }
                if (!firstInstanceOfContext)
                    break;
            }
        }
    }

    /* if first instance - find shared resource in pool */
    if (firstInstanceOfContext) {

        // if no HW resources available setup SW barrier
        int poolIndex = *(swBarrier.lastPoolUsed);

        int poolCnt = 0;
        while ((!foundFreeElement) && (poolCnt < swBarrier.nPools)) {

            poolIndex++;
            // wrap around
            if (poolIndex == swBarrier.nPools)
                poolIndex = 0;

            // check pool for free elements
            for (int ele = 0; ele < swBarrier.nElementsInPool[poolIndex];
                 ele++) {

                // if element available - grab it
                if (!swBarrier.pool[poolIndex][ele].inUse) {
                    foundFreeElement = true;
                    // set element as in use and list group infomation
                    swBarrier.pool[poolIndex][ele].commRoot =
                        localGroup->mapGroupProcIDToGlobalProcID[0];
                    swBarrier.pool[poolIndex][ele].contextID = contextID;
                    swBarrier.pool[poolIndex][ele].nAllocated = 1;
                    swBarrier.pool[poolIndex][ele].nFreed = 0;
                    swBarrier.pool[poolIndex][ele].inUse = true;
                    // initialize barrier data
                    *(swBarrier.pool[poolIndex][ele].barrierData->
                      Counter) = 0;
                    swBarrier.pool[poolIndex][ele].barrierData->commSize =
                        localGroup->groupHostData[localGroup->
                                                  hostIndexInGroup].
                        nGroupProcIDOnHost;
                    swBarrier.pool[poolIndex][ele].barrierData->
                        releaseCnt = 0;
                    swBarrier.pool[poolIndex][ele].barrierData->lock->
                        init();
                    // set barrier type
                    SMPBarrierType = SMPSWBARRIER;
                    // set pointer to barrier data structure
                    barrierData =
                        (void *) swBarrier.pool[poolIndex][ele].
                        barrierData;
                    // set pointer to barrier control structure
                    barrierControl =
                        (void *) (&(swBarrier.pool[poolIndex][ele]));
                    // set pointer to barrier function
                    smpBarrier = SMPSWBarrier;
                    // set the pool index this is taken out of
                    *(swBarrier.lastPoolUsed) = poolIndex;
                    // set return code
                    returnCode = ULM_SUCCESS;

                    break;
                }
            }

            poolCnt++;

        }                       // end while loop

        /* if could not find a free element - return error */
        if (!foundFreeElement) {
            *gotSMPResources = 0;
            /* unlock pool */
            swBarrier.Lock->unlock();
            return ULM_ERR_OUT_OF_RESOURCE;
        }

    }

    /* end if (firstInstanceOfContext) */

    /*
     * resource already allocated for this communicator, just find it,
     * and fill in Communicator details
     */
    if (!firstInstanceOfContext) {

        bool foundElement = false;

        for (int poolIndex = 0; poolIndex < swBarrier.nPools; poolIndex++) {

            // check pool for free elements
            for (int ele = 0; ele < swBarrier.nElementsInPool[poolIndex];
                 ele++) {

                // if element available - grab it
                if (swBarrier.pool[poolIndex][ele].inUse &&
                    (swBarrier.pool[poolIndex][ele].contextID == contextID)
                    && (swBarrier.pool[poolIndex][ele].commRoot ==
                        localGroup->mapGroupProcIDToGlobalProcID[0])
                    ) {
                    foundElement = true;
                    // set element as in use and list group infomation
                    swBarrier.pool[poolIndex][ele].nAllocated++;
                    // initialize barrier data
                    swBarrier.pool[poolIndex][ele].barrierData->commSize =
                        localGroup->groupHostData[localGroup->
                                                  hostIndexInGroup].
                        nGroupProcIDOnHost;
                    swBarrier.pool[poolIndex][ele].barrierData->
                        releaseCnt = 0;
                    swBarrier.pool[poolIndex][ele].barrierData->lock->
                        init();
                    // set barrier type
                    SMPBarrierType = SMPSWBARRIER;
                    // set pointer to barrier data structure
                    barrierData =
                        (void *) swBarrier.pool[poolIndex][ele].
                        barrierData;
                    // set pointer to barrier control structure
                    barrierControl =
                        (void *) (&(swBarrier.pool[poolIndex][ele]));
                    // set pointer to barrier function
                    smpBarrier = SMPSWBarrier;
                    // set return code
                    returnCode = ULM_SUCCESS;

                    break;
                }
            }
            if (foundElement) {
                break;
            }

        }                       // end poolIndex loop

        /* could not find allocated element */
        if (!foundElement) {
            *gotSMPResources = 0;
            /* unlock pool */
            swBarrier.Lock->unlock();
            return ULM_ERR_BAD_PARAM;
        }

    }

    /* end if (!firstInstanceOfContext) */

    /* unlock pool */
    swBarrier.Lock->unlock();
    return returnCode;
}

int Communicator::barrierFree()
{
    int returnValue = ULM_SUCCESS;

    // SMP barrier
    // determine barrier type
    int barrierType = SMPBarrierType;

    swBarrierCtlData *swbarCtl;

    switch (barrierType) {

    case HWSPECIFICBARRIER:
        freePlatformSpecificBarrier();
        break;

    case SMPSWBARRIER:
        /* lock pool - also ensures I/O ordering */
        swBarrier.Lock->lock();
        // increment nFreed
        swbarCtl = (swBarrierCtlData *) barrierControl;
        swbarCtl->nFreed++;

        // if all local process have freed the group - recyle the barrier object
        if (swbarCtl->nFreed == swbarCtl->barrierData->commSize) {
            swbarCtl->inUse = false;
        }
        // reset resouce pointers
        barrierControl = NULL;
        barrierData = NULL;
        smpBarrier = NULL;
        /* unlock pool - also ensures I/O ordering */
        swBarrier.Lock->unlock();

        break;

    default:
        ulm_err(("Error: Unrecognized SMP barrier type (%d)\n", barrierType));
        return ULM_ERR_BAD_PARAM;
        break;
    }

    return returnValue;
}
