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

#include "internal/mpi.h"
#include "internal/buffer.h"
#include "internal/state.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Buffer_attach = PMPI_Buffer_attach
#endif

int PMPI_Buffer_attach(void *buffer, int bufferLength)
{
    int rc = MPI_SUCCESS;

    /* lock control structure */
    ATOMIC_LOCK_THREAD(lampiState.bsendData->lock);

    /* check to make sure pool is available */
    if (lampiState.bsendData->poolInUse) {

	/*
	 * buffer not available
	 */

	/* set error code */
	rc = MPI_ERR_BUFFER;

	/* unlock control structure */
        ATOMIC_UNLOCK_THREAD(lampiState.bsendData->lock);

	_mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
	return rc;
    }

    /* verify that pointer is not null */
    if (buffer == (void *) 0) {

	/* set error code */
	rc = MPI_ERR_BUFFER;

	/* unlock control structure */
        ATOMIC_UNLOCK_THREAD(lampiState.bsendData->lock);

	_mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
	return rc;

    }

    /* set the in use flag */
    lampiState.bsendData->poolInUse = 1;

    /* set buffer length */
    lampiState.bsendData->bufferLength = bufferLength;

    /* set bytes in use */
    lampiState.bsendData->bytesInUse = 0;

    /* set buffer pointer */
    lampiState.bsendData->buffer = buffer;

    /* set allocations pointer to null */
    lampiState.bsendData->allocations = NULL;

    /* unlock control structure */
    ATOMIC_UNLOCK_THREAD(lampiState.bsendData->lock);

    return rc;
}
