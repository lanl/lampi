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
#pragma weak MPI_Cart_shift = PMPI_Cart_shift
#endif

int PMPI_Cart_shift(MPI_Comm comm, int direction, int disp,
		    int *rank_source, int *rank_dest)
{
    ULMTopology_t *topology;
    int coord;
    int coord_dest;
    int coord_source;
    int period;
    int rc;
    int size;

    if (rank_source == NULL || rank_dest == NULL) {
	rc = MPI_ERR_ARG;
	goto ERRHANDLER;
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

    if (direction < 0 || direction >= topology->cart.ndims) {
	rc = MPI_ERR_ARG;
	goto ERRHANDLER;
    }

    if (disp == 0) {
	int rank;

	rc = PMPI_Comm_rank(comm, &rank);
	if (rc != MPI_SUCCESS) {
	    goto ERRHANDLER;
	}
	*rank_source = rank;
	*rank_dest = rank;
	goto ERRHANDLER;
    }

    size = topology->cart.dims[direction];
    coord = topology->cart.position[direction];
    period = topology->cart.periods[direction];

    coord_source = coord - disp;
    coord_dest  = coord + disp;

    *rank_source = 0;
    *rank_dest = 0;

    if (!period) {
	if (coord_source < 0 || coord_source >= size) {
	    *rank_source = MPI_PROC_NULL;
	}
	if (coord_dest < 0 || coord_dest >= size) {
	    *rank_dest = MPI_PROC_NULL;
	}
    }

    if (*rank_source != MPI_PROC_NULL) {
	topology->cart.position[direction] = coord_source;
	rc = PMPI_Cart_rank(comm, topology->cart.position, rank_source);
	if (rc != MPI_SUCCESS) {
	    goto ERRHANDLER;
	}
	topology->cart.position[direction] = coord;
    }

    if (*rank_dest != MPI_PROC_NULL) {
	topology->cart.position[direction] = coord_dest;
	rc = PMPI_Cart_rank(comm, topology->cart.position, rank_dest);
	if (rc != MPI_SUCCESS) {
	    goto ERRHANDLER;
	}
	topology->cart.position[direction] = coord;
    }

    rc = MPI_SUCCESS;

ERRHANDLER:
    if (rc != MPI_SUCCESS) {
	_mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    return rc;
}
