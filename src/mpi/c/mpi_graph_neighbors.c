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

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Graph_neighbors = PMPI_Graph_neighbors
#endif

int PMPI_Graph_neighbors(MPI_Comm comm, int rank, int maxneighbors,
			 int *neighbors)
{
    ULMTopology_t *topology;
    int i;
    int j;
    int rc = MPI_SUCCESS;

    if (rank < 0 || maxneighbors < 0 || neighbors == NULL) {
	rc = MPI_ERR_ARG;
	goto ERRHANDLER;
    }

    rc = ulm_get_topology(comm, &topology);
    if (rc != ULM_SUCCESS) {
	rc = MPI_ERR_COMM;
	goto ERRHANDLER;
    }

    if ((topology == NULL) || (topology->type != MPI_GRAPH)) {
	rc = MPI_ERR_TOPOLOGY;
	goto ERRHANDLER;
    }

    if (rank >= topology->graph.nnodes) {
	rc = MPI_ERR_ARG;
	goto ERRHANDLER;
    }

    if (rank == 0) {
	i = 0;
    } else {
	i = topology->graph.index[rank - 1];
    }
    j = 0;
    while (i < topology->graph.index[rank]) {
	if (j >= maxneighbors) {
	    rc = MPI_ERR_ARG;
	    goto ERRHANDLER;
	}
	neighbors[j++] = topology->graph.edges[i++];
    }

ERRHANDLER:
    if (rc != MPI_SUCCESS) {
	_mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    return rc;
}
