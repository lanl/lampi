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



#include "mpi/constants.h"
#include "ulm/ulm.h"
#include "queue/globals.h"
#include "internal/malloc.h"

//
//! form the union of group1 and group2
//
extern "C" int ulm_group_union(int group1Index, int group2Index,
                               int *newGroupIndex)
{
    int retValue = MPI_SUCCESS;

    // verify that group is valid group
    if (group1Index >= grpPool.poolSize) {
        return MPI_ERR_GROUP;
    }
    if (group2Index >= grpPool.poolSize) {
        return MPI_ERR_GROUP;
    }
    if ((group1Index == (int) MPI_GROUP_EMPTY)
        && (group2Index == (int) MPI_GROUP_EMPTY)) {
        *newGroupIndex = (int) MPI_GROUP_EMPTY;
        return retValue;
    } else if (group1Index == (int) MPI_GROUP_EMPTY) {
        *newGroupIndex = group2Index;
        // increment group reference count
        Group *grp2Ptr = grpPool.groups[group2Index];
        if (grp2Ptr == 0) {
            return MPI_ERR_GROUP;
        }
        grp2Ptr->incrementRefCount(1);
        return retValue;
    } else if (group2Index == (int) MPI_GROUP_EMPTY) {
        *newGroupIndex = group1Index;
        // increment group reference count
        Group *grp1Ptr = grpPool.groups[group1Index];
        if (grp1Ptr == 0) {
            return MPI_ERR_GROUP;
        }
        grp1Ptr->incrementRefCount(1);
        return retValue;
    }
    Group *grp1Ptr = grpPool.groups[group1Index];
    if (grp1Ptr == 0) {
        return MPI_ERR_GROUP;
    }
    Group *grp2Ptr = grpPool.groups[group2Index];
    if (grp2Ptr == 0) {
        return MPI_ERR_GROUP;
    }
    // get new group element
    int returnValue = getGroupElement(newGroupIndex);
    if (returnValue != ULM_SUCCESS) {
        return returnValue;
    }
    // get new group pointer
    Group *newGroupPtr = grpPool.groups[*newGroupIndex];

    //
    // form union
    //

    // check to see if the group is empty
    if ((grp1Ptr->groupSize + grp2Ptr->groupSize) == 0) {
        newGroupPtr->groupSize = 0;
        return ULM_SUCCESS;
    }
    // allocate memory for maximum number possible
    int *procIDs = (int *) ulm_malloc(sizeof(int) *
                                      (grp1Ptr->groupSize +
                                       grp2Ptr->groupSize));
    if (!procIDs) {
        return MPI_ERR_OTHER;
    }
    // put group1 elements in the list
    int newGroupSize = grp1Ptr->groupSize;
    for (int proc = 0; proc < newGroupSize; proc++) {
        procIDs[proc] = grp1Ptr->mapGroupProcIDToGlobalProcID[proc];
    }

    // check group2 elements to see if they need to be included in the list
    for (int proc = 0; proc < grp2Ptr->groupSize; proc++) {
        int globalProcID = grp2Ptr->mapGroupProcIDToGlobalProcID[proc];
        // check to see if this proc is alread in the group
        bool foundInGroup1 = false;
        for (int proc1 = 0; proc1 < grp1Ptr->groupSize; proc1++) {
            if (grp1Ptr->mapGroupProcIDToGlobalProcID[proc1] ==
                globalProcID) {
                foundInGroup1 = true;
                break;
            }
        }                       // end proc1 loop

        if (foundInGroup1)
            continue;

        procIDs[newGroupSize] = globalProcID;
        newGroupSize++;
    }                           // end proc loop

    // setup the group object
    returnValue =
        newGroupPtr->create(newGroupSize, procIDs, *newGroupIndex);
    ulm_free(procIDs);

    return retValue;
}
