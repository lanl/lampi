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
#include "mpi/constants.h"
#include "queue/globals.h"

//
//! free group element - if reference count i 0, return to the pool
//

int ulm_group_free(int group)

{
    //
    int retValue=MPI_SUCCESS;

    // verify that group is valid group
    if( group >= grpPool.poolSize ){
	return MPI_ERR_GROUP;
    }

    Group *grpPtr=grpPool.groups[group];
    if( grpPtr == 0 ) {
	return MPI_ERR_GROUP;
    }

    // lock group pool
    grpPool.poolLock.lock();

    // free group
    retValue=grpPool.groups[group]->freeGroup();
    if( retValue != ULM_SUCCESS)
	return MPI_ERR_OTHER;

    // if group reference count == 0, return it to pool
    if( grpPool.groups[group]->refCount == 0 ) {

	// free the group
        free(grpPool.groups[group]);

	// set the groups to 0
        grpPool.groups[group]=0;

	// reset firstFreeElement, if appropriate
        if( group < grpPool.firstFreeElement) {
	    grpPool.firstFreeElement=group;
        }

    }

    // unlock group pool
    grpPool.poolLock.unlock();

    return retValue;
}
