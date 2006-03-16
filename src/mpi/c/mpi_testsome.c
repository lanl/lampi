/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "internal/mpi.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Testsome = PMPI_Testsome
#endif

int PMPI_Testsome(int incount, MPI_Request *array_of_requests,
		  int *outcount, int *array_of_indices,
		  MPI_Status *array_of_statuses)
{
    ULMRequest_t *req = (ULMRequest_t *) array_of_requests;
    int i, rc, nnull, ninactive;

    if (_mpi.check_args) {
        rc = MPI_SUCCESS;
        if (_mpi.finalized) {
            rc = MPI_ERR_INTERN;
        } else if (array_of_requests == NULL) {
            rc = MPI_ERR_REQUEST;
        } else if (array_of_indices == NULL) {
            rc = MPI_ERR_ARG;
        }
        if (rc != MPI_SUCCESS) {
            _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
            return rc;
        }
    }

    if (array_of_statuses != MPI_STATUSES_IGNORE) {
	memset(array_of_statuses, 0, incount * sizeof(MPI_Status));
    }

    nnull = 0;
    ninactive = 0;
    *outcount = 0;

    for (i = 0; i < incount; i++) {

	ULMStatus_t stat;
	int completed, rc;

        /* skip null requests */
	if (array_of_requests[i] == MPI_REQUEST_NULL) {
	    nnull++;
	    continue;
	}

	/* skip persistent inactive requests */
	if (ulm_request_status(req[i]) == ULM_STATUS_INACTIVE) {
	    ninactive++;
	    continue;
	}

        /* handle proc null requests */
        if (array_of_requests[i] == _mpi.proc_null_request ||
            array_of_requests[i] == _mpi.proc_null_request_persistent) {
            if (array_of_statuses != MPI_STATUSES_IGNORE) {
                array_of_statuses->MPI_ERROR = MPI_SUCCESS;
                array_of_statuses->MPI_SOURCE = MPI_PROC_NULL;
                array_of_statuses->MPI_TAG = MPI_ANY_TAG;
                array_of_statuses->_count = 0;
                array_of_statuses->_persistent = 0;
                array_of_statuses++;
            }
            if (array_of_requests[i] == _mpi.proc_null_request) {
                array_of_requests[i] = MPI_REQUEST_NULL;
            }
            (*outcount) += 1;
            continue;
        }

	completed = 0;
	rc = ulm_test(req + i, &completed, &stat);
	if (rc == ULM_ERR_RECV_LESS_THAN_POSTED) {
	    rc = ULM_SUCCESS;
	}
	if (rc != ULM_SUCCESS) {
            if (array_of_statuses != MPI_STATUSES_IGNORE) {
		array_of_statuses->MPI_ERROR = _mpi_error(stat.error_m);
		array_of_statuses->MPI_SOURCE = stat.peer_m;
		array_of_statuses->MPI_TAG = stat.tag_m;
		array_of_statuses->_count = stat.length_m;
		array_of_statuses->_persistent = stat.persistent_m;
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
            if (array_of_statuses != MPI_STATUSES_IGNORE) {
		array_of_statuses->MPI_ERROR = _mpi_error(stat.error_m);
		array_of_statuses->MPI_SOURCE = stat.peer_m;
		array_of_statuses->MPI_TAG = stat.tag_m;
		array_of_statuses->_count = stat.length_m;
		array_of_statuses->_persistent = stat.persistent_m;
		array_of_statuses++;
	    }
	}
    }				/* end for loop */

    if ((nnull + ninactive) == incount) {
	*outcount = MPI_UNDEFINED;
    }

    return MPI_SUCCESS;
}
