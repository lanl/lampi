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

#pragma weak MPI_Cart_sub = PMPI_Cart_sub
int PMPI_Cart_sub(MPI_Comm comm_old, int *remain_dims, MPI_Comm * comm_new)
{
    ULMTopology_t *topology_new=NULL;
    ULMTopology_t *topology_old=NULL;
    int i;
    int j;
    int color;
    int key;
    int ndims_new;
    int nnodes_new;
    int rank_new;
    int rank_old;
    int rc;
    void *p=NULL;

    rc = PMPI_Comm_rank(comm_old, &rank_old);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }
    if (comm_new == NULL || remain_dims == NULL) {
	rc = MPI_ERR_ARG;
	goto ERRHANDLER;
    }

    *comm_new = MPI_COMM_NULL;

    rc = ulm_get_topology(comm_old, &topology_old);
    if (rc != ULM_SUCCESS) {
	rc = MPI_ERR_COMM;
	goto ERRHANDLER;
    }

    if ((topology_old == NULL) || (topology_old->type != MPI_CART)) {
	rc = MPI_ERR_TOPOLOGY;
	goto ERRHANDLER;
    }

    ndims_new = 0;
    nnodes_new = 1;
    for (i = 0; i < topology_old->cart.ndims; i++) {
	if (remain_dims[i]) {
	    ndims_new++;
	    nnodes_new *= topology_old->cart.dims[i];
	}
    }

    key = 0;
    color = 0;
    for (i = 0; i < topology_old->cart.ndims; i++) {
	if (remain_dims[i]) {
	    key =
		key * topology_old->cart.dims[i] +
		topology_old->cart.position[i];
	} else {
	    color =
		color * topology_old->cart.dims[i] +
		topology_old->cart.position[i];
	}
    }
    rc = PMPI_Comm_split(comm_old, color, key, comm_new);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }

    topology_new = (ULMTopology_t *) ulm_malloc(sizeof(ULMTopology_t));
    if (topology_new == NULL) {
	rc = MPI_ERR_OTHER;
	goto ERRHANDLER;
    }
    p = ulm_malloc(3 * ndims_new * sizeof(int));
    if (p == NULL) {
	rc = MPI_ERR_OTHER;
	goto ERRHANDLER;
    }

    topology_new->type = MPI_CART;
    topology_new->cart.nnodes = nnodes_new;
    topology_new->cart.ndims = ndims_new;
    topology_new->cart.dims = (int *) p;
    topology_new->cart.periods = (int *) p + ndims_new;
    topology_new->cart.position = (int *) p + 2 * ndims_new;

    rc = PMPI_Comm_rank(*comm_new, &rank_new);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }

    j = 0;
    for (i = 0; i < topology_old->cart.ndims; i++) {
	if (remain_dims[i]) {
	    topology_new->cart.dims[j] = topology_old->cart.dims[i];
	    topology_new->cart.periods[j] = topology_old->cart.periods[i];

	    nnodes_new /= topology_new->cart.dims[j];
	    topology_new->cart.position[j] = rank_new / nnodes_new;
	    rank_new = rank_new % nnodes_new;

	    j++;
	}
    }

    rc = ulm_set_topology(*comm_new, topology_new);
    if (rc != ULM_SUCCESS) {
	rc = MPI_ERR_TOPOLOGY;
	goto ERRHANDLER;
    }

    rc = MPI_SUCCESS;

ERRHANDLER:
    if (rc != MPI_SUCCESS) {
	if (p != NULL) {
	    free(p);
	}
	if (topology_new != NULL) {
	    free(topology_new);
	}
	if (*comm_new != MPI_COMM_NULL) {
	    PMPI_Comm_free(comm_new);
	}
	_mpi_errhandler(comm_old, rc, __FILE__, __LINE__);
    }

    return rc;
}
