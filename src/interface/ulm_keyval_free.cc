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

/*
 *  This routine is used to free attribute data
 */
extern "C" int ulm_keyval_free(int *keyval)
{
    /* check to make sure keyval is within range */
    if( *keyval >= attribPool.poolSize ) {
	return MPI_ERR_KEYVAL;
    }

    /* lock element */
    attribPool.attributes[*keyval].Lock.lock();

    /* check to see if keyval is in use */
    if( !attribPool.attributes[*keyval].inUse ) {
	attribPool.attributes[*keyval].Lock.unlock();
	return MPI_ERR_KEYVAL;
    }

    /* decrement reference count */
    attribPool.attributes[*keyval].refCount--;

    /* if refrence count <= 0, free resource */
    if( attribPool.attributes[*keyval].refCount <= 0 ) {

	/* lock pool */
	attribPool.poolLock.lock();

	/* set inUse */
	attribPool.attributes[*keyval].inUse=0;

	/* reset eleInUse */
	attribPool.eleInUse--;

	/* reset firstFreeElement */
	if( *keyval < attribPool.firstFreeElement ) {
	    attribPool.firstFreeElement=*keyval;
	}

	/* unlock pool */
	attribPool.poolLock.unlock();
    }

    /* unlock individual element */
    attribPool.attributes[*keyval].Lock.unlock();

    /* reset keyval */
    *keyval=MPI_KEYVAL_INVALID;

    return MPI_SUCCESS;
}
