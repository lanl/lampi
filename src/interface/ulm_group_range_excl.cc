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
//! form the new group by excluding the ranges of ranks listed
//
extern "C" int ulm_group_range_excl(int group, int nTriplets, int ranges[][3],
				    int *newGroupIndex)
{
    int retValue = MPI_SUCCESS;

    // excluding anything from the empty group gives the empty group
    if (group == (int) MPI_GROUP_EMPTY) {
	*newGroupIndex = (int) MPI_GROUP_EMPTY;
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

    // special case: nothing to exclude...

    if (nTriplets == 0) {
        *newGroupIndex = group;
        return retValue;
    }

    //
    // pull out elements
    //
    int *elementsInList = (int *) ulm_malloc(sizeof(int) * grpPtr->groupSize);
    if (!elementsInList) {
	return MPI_ERR_OTHER;
    }
    for (int proc = 0; proc < grpPtr->groupSize; proc++) {
	elementsInList[proc] = -1;
    }

    // loop over triplet
    int nRanks = 0;
    for (int triplet = 0; triplet < nTriplets; triplet++) {
	int firstRank = ranges[triplet][0];
	int lastRank = ranges[triplet][1];
	int stride = ranges[triplet][2];
	if ((firstRank < 0) || (firstRank > grpPtr->groupSize)) {
	    return MPI_ERR_RANK;
	}
	if ((lastRank < 0) || (lastRank > grpPtr->groupSize)) {
	    return MPI_ERR_RANK;
	}
	if (stride == 0) {
	    return MPI_ERR_RANK;
	}
	if (firstRank < lastRank) {

	    if (stride < 0) {
		return MPI_ERR_RANK;
	    }
	    // positive stride
	    int index = firstRank;
	    while (index <= lastRank) {
		// make sure rank has not already been selected
		if (elementsInList[index] != -1) {
		    return MPI_ERR_RANK;
		}
		elementsInList[index] = nRanks;
		index += stride;
		nRanks++;
	    }			// end while loop

	} else if (firstRank > lastRank) {

	    if (stride > 0) {
		return MPI_ERR_RANK;
	    }
	    // negative stride
	    int index = firstRank;
	    while (index >= lastRank) {
		// make sure rank has not already been selected
		if (elementsInList[index] != -1) {
		    return MPI_ERR_RANK;
		}
		elementsInList[index] = nRanks;
		index += stride;
		nRanks++;
	    }			// end while loop

	} else {
	    // firstRank == lastRank

	    int index = firstRank;
	    if (elementsInList[index] != -1) {
		return MPI_ERR_RANK;
	    }
	    elementsInList[index] = nRanks;
	    nRanks++;
	}
    }

    int *procIDs = (int *) ulm_malloc(sizeof(int) * nRanks);
    if (!procIDs) {
	return MPI_ERR_OTHER;
    }
    for (int proc = 0; proc < grpPtr->groupSize; proc++) {
	if (elementsInList[proc] >= 0) {
	    procIDs[elementsInList[proc]] = grpPtr->mapGroupProcIDToGlobalProcID[proc];
	}
    }

    retValue = ulm_group_excl(group, nRanks, procIDs, newGroupIndex);

    ulm_free(procIDs);
    ulm_free(elementsInList);

    return retValue;
}
