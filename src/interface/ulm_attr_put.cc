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

extern "C" int ulm_attr_put(int comm, int keyval, void *attributeVal)

{
    /* check for MPI_KEYVAL_INVALID */
    if( keyval == MPI_KEYVAL_INVALID ) {
	return MPI_ERR_KEYVAL;
    }

    /* check to see if this is a valid keyval */
    if( (keyval < 0) || (keyval >= attribPool.poolSize) ||
	(attribPool.attributes[keyval].inUse==0 )   ) {
	return MPI_ERR_KEYVAL;
    }


    Communicator *commPtr = (Communicator *) communicators[comm];

    /* check to see if comm is valid */
    if( commPtr == (Communicator *)0 ) {
	return MPI_ERR_COMM;
    }

    /* lock communicator */
    commPtr->commLock.lock();

    /* check to see if keyval is already in use */
    int keyvalInUse=-1;
    bool keyvalFound=false;
    for( int key=0 ; key < commPtr->sizeOfAttributeArray ; key++ ) {
	if( commPtr->attributeList[key].keyval == keyval ){
	    keyvalInUse=key;
	    keyvalFound=true;
	    break;
	}
    }

    /* if keyval is already in use, run delete_fn */
    if( keyvalInUse>=0 ) {

	/* if return value is not MPI_SUCCES - return failure */
	int errorCode;
	if( attribPool.attributes[keyval].setFromFortran ){
	    /* fortran version */
	    attribPool.attributes[keyval].fDeleteFunction(&comm, &keyval,
                                                          (int *)&(commPtr->attributeList[keyvalInUse].attributeValue),
                                                          (int *)(attribPool.attributes[keyval].extraState), &errorCode);
	} else {
	    /* c version */
	    errorCode=
		attribPool.attributes[keyval].cDeleteFunction(comm, keyval,
                                                              commPtr->attributeList[keyvalInUse].attributeValue,
                                                              attribPool.attributes[keyval].extraState);
	}

	if( errorCode != MPI_SUCCESS ) {
	    commPtr->commLock.unlock();
	    return errorCode;
	}

    }

    /* if keyval not in use, create net instance for use */
    if( keyvalInUse == -1 ) {

	/* check to see if need to grow the list */
	if( commPtr->attributeCount == commPtr->sizeOfAttributeArray ){
	    commPtr->attributeList=(commAttribute_t *)realloc( commPtr->attributeList,
                                                               ( sizeof(commAttribute_t)* (commPtr->attributeCount+
                                                                                           Communicator::ATTRIBUTE_CACHE_GROWTH_INCREMENT) ) );
	    if( !commPtr->attributeList ) {
		commPtr->commLock.unlock();
		return MPI_ERR_NO_SPACE;
	    }
	    for( int i=commPtr->sizeOfAttributeArray ;
		 i<(commPtr->sizeOfAttributeArray+Communicator::ATTRIBUTE_CACHE_GROWTH_INCREMENT) ; i++ ) {
		commPtr->attributeList[i].keyval=-1;
		commPtr->attributeList[i].attributeValue=(void *)0;
	    }
	    commPtr->sizeOfAttributeArray+=Communicator::ATTRIBUTE_CACHE_GROWTH_INCREMENT;
	}

	/* search for free element */
	for( int i=0 ; i < commPtr->sizeOfAttributeArray ; i++ ) {
	    if( commPtr->attributeList[i].keyval == -1 ) {
		commPtr->attributeCount++;
		commPtr->attributeList[i].keyval=keyval;
		keyvalInUse=i;
		break;
	    }
	}

    }


    /* if need, increment keyval refcount */
    if( !keyvalFound ) {
	attribPool.attributes[keyval].Lock.lock();
	attribPool.attributes[keyval].refCount++;
	attribPool.attributes[keyval].Lock.unlock();
    }

    /* set new attribute value */
    commPtr->attributeList[keyvalInUse].attributeValue=attributeVal;

    /* unlock communicator */
    commPtr->commLock.unlock();

    return MPI_SUCCESS;
}
