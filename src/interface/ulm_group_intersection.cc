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

#include "ulm/ulm.h"
#include "queue/globals.h"
#include "internal/malloc.h"

//
//! form the intersection of group1 and group2
//
extern "C" int ulm_group_intersection( int group1Index, int group2Index,
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
    Group *grp1Ptr = grpPool.groups[group1Index];
    if (grp1Ptr == 0) {
	return MPI_ERR_GROUP;
    }
    Group *grp2Ptr = grpPool.groups[group2Index];
    if (grp2Ptr == 0) {
	return MPI_ERR_GROUP;
    }
    if ((grp1Ptr->groupSize == 0) || (grp2Ptr->groupSize == 0)) {
	return MPI_ERR_GROUP;
    }

    //
    // form intersection
    //

    // allocate memory for maximum number possible
    int *procIDs = (int *) ulm_malloc(sizeof(int) * grp1Ptr->groupSize);
    if (!procIDs) {
	return MPI_ERR_OTHER;
    }
    // put group1 elements in the list
    int newGroupSize = 0;

    // loop over group1 members
    for (int proc = 0; proc < grp1Ptr->groupSize; proc++) {
	int globalProcID = grp1Ptr->mapGroupProcIDToGlobalProcID[proc];
	// check to see if this proc is in group2
	bool foundInGroup2 = false;
	for (int proc2 = 0; proc2 < grp2Ptr->groupSize; proc2++) {
	    if (grp2Ptr->mapGroupProcIDToGlobalProcID[proc2] == globalProcID) {
		foundInGroup2 = true;
		break;
	    }
	}			// end proc1 loop

	if (!foundInGroup2) {
	    continue;
	}
	procIDs[newGroupSize] = globalProcID;
	newGroupSize++;
    }				// end proc loop

    if (newGroupSize == 0) {
	*newGroupIndex = MPI_GROUP_EMPTY;
    } else {
	// get new group element
	retValue = getGroupElement(newGroupIndex);
	if (retValue == ULM_SUCCESS) {
	    // get new group pointer
	    Group *newGroupPtr = grpPool.groups[*newGroupIndex];
	    retValue = newGroupPtr->create(newGroupSize, procIDs, *newGroupIndex);
	}
    }

    ulm_free(procIDs);

    return retValue;
}
