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

#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "ulm/ulm.h"
#include "mpi/mpi.h"
#include "queue/globals.h"

/*!
 * create inter communicator
 */
extern "C" int ulm_intercomm_merge(int interComm, int high,
				   int *newIntraComm)
{
    int returnValue = MPI_SUCCESS;
    int *allLocalHighs = 0;
    int *newGroup = 0;
    int tag, localGroupSize, compareValue, sizeOfNewGroup;
    bool allSame;

    if (OPT_CHECK_API_ARGS) {

        if ((interComm > communicatorsArrayLen) || (interComm < 0)) {
            ulm_err(("Error: ulm_comm_create: bad communicator %d\n",
                     interComm));
            returnValue = MPI_ERR_COMM;
            goto BarrierTag;
        }

        if (communicators[interComm] == 0L) {
            ulm_err(("Error: ulm_comm_create: bad communicator %d\n",
                     interComm));
            returnValue = MPI_ERR_COMM;
            goto BarrierTag;
        }
        // verify that this is an inter-communictor
        if (communicators[interComm]->communicatorType !=
            Communicator::INTER_COMMUNICATOR) {
            returnValue = MPI_ERR_ARG;
            goto BarrierTag;
        }

    }

    // process 0 in each group exchange their high values
    ULMStatus_t recvStatus;
    tag = Communicator::INTERCOMM_TAG;
    int remoteHigh;
    returnValue = peerExchange(interComm,
                               communicators[interComm]->localGroupsComm,
                               0, 0, tag, &high, sizeof(int), &remoteHigh,
                               sizeof(int), &recvStatus);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }

    // ensure all process in the local group have the same high value
    localGroupSize = communicators[interComm]->localGroup->groupSize;
    allLocalHighs = (int *) ulm_malloc(sizeof(int) * localGroupSize);
    if (!allLocalHighs) {
        returnValue = MPI_ERR_NO_SPACE;
        goto BarrierTag;
    }

    //returnValue=ulm_gather(communicators[interComm]->localGroupsComm,
    // 0, tag, allLocalHighs, &high, sizeof(int));
    returnValue = ulm_gather(&high, 1, (ULMType_t *) MPI_INT,
                             allLocalHighs, 1, (ULMType_t *) MPI_INT, 0,
                             communicators[interComm]->localGroupsComm);

    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }
    returnValue = ulm_bcast(allLocalHighs, sizeof(int) * localGroupSize,
                            (ULMType_t *) MPI_BYTE, 0,
                            communicators[interComm]->localGroupsComm);
    if (returnValue != ULM_SUCCESS) {
        returnValue = MPI_ERR_INTERN;
        goto BarrierTag;
    }
    compareValue = allLocalHighs[0];
    allSame = true;
    for (int i = 1; i < localGroupSize; i++) {
        if (allLocalHighs[i] != compareValue) {
            allSame = false;
            break;
        }
    }
    if (!allSame) {
        returnValue = MPI_ERR_ARG;
        goto BarrierTag;
    }
    // broadcast remote high value to all members of the local group
    returnValue =
        ulm_bcast(&remoteHigh, sizeof(int), (ULMType_t *) MPI_BYTE, 0,
                  communicators[interComm]->localGroupsComm);
    if (returnValue != ULM_SUCCESS) {
        returnValue = MPI_ERR_INTERN;
        goto BarrierTag;
    }
    // decide which group is first on list
    if (high == remoteHigh) {
        int localProc0 =
            communicators[interComm]->localGroup->
            mapGroupProcIDToGlobalProcID[0];
        int remoteProc0 =
            communicators[interComm]->remoteGroup->
            mapGroupProcIDToGlobalProcID[0];
        if (localProc0 > remoteProc0) {
            high = 1;
        } else {
            high = 0;
        }
    }
    // get new context ID
    returnValue = communicators[interComm]->getNewContextID(newIntraComm);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }
    //
    // setup new group
    //
    // setup new list of group members
    sizeOfNewGroup = communicators[interComm]->localGroup->groupSize +
        communicators[interComm]->remoteGroup->groupSize;
    newGroup = (int *) ulm_malloc(sizeof(int) * sizeOfNewGroup);
    if (!newGroup) {
        returnValue = MPI_ERR_NO_SPACE;
        goto BarrierTag;
    }
    if (high) {
        int cnt = 0;
        for (int i = 0;
             i < communicators[interComm]->remoteGroup->groupSize;
             i++, cnt++) {
            newGroup[cnt] =
                communicators[interComm]->remoteGroup->
                mapGroupProcIDToGlobalProcID[i];
        }
        for (int i = 0;
             i < communicators[interComm]->localGroup->groupSize;
             i++, cnt++) {
            newGroup[cnt] =
                communicators[interComm]->localGroup->
                mapGroupProcIDToGlobalProcID[i];
        }
    } else {
        int cnt = 0;
        for (int i = 0;
             i < communicators[interComm]->localGroup->groupSize;
             i++, cnt++) {
            newGroup[cnt] =
                communicators[interComm]->localGroup->
                mapGroupProcIDToGlobalProcID[i];
        }
        for (int i = 0;
             i < communicators[interComm]->remoteGroup->groupSize;
             i++, cnt++) {
            newGroup[cnt] =
                communicators[interComm]->remoteGroup->
                mapGroupProcIDToGlobalProcID[i];
        }
    }

    // create new group
    int newGroupIndex;
    int worldGroup;
    returnValue = ulm_comm_group(ULM_COMM_WORLD, &worldGroup);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }
    returnValue = ulm_group_incl(worldGroup, sizeOfNewGroup, newGroup,
                                 &newGroupIndex);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }
    // increment group counter - ulm_group_incl sets ref count to 1
    grpPool.groups[newGroupIndex]->incrementRefCount();

    // create new communicator
    returnValue = ulm_communicator_alloc(*newIntraComm,
                                         communicators[interComm]->
                                         useThreads, newGroupIndex,
                                         newGroupIndex, 0, 0, (int *) NULL,
                                         Communicator::INTRA_COMMUNICATOR,
                                         1, 1, 1);

  BarrierTag:
    int tmpreturnValue = returnValue;

    //
    // block until all processes are done - to make sure remote communication
    //   infrastructure is in place.
    //
    // barrier over local communictor
    returnValue = ulm_barrier(communicators[interComm]->localGroupsComm);
    if (returnValue != ULM_SUCCESS) {
        if (allLocalHighs) {
            ulm_free(allLocalHighs);
        }
        if (newGroup) {
            ulm_free(newGroup);
        }
        return returnValue;
    }
    // exchange peer data
    tag = Communicator::INTERCOMM_TAG;
    returnValue = peerExchange(interComm,
                               communicators[interComm]->localGroupsComm,
                               0, 0, tag, (void *) NULL, 0, (void *) NULL,
                               0, &recvStatus);
    if (returnValue != MPI_SUCCESS) {
        if (allLocalHighs) {
            ulm_free(allLocalHighs);
        }
        if (newGroup) {
            ulm_free(newGroup);
        }
        return returnValue;
    }
    // barrier over local communictor
    returnValue = ulm_barrier(communicators[interComm]->localGroupsComm);
    if (returnValue != ULM_SUCCESS) {
        if (allLocalHighs) {
            ulm_free(allLocalHighs);
        }
        if (newGroup) {
            ulm_free(newGroup);
        }
        return returnValue;
    }
    // free temporaries
    if (allLocalHighs) {
        ulm_free(allLocalHighs);
    }
    if (newGroup) {
        ulm_free(newGroup);
    }
    // return
    returnValue = tmpreturnValue;
    return returnValue;
}
