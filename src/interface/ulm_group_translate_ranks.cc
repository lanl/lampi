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

//
//! translate ranks in group1 to their rank in group2
//
extern "C" int ulm_group_translate_ranks(int group1Index, int numRanks,
                                         int *ranks1List, int group2Index,
                                         int *ranks2List)
{
    int retValue = MPI_SUCCESS;
    int i;

    // verify that group is valid group
    if (group1Index >= grpPool.poolSize) {
        return MPI_ERR_GROUP;
    }
    if (group2Index >= grpPool.poolSize) {
        return MPI_ERR_GROUP;
    }
    // return an error if the source group is the empty group...
    if (group1Index == (int) MPI_GROUP_EMPTY) {
        return MPI_ERR_GROUP;
    }
    // if group 2 is empty, fill the rank list with MPI_UNDEFINED
    if (group2Index == (int) MPI_GROUP_EMPTY) {
        for (i = 0; i < numRanks; i++) {
            ranks2List[i] = MPI_UNDEFINED;
        }
        return MPI_SUCCESS;
    }

    Group *grp1Ptr = grpPool.groups[group1Index];
    if (grp1Ptr == 0) {
        return MPI_ERR_GROUP;
    }
    Group *grp2Ptr = grpPool.groups[group2Index];
    if (grp2Ptr == 0) {
        return MPI_ERR_GROUP;
    }
    // loop over all ranks
    for (int proc = 0; proc < numRanks; proc++) {
        int globProc1 =
            grp1Ptr->mapGroupProcIDToGlobalProcID[ranks1List[proc]];
        // initialize to no "match"
        int groupProc2 = MPI_UNDEFINED;
        for (int proc2 = 0; proc2 < grp2Ptr->groupSize; proc2++) {
            if (globProc1 == grp2Ptr->mapGroupProcIDToGlobalProcID[proc2]) {
                groupProc2 = proc2;
                break;
            }
        }

        ranks2List[proc] = groupProc2;
    }

    return retValue;
}
