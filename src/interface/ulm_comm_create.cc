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

#include <stdio.h>

#include "internal/log.h"
#include "internal/options.h"
#include "internal/profiler.h"
#include "ulm/ulm.h"
#include "mpi/constants.h"
#include "mpi/mpi.h"
#include "queue/globals.h"

/*
 * create a communicator from a group
 */
extern "C" int ulm_comm_create(int comm, int group, int *newComm)
{
    int returnValue;
    bool isSubSet;
    Group *grpPtr;

    if (OPT_CHECK_API_ARGS) {

        if ((comm > communicatorsArrayLen) || (comm < 0)) {
            ulm_err(("Error: ulm_comm_create: bad communicator %d\n",
                     comm));
            returnValue = MPI_ERR_COMM;
            goto BarrierTag;
        }

        if (communicators[comm] == 0L) {
            ulm_err(("Error: ulm_comm_create: bad communicator %d\n",
                     comm));
            returnValue = MPI_ERR_COMM;
            goto BarrierTag;
        }
        // verify that group is valid group
        if (group >= grpPool.poolSize) {
            returnValue = MPI_ERR_GROUP;
            goto BarrierTag;
        }
        // get group pointers
        if (grpPool.groups[group] == (Group *) 0) {
            returnValue = MPI_ERR_GROUP;
            goto BarrierTag;
        }

    }                           // end OPT_CHECK_API_ARGS

    grpPtr = grpPool.groups[group];

    // check to make sure that group has %100 overlap with
    //   comm->localGroup
    isSubSet = true;
    for (int proc1 = 0; proc1 < grpPtr->groupSize; proc1++) {
        bool found = false;
        for (int proc2 = 0;
             proc2 < communicators[comm]->localGroup->groupSize; proc2++) {
            if (grpPtr->mapGroupProcIDToGlobalProcID[proc1] ==
                communicators[comm]->localGroup->
                mapGroupProcIDToGlobalProcID[proc2]) {
                found = true;
                break;
            }
        }                       // end proc2 loop
        if (!found) {
            isSubSet = false;
            break;
        }
    }                           // end proc1 loop

    // if group is not a subset of comm->localGroup, return error
    if (!isSubSet) {
        returnValue = MPI_ERR_GROUP;
        goto BarrierTag;
    }
    // get new contextID - all need to update the counter so that
    //   all processes beloging to this communicator have a consistent
    //   cache
    returnValue = communicators[comm]->getNewContextID(newComm);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }
    // check to see if calling process is a member of the group, and
    //   return MPI_COMM_NULL
    if (grpPtr->ProcID == -1) {
        *newComm = MPI_COMM_NULL;
        returnValue = MPI_SUCCESS;
        goto BarrierTag;
    }
    // increment local group count by 2
    grpPtr->incrementRefCount(2);

    // create comunicator
    returnValue =
        ulm_communicator_alloc(*newComm, communicators[comm]->useThreads,
                               group, group, false, 0, (int *) NULL,
                               Communicator::INTRA_COMMUNICATOR, true, 1,
                               communicators[comm]->
                               useSharedMemForCollectives);
  BarrierTag:
    int tmpreturnValue = returnValue;

    // block to make sure the new communicator is set up in all processes
    //   before  any data is send
    returnValue = ulm_barrier(comm);
    if (returnValue != ULM_SUCCESS) {
        return returnValue;
    }

    returnValue = tmpreturnValue;
    return returnValue;
}
