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

#include "queue/Communicator.h"
#include "queue/globals.h"
#include "mpi/constants.h"
#include "internal/ftoc.h"

/*
 * This routine is used to set up a new attributes key.  This is called
 *   via the "Fortran" like interface.
 */
extern "C" int ulm_f_keyval_create(void *copy_fp,
				   void *delete_fp,
                                   int *keyval, void *extraState)
{
    Fortran_Copy_function *copyFunction;
    Fortran_Delete_function *deleteFunction;

    copyFunction = (Fortran_Copy_function *) copy_fp;
    deleteFunction = (Fortran_Delete_function *) delete_fp;

    /* lock pool - thread safety*/
    attribPool.poolLock.lock();

    /*
     * find free element
     */
    int eleToUse;

    if( attribPool.firstFreeElement != attribPool.poolSize ) {

	/* find free element */
	eleToUse=attribPool.firstFreeElement;

	/* find next free element */
	attribPool.firstFreeElement=attribPool.poolSize;
	for( int ele=eleToUse+1 ; ele < attribPool.poolSize ; ele++ ) {
	    if( attribPool.attributes[ele].inUse != 1 ){
		attribPool.firstFreeElement=ele;
		break;
	    }
	}

    } else {

	/* if free element no available - resize array */
	attribPool.attributes=(attributes_t *)realloc(attribPool.attributes,
                                                      sizeof(attributes_t)*
                                                      ( attribPool.poolSize+Communicator::ATTRIBUTE_CACHE_GROWTH_INCREMENT ) );
	if( !attribPool.attributes) {
	    ulm_err(("Error: allocating space for attribPool.attributes\n"));
	    attribPool.poolLock.unlock();
	    return MPI_ERR_BUFFER;
	}

	/* find free element */
	eleToUse=attribPool.poolSize;

	/* find next free element */
	attribPool.firstFreeElement=eleToUse+1;

	/* reset poolSize */
	attribPool.poolSize+=Communicator::ATTRIBUTE_CACHE_GROWTH_INCREMENT;

    }

    /* set keyval - just the index of the element obtained */
    *keyval=eleToUse;

    /* set extraState */
    attribPool.attributes[eleToUse].extraState=extraState;

    /* set setFromFortran to false */
    attribPool.attributes[eleToUse].setFromFortran=1;

    /* set cCopyFunction */
    attribPool.attributes[eleToUse].fCopyFunction=copyFunction;

    /* set cDeleteFunction */
    attribPool.attributes[eleToUse].fDeleteFunction=deleteFunction;

    /* set inUse flag */
    attribPool.attributes[eleToUse].inUse=1;

    /* set eleInUse */
    attribPool.eleInUse+=1;

    /* initialize lock - for threaded protection */
    attribPool.attributes[eleToUse].Lock.init();

    /* initialize markedForDeletion and refCount */
    attribPool.attributes[eleToUse].markedForDeletion=0;
    attribPool.attributes[eleToUse].refCount=1;

    /* unlock pool */
    attribPool.poolLock.unlock();

    return MPI_SUCCESS;
}
