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

#include "queue/globals.h"
#include "queue/globals.h"
#include "internal/log.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "ulm/ulm.h"


/*
 * This routine is used to set up a group of prcesses identified
 * by the id comm.
 */
extern "C" int ulm_communicator_alloc(int comm, int useThreads, int group1Index,
                                      int group2Index, int setCtxCache, int sizeCtxCache, int *lstCtxElements,
                                      int commType, int okToDeleteComm, int useSharedMem, int useSharedMemForColl)
{
    int returnCode = ULM_SUCCESS;
    Communicator **tmpPtr;

    //
    // Check to make sure group size is within the size of the
    // global group
    //

    if (OPT_CHECK_API_ARGS) {
        if (group1Index >= grpPool.poolSize || group1Index < 0) {
            ulm_err(("Error: group index out of range\n"));
            return ULM_ERR_BAD_PARAM;
        }
        if (group2Index >= grpPool.poolSize || group2Index < 0) {
            ulm_err(("Error: group index out of range\n"));
            return ULM_ERR_BAD_PARAM;
        }
        if (grpPool.groups[group1Index] == 0) {
            ulm_err(("Error: null group\n"));
            return ULM_ERR_BAD_PARAM;
        }
        if (grpPool.groups[group2Index] == 0) {
            ulm_err(("null group\n"));
            return ULM_ERR_BAD_PARAM;
        }
    }
    //
    // find communicator marked for deletion, and see if the can
    //   be deleted. - lazy free
    //
    if (usethreads())
        communicatorsLock.lock();
    /* we really only want one thread at a time trying to free communicators */
    for (int cm = 0; cm < communicatorsArrayLen; cm++) {
        // check to see if marked for deletion
        if (communicators[cm]
            && (communicators[cm]->markedForDeletion == 1)) {

            returnCode = ulm_communicator_really_free(cm);
            if (returnCode != ULM_SUCCESS) {
                if (usethreads())
                    communicatorsLock.unlock();
                return returnCode;
            }
        }
    }                           // end cm loop

    /* lock to make sure only one thread at a time will modify the communicators
     *  and activeCommunicators array
     */

    // grow communictor array if array is not large enough to hold
    //   pointer to new group
    //
    if (communicatorsArrayLen <= comm) {
        int oldLen = communicatorsArrayLen;
        communicatorsArrayLen = comm + nIncreaseCommunicatorsArray + 1;

        // grow array
        /* if threaded, lock to make sure only one thread at a time changes
         *   this array.  Also, make sure that if the pointer is changed,
         *   and this is a threaded job, the job aborts - want to make sure
         *   that if other threads reference communictor, they do not end
         *   up holding on to stale pointer, and don't want to take the
         *   hit of making the array a volatile array */
        /* check to see if this has already been reallocated */
        tmpPtr = communicators;
        communicators = (Communicator **) realloc((void *) communicators,
                                                  communicatorsArrayLen *
                                                  sizeof(Communicator *));
        if (!communicators) {
            ulm_err(("Error: Out of memory\n"));
            if (usethreads())
                communicatorsLock.unlock();
            return ULM_ERR_OUT_OF_RESOURCE;
        }

        if ((tmpPtr != communicators) && usethreads()) {
            ulm_err(("Error: ulm_communicator_alloc: "
                     " The communicator array moved with the realloc call - stale pointers possible\n"));
            if (usethreads())
                communicatorsLock.unlock();
            return ULM_ERROR;
        }
        // initialize new  elements
        for (int grp = oldLen; grp < communicatorsArrayLen; grp++)
            communicators[grp] = 0L;

        if (usethreads())
            communicatorsLock.unlock();
    }
    // make sure that this context id is not locally in use
    if ((comm > communicatorsArrayLen)
        && (communicators[comm] != 0L)) {
        ulm_err(("Error: communicator already in use (%d)\n",
                 comm));
        if (usethreads())
            communicatorsLock.unlock();
        return ULM_ERR_COMM;
    }

    /* get element from Queue pool */
    bool firstInstanceOfContext = true;

    //
    // set communicators array
    //
    // allocate communicator object
    communicators[comm] = ulm_new(Communicator, 1);
    if (!communicators[comm]) {
        ulm_err(("Error: Out of memory\n"));
        if (usethreads())
            communicatorsLock.unlock();
        return ULM_ERR_OUT_OF_RESOURCE;
    }
    // increment the reference counter
    communicators[comm]->refCount = 1;

    /* need to check and make sure that activeCommunicators can hold
     *   another entry */


    activeCommunicators[nCommunicatorInstancesInUse] = comm;
    nCommunicatorInstancesInUse++;

    //
    // call group init function to set up the group object
    //
    bool threadUsage = (bool) usethreads();

    // initialize the Communicator
    returnCode =
        communicators[comm]->init(comm, threadUsage, group1Index,
                                  group2Index, firstInstanceOfContext,
                                  setCtxCache, sizeCtxCache,
                                  lstCtxElements, commType, okToDeleteComm,
                                  useSharedMem, useSharedMemForColl);

    if (usethreads())
        communicatorsLock.unlock();

    return returnCode;
}
