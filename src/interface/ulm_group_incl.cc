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



#include "ulm/ulm.h"
#include "queue/globals.h"
#include "internal/malloc.h"

//
//! form the new group out of the list of ranks in the input group passed in
//
extern "C" int ulm_group_incl(int group, int nRanks, int *ranks,
			      int *newGroupIndex)
{
    int retValue = MPI_SUCCESS;

    // including anything of the empty group is still the empty group
    if ((group == (int) MPI_GROUP_EMPTY) || (nRanks == 0)) {
        *newGroupIndex = MPI_GROUP_EMPTY;
        return retValue;
    }
    // verify that group is valid group
    if (group >= grpPool.poolSize) {
        return MPI_ERR_GROUP;
    }

    Group *grpPtr = grpPool.groups[group];
    if (grpPtr == 0) {
        return MPI_ERR_GROUP;
    }

    if (nRanks > grpPtr->groupSize) {
        return MPI_ERR_RANK;
    }
    // get new group element
    int returnValue = getGroupElement(newGroupIndex);
    if (returnValue != ULM_SUCCESS) {
        return MPI_ERR_OTHER;
    }
    // get new group pointer
    Group *newGroupPtr = grpPool.groups[*newGroupIndex];

    //
    // pull out elements
    //

    // check to see if the group is empty
    if (nRanks == 0) {
        newGroupPtr->groupSize = 0;
        return MPI_SUCCESS;
    }
    // allocate memory for maximum number possible
    int *procIDs = (int *) ulm_malloc(sizeof(int) * nRanks);
    if (!procIDs) {
        return MPI_ERR_OTHER;
    }
    // put group elements in the list

    // loop over group1 members
    for (int proc = 0; proc < nRanks; proc++) {

        if ((ranks[proc] < 0) || (ranks[proc] > grpPtr->groupSize))
            return MPI_ERR_RANK;

        procIDs[proc] = grpPtr->mapGroupProcIDToGlobalProcID[ranks[proc]];

    }                           // end proc loop

    // setup Group
    returnValue = newGroupPtr->create(nRanks, procIDs, *newGroupIndex);

    ulm_free(procIDs);

    return retValue;
}
