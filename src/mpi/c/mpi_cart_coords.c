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



#include "internal/mpi.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Cart_coords = PMPI_Cart_coords
#endif

int PMPI_Cart_coords(MPI_Comm comm, int rank, int maxdims, int *coords)
{
    ULMTopology_t *topology;
    int factor;
    int i;
    int rc;
    int size;

    if (_mpi.check_args) {
        rc = MPI_SUCCESS;
        if (_mpi.finalized) {
            rc = MPI_ERR_INTERN;
        } else if (coords == NULL) {
            rc = MPI_ERR_ARG;
        } else if (maxdims < 1) {
            rc = MPI_ERR_COUNT;
        } else if (ulm_invalid_comm(comm)) {
            rc = MPI_ERR_COMM;
        } else if (ulm_invalid_destination(comm, rank)) {
            rc = MPI_ERR_RANK;
        }
        if (rc != MPI_SUCCESS) {
            goto ERRHANDLER;
        }
    }

    rc = ulm_get_topology(comm, &topology);
    if (rc != ULM_SUCCESS) {
	rc = MPI_ERR_COMM;
	goto ERRHANDLER;
    }
    if ((topology == NULL) || topology->type != MPI_CART) {
	rc = MPI_ERR_TOPOLOGY;
	goto ERRHANDLER;
    }

    rc = PMPI_Comm_size(comm, &size);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }
    if (rank >= size) {
	rc = MPI_ERR_DIMS;
	goto ERRHANDLER;
    }

    factor = size;
    for (i = 0; i < topology->cart.ndims; i++) {
	factor /= topology->cart.dims[i];
	coords[i] = rank / factor;
	rank = rank % factor;
    }

    rc = MPI_SUCCESS;

ERRHANDLER:
    if (rc != MPI_SUCCESS) {
	_mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    return rc;
}
