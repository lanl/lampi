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



#include "queue/Communicator.h"
#include "queue/globals.h"
#include "mpi/constants.h"
#include "internal/ftoc.h"

/*
 * associate an attribute with a communicator
 */

static int mpi_tag_ub_value = MPI_TAG_UB_VALUE;
static int mpi_proc_null_value = (int)MPI_PROC_NULL;

/* this value should be set to 1 if explicit effort has been
 * taken to synchronize clocks on this system...
 */
static int mpi_wtime_is_global_value = 0;

extern "C" int ulm_attr_get(int comm, int keyval, void *attributeVal, int *flag)

{
    /* initialize flag */
    *flag=0;

    Communicator *commPtr = (Communicator *) communicators[comm];
    int **integerResult = (int **)attributeVal;

    if ( keyval < 0) {
	switch (keyval) {
	case MPI_TAG_UB:
	    *flag = 1;
	    *integerResult = &(mpi_tag_ub_value);
	    return MPI_SUCCESS;
	case MPI_IO:
	    if (commPtr == (Communicator *)0) {
		return MPI_ERR_COMM;
	    }
	    *flag = 1;
	    *integerResult = &(commPtr->localGroup->ProcID);
	    return MPI_SUCCESS;
	case MPI_HOST:
	    *flag = 1;
	    *integerResult = &(mpi_proc_null_value);
	    return MPI_SUCCESS;
	case MPI_WTIME_IS_GLOBAL:
	    *flag = 1;
	    *integerResult = &(mpi_wtime_is_global_value);
	    return MPI_SUCCESS;
	case MPI_KEYVAL_INVALID:
	default:
	    return MPI_ERR_KEYVAL;
	}

    }

    /* check to see if this is a valid keyval */
    if ( (keyval >= attribPool.poolSize) ||
	 (attribPool.attributes[keyval].inUse==0 )   ) {
	return MPI_ERR_KEYVAL;
    }

    /* check to see if comm is valid */
    if( commPtr == (Communicator *)0 ) {
	return MPI_ERR_COMM;
    }

    /* lock communicator */
    commPtr->commLock.lock();

    /* check to see if keyval is already in use */
    int keyvalInUse=-1;
    for( int key=0 ; key < commPtr->sizeOfAttributeArray ; key++ ) {
	if( commPtr->attributeList[key].keyval == keyval ){
	    keyvalInUse=key;
	    *flag=1;
	    break;
	}
    }

    /*
     * if keyval not in use by this communicator - return with out setting
     *   any return value
     */
    if( keyvalInUse< 0 ) {
	commPtr->commLock.unlock();
	return MPI_SUCCESS;
    }

    /* set return value */
    if (attribPool.attributes[keyval].setFromFortran) {
	long int tmpValue = (long int)commPtr->attributeList[keyvalInUse].attributeValue;
	*((int *)attributeVal)= (int)tmpValue;
    }
    else {
	*((void **)attributeVal)=commPtr->attributeList[keyvalInUse].attributeValue;
    }

    /* unlock communicator */
    commPtr->commLock.unlock();

    return MPI_SUCCESS;
}
