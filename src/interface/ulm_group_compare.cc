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

#include "mpi/constants.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

//
//! compare group1 and group2
//

extern "C" int ulm_group_compare(int group1Index, int group2Index, int *result)
{
    int returnValue=MPI_SUCCESS;
    // verify that group is valid group
    if( group1Index >= grpPool.poolSize ){
	return MPI_ERR_GROUP;
    }
    if( group2Index >= grpPool.poolSize ){
	return MPI_ERR_GROUP;
    }

    // get group pointers
    Group *grp1Ptr=grpPool.groups[group1Index];
    if( grp1Ptr == 0 ) {
	return MPI_ERR_GROUP;
    }
    Group *grp2Ptr=grpPool.groups[group2Index];
    if( grp2Ptr == 0 ) {
	return MPI_ERR_GROUP;
    }

    // compare sizes
    if( grp1Ptr->groupSize != grp2Ptr->groupSize ) {

	// if not same size - return
	*result=MPI_UNEQUAL;
	return returnValue;

    }

    // loop over group1 processes
    bool similar=true;
    bool identical=true;
    for(int proc1=0 ; proc1 < grp1Ptr->groupSize ; proc1++ ) {

	int globProc1=grp1Ptr->mapGroupProcIDToGlobalProcID[proc1];
	// loop over group2 processes to find "match"
	int match=-1;
	for(int proc2=0 ; proc2 < grp2Ptr->groupSize ; proc2++ ) {
	    if(globProc1 ==
	       grp2Ptr->mapGroupProcIDToGlobalProcID[proc2] ) {
		if(proc1 != proc2 ) {
		    identical=false;
		}
		match=proc2;
		break;
	    }
	} // end proc2 loop
	if( match== -1 ) {
	    similar=false;
	    break;
	}
    } // end proc1 loop

    // set comparison result
    if( !similar ) {
	*result=MPI_UNEQUAL;
    } else if( !identical ) {
	*result=MPI_SIMILAR;
    } else {
	*result=MPI_IDENT;
    }

    return returnValue;
}
