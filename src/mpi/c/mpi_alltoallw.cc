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
#include "queue/Communicator.h"
#include "queue/globals.h"

#pragma weak MPI_Alltoallw = PMPI_Alltoallw
int PMPI_Alltoallw(void *s_buf, int *s_counts, int *s_displs,
		   MPI_Datatype *s_mtypes,
		   void *r_buf, int *r_counts, int *r_displs,
		   MPI_Datatype *r_mtypes, MPI_Comm comm)
{
    int rc;
    ULMType_t **s_types;
    ULMType_t **r_types;
    Communicator *communicator;

    if (_mpi.check_args) {
        rc = MPI_SUCCESS;
        if (_mpi.finalized) {
            rc = MPI_ERR_INTERN;
        } else if (s_buf == NULL) {
            rc = MPI_ERR_BUFFER;
        } else if (s_counts == NULL) {
            rc = MPI_ERR_COUNT;
        } else if (s_displs == NULL) {
            rc = MPI_ERR_DISP;
        } else if (s_mtypes == NULL) {
            rc = MPI_ERR_TYPE;
        } else if (r_buf == NULL) {
            rc = MPI_ERR_BUFFER;
        } else if (r_counts == NULL) {
            rc = MPI_ERR_COUNT;
        } else if (r_displs == NULL) {
            rc = MPI_ERR_DISP;
        } else if (r_mtypes == NULL) {
            rc = MPI_ERR_TYPE;
        } else if (ulm_invalid_comm(comm)) {
            rc = MPI_ERR_COMM;
        }
        if (rc != MPI_SUCCESS) {
            goto ERRHANDLER;
        }
    }

    s_types = (ULMType_t **) s_mtypes;
    r_types = (ULMType_t **) r_mtypes;

    communicator = communicators[comm];
    rc = communicator->collective.alltoallw(s_buf, s_counts, s_displs,
                                   (ULMType_t **)s_types, r_buf, r_counts,
                                   r_displs, (ULMType_t **)r_types, comm);
    rc = (rc == ULM_SUCCESS) ? MPI_SUCCESS : _mpi_error(rc);

ERRHANDLER:
    if (rc != MPI_SUCCESS) {
	_mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    return rc;
}
