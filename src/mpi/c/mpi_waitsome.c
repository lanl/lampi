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



#include <string.h>

#include "internal/mpi.h"

#pragma weak MPI_Waitsome = PMPI_Waitsome
int PMPI_Waitsome(int incount, MPI_Request array_of_requests[],
		  int *outcount, int array_of_indices[],
		  MPI_Status array_of_statuses[] )
{
    ULMRequestHandle_t *req = (ULMRequestHandle_t *) array_of_requests;
    int i, rc;

    if (_mpi.check_args) {
        rc = MPI_SUCCESS;
        if (_mpi.finalized) {
            rc = MPI_ERR_INTERN;
        } else if (array_of_requests == NULL) {
            rc = MPI_ERR_REQUEST;
        } else if (array_of_indices == NULL) {
            rc = MPI_ERR_ARG;
        } else if (outcount == NULL) {
            rc = MPI_ERR_ARG;
        }
        if (rc != MPI_SUCCESS) {
            _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
            return rc;
        }
    }

    if (array_of_statuses) {
	memset(array_of_statuses, 0, incount * sizeof(MPI_Status));
    }

    *outcount = 0;

    while (*outcount == 0) {

	ULMStatus_t stat;
	int ninactive, nnull;

	ninactive = 0;
	nnull = 0;

	for (i = 0; i < incount; i++) {

	    int completed, rc;

	    if (array_of_requests[i] == MPI_REQUEST_NULL) {
		nnull++;
		continue;	/* skip null requests */
	    }

	    /* skip persistent inactive requests */
	    if (ulm_request_status(req[i]) == ULM_STATUS_INACTIVE) {
		ninactive++;
		continue;
	    }

	    completed = 0;
	    rc = ulm_test(req + i, &completed, &stat);
	    if (rc == ULM_ERR_RECV_LESS_THAN_POSTED) {
		rc = ULM_SUCCESS;
	    }
	    if (rc != ULM_SUCCESS) {
		*array_of_indices = i;
		(*outcount) += 1;
		if (array_of_statuses) {
		    array_of_statuses->MPI_ERROR = _mpi_error(stat.error);
		    array_of_statuses->MPI_SOURCE =
			(stat.proc.source ==
			 ULM_ANY_PROC) ? MPI_ANY_SOURCE : (int) stat.proc.
			source;
		    array_of_statuses->MPI_TAG =
			(stat.tag == ULM_ANY_TAG) ? MPI_ANY_TAG : stat.tag;
		    array_of_statuses->_count = stat.matchedSize;
		    array_of_statuses->_persistent = stat.persistent;
		    rc = array_of_statuses->MPI_ERROR;
		} else {
		    rc = _mpi_error(rc);
		}
		_mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
		return rc;
	    }
	    if (completed) {
		*array_of_indices = i;
		(*outcount) += 1;
		if (req[i] == ULM_REQUEST_NULL)
		    array_of_requests[i] = MPI_REQUEST_NULL;
		array_of_indices++;
		if (array_of_statuses) {
		    array_of_statuses->MPI_ERROR = _mpi_error(stat.error);
		    array_of_statuses->MPI_SOURCE =
			(stat.proc.source ==
			 ULM_ANY_PROC) ? MPI_ANY_SOURCE : (int) stat.proc.
			source;
		    array_of_statuses->MPI_TAG =
			(stat.tag == ULM_ANY_TAG) ? MPI_ANY_TAG : stat.tag;
		    array_of_statuses->_count = stat.matchedSize;
		    array_of_statuses->_persistent = stat.persistent;
		    array_of_statuses++;
		}
	    }			/* end if (completed) */
	}
	if ((nnull + ninactive) == incount) {
	    *outcount = MPI_UNDEFINED;
	    break;		/* no active requests to wait for */
	}
    }

    return MPI_SUCCESS;
}
