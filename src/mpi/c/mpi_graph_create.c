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

#include "internal/malloc.h"
#include "internal/mpi.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Graph_create = PMPI_Graph_create
#endif

int PMPI_Graph_create(MPI_Comm comm_old, int nnodes, int *index,
		      int *edges, int reorder, MPI_Comm *comm_new)
{
    MPI_Group group_old;
    MPI_Group group_new;
    ULMTopology_t *topology;
    int i;
    int nedges;
    int range[1][3];
    int rc;
    int size_old;
    void *p;

    /*
     * Sanity checks
     *
     * Note: reorder is ignored (assumed always false)
     */

    rc = MPI_SUCCESS;
    p = NULL;
    topology = NULL;

    if (comm_new == NULL) {
	rc = MPI_ERR_ARG;
	goto ERRHANDLER;
    }

    *comm_new = MPI_COMM_NULL;

    if (nnodes < 1 || index == NULL || edges == NULL) {
	rc = MPI_ERR_ARG;
	goto ERRHANDLER;
    }

    /*
     * Create a new communicator.
     * Attributes do not propagate, so use MPI_Comm_create
     */

    rc = PMPI_Comm_size(comm_old, &size_old);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }
    if (nnodes > size_old) {
	rc = MPI_ERR_ARG;
	goto ERRHANDLER;
    }

    range[0][0] = 0;
    range[0][1] = nnodes - 1;
    range[0][2] = 1;

    rc = PMPI_Comm_group(comm_old, &group_old);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }
    rc = PMPI_Group_range_incl(group_old, 1, range, &group_new);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }
    rc = PMPI_Comm_create(comm_old, group_new, comm_new);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }
    rc = PMPI_Group_free(&group_old);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }
    rc = PMPI_Group_free(&group_new);
    if (rc != MPI_SUCCESS) {
	goto ERRHANDLER;
    }

    if (*comm_new == MPI_COMM_NULL) {
	return MPI_SUCCESS;
    }

    /*
     * Attach the topology
     */

    nedges = index[nnodes - 1];

    topology = (ULMTopology_t *) ulm_malloc(sizeof(ULMTopology_t));
    if (topology == (ULMTopology_t *) NULL) {
	rc = MPI_ERR_OTHER;
	goto ERRHANDLER;
    }
    p = ulm_malloc((nnodes + nedges) * sizeof(int));
    if (p == NULL) {
	rc = MPI_ERR_OTHER;
	goto ERRHANDLER;
    }

    topology->type = MPI_GRAPH;
    topology->graph.nnodes = nnodes;
    topology->graph.nedges = nedges;
    topology->graph.index = (int *) p;
    topology->graph.edges = (int *) p + nnodes;

    for (i = 0; i < nnodes; i++) {
	if (index[i] < 0) {
	    rc = MPI_ERR_ARG;
	    goto ERRHANDLER;
	}
	topology->graph.index[i] = index[i];
    }


    for (i = 0; i < nedges; i++) {
	if (edges[i] < 0 || edges[i] >= nnodes) {
	    rc = MPI_ERR_ARG;
	    goto ERRHANDLER;
	}
	topology->graph.edges[i] = edges[i];
    }

    rc = ulm_set_topology(*comm_new, topology);
    if (rc != ULM_SUCCESS) {
	rc = MPI_ERR_TOPOLOGY;
	goto ERRHANDLER;
    }

ERRHANDLER:
    if (rc != MPI_SUCCESS) {
	if (p != NULL) {
	    ulm_free(p);
	}
	if (topology != NULL) {
	    ulm_free(topology);
	}
	if (*comm_new != MPI_COMM_NULL) {
	    MPI_Comm_free(comm_new);
	}
	_mpi_errhandler(comm_old, rc, __FILE__, __LINE__);
    }

    return rc;
}
