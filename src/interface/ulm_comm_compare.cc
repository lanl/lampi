/*
 * Copyright 2002-2004. The Regents of the University of
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



#include "mpi/constants.h"
#include "os/atomic.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

//
//! compare communcator1 and communictor2
//

#include "internal/options.h"

extern "C" int ulm_comm_compare(int comm1, int comm2, int *result)
{
    int returnValue = MPI_SUCCESS;

    if (OPT_CHECK_API_ARGS) {
	if ((comm1 < 0 && comm1 != MPI_COMM_NULL) ||
	    communicators[comm1] == 0L) {
	    ulm_err(("Error: ulm_group_compare: bad communicator %d\n",
		     comm1));
	    return MPI_ERR_COMM;
	}
	if ((comm2 < 0 && comm2 != MPI_COMM_NULL) ||
	    communicators[comm2] == 0L) {
	    ulm_err(("Error: ulm_group_compare: bad communicator %d\n",
		     comm2));
	    return MPI_ERR_COMM;
	}
    }

    // check to see if communictors are identical
    if (comm1 == comm2) {
	*result = MPI_IDENT;
	return MPI_SUCCESS;
    }

    // handle MPI_COMM_NULL
    if (comm1 == MPI_COMM_NULL || comm2 == MPI_COMM_NULL) {
	*result = MPI_UNEQUAL;
	return MPI_SUCCESS;
    }

    // check to make sure that comm1 and comm2 are  of same type
    if (communicators[comm1]->communicatorType !=
	communicators[comm2]->communicatorType) {
	*result = MPI_UNEQUAL;
	return MPI_SUCCESS;
    }

    Communicator *comm1Ptr = communicators[comm1];
    Communicator *comm2Ptr = communicators[comm2];

    // check to see if local groups are same size
    if (comm1Ptr->localGroup->groupSize != comm2Ptr->localGroup->groupSize) {
	*result = MPI_UNEQUAL;
	return MPI_SUCCESS;
    }
    // groups are same size, check to see if they are identical, reordered, or
    //  different
    if (comm1Ptr->localGroup->groupID == comm2Ptr->localGroup->groupID) {
	*result = MPI_CONGRUENT;
	return MPI_SUCCESS;
    }

    bool sameOrder = true;
    bool sameGlobalProcIDs = true;
    // loop over all group1 ranks
    for (int proc1 = 0; proc1 < comm1Ptr->localGroup->groupSize; proc1++) {
	// loop over group2 ranks and check for match
	bool found = false;
	for (int proc2 = 0; proc2 < comm2Ptr->localGroup->groupSize;
	     proc2++) {
	    if (comm1Ptr->localGroup->mapGroupProcIDToGlobalProcID[proc1] ==
		comm2Ptr->localGroup->mapGroupProcIDToGlobalProcID[proc2]) {
		found = true;
		if (proc1 != proc2) {
		    sameOrder = false;
		}
	    }
	}			// end proc2 loop
	if (!found) {
	    sameGlobalProcIDs = false;
	    break;
	}
    }				// end proc1 loop

    // set result
    if (sameGlobalProcIDs) {
	if (sameOrder) {
	    *result = MPI_CONGRUENT;
	} else {
	    *result = MPI_SIMILAR;
	}
    } else {
	*result = MPI_UNEQUAL;
    }

    // check to see if any need to check remote group
    if (*result == MPI_UNEQUAL) {
	return returnValue;
    }
    // for remote groups check remote group
    if (communicators[comm1]->communicatorType ==
	Communicator::INTER_COMMUNICATOR) {

	// check to see if remote groups are same size
	if (comm1Ptr->remoteGroup->groupSize !=
	    comm2Ptr->remoteGroup->groupSize) {
	    *result = MPI_UNEQUAL;
	    return MPI_SUCCESS;
	}
	// groups are same size, check to see if they are identical, reordered, or
	//  different
	if (comm1Ptr->remoteGroup->groupID ==
	    comm2Ptr->remoteGroup->groupID) {
	    *result = MPI_CONGRUENT;
	    return MPI_SUCCESS;
	}

	bool sameOrder = true;
	bool sameGlobalProcIDs = true;
	// loop over all group1 ranks
	for (int proc1 = 0; proc1 < comm1Ptr->remoteGroup->groupSize;
	     proc1++) {
	    // loop over group2 ranks and check for match
	    bool found = false;
	    for (int proc2 = 0; proc2 < comm2Ptr->remoteGroup->groupSize;
		 proc2++) {
		if (comm1Ptr->remoteGroup->mapGroupProcIDToGlobalProcID[proc1] ==
		    comm2Ptr->remoteGroup->mapGroupProcIDToGlobalProcID[proc1]) {
		    found = true;
		    if (proc1 != proc2) {
			sameOrder = false;
		    }
		}
	    }			// end proc2 loop
	    if (!found) {
		sameGlobalProcIDs = false;
		break;
	    }
	}			// end proc1 loop


	// set result
	if (sameGlobalProcIDs) {
	    if (sameOrder) {
		if (*result == MPI_CONGRUENT) {
		    // local group is also congruent
		    *result = MPI_CONGRUENT;
		} else {
		    // local group similat
		    *result = MPI_SIMILAR;
		}
	    } else {
		*result = MPI_SIMILAR;
	    }
	} else {
	    *result = MPI_UNEQUAL;
	}

    }				// end inter-communicator section

    return returnValue;
}
