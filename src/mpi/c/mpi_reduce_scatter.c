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



#include "internal/malloc.h"
#include "internal/mpi.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Reduce_scatter = PMPI_Reduce_scatter
#endif

int PMPI_Reduce_scatter(void *sendbuf, void *recvbuf,
			int *recvcount, MPI_Datatype mtype,
			MPI_Op mop, MPI_Comm comm)
{
    int rc, count, rank, size, i;
    int *displs;
    void *buf;
    ULMType_t *type = mtype;

    if (_mpi.check_args) {
        rc = MPI_SUCCESS;
        if (_mpi.finalized) {
            rc = MPI_ERR_INTERN;
        } else if (sendbuf == NULL) {
            rc = MPI_ERR_BUFFER;
        } else if (recvbuf == NULL) {
            rc = MPI_ERR_BUFFER;
        } else if (*recvcount < 0) {
            rc = MPI_ERR_COUNT;
        } else if (mtype == MPI_DATATYPE_NULL) {
            rc = MPI_ERR_TYPE;
        } else if (mop == MPI_OP_NULL) {
            rc = MPI_ERR_OP;
        } else if (ulm_invalid_comm(comm)) {
            rc = MPI_ERR_COMM;
        }
        if (rc != MPI_SUCCESS) {
            _mpi_errhandler(comm, rc, __FILE__, __LINE__);
            return rc;
        }
    }

    if ((ulm_comm_rank(comm, &rank) != ULM_SUCCESS)
	|| (ulm_comm_size(comm, &size) != ULM_SUCCESS)) {
	rc = MPI_ERR_COMM;
	_mpi_errhandler(comm, rc, __FILE__, __LINE__);
	return rc;
    }

    displs = ulm_malloc(size * sizeof(int));
    if (displs == NULL) {
	rc = MPI_ERR_INTERN;
        _mpi_errhandler(comm, rc, __FILE__, __LINE__);
	return rc;
    }

    count = 0;
    for (i = 0; i < size; i++) {
	displs[i] = count;
	count += recvcount[i];
    }

    buf = ulm_malloc(count * type->extent);
    if (buf == NULL) {
	ulm_free(displs);
	rc = MPI_ERR_INTERN;
        _mpi_errhandler(comm, rc, __FILE__, __LINE__);
	return rc;
    }

    rc = PMPI_Reduce(sendbuf, buf, count, mtype, mop, 0, comm);
    if (rc == MPI_SUCCESS) {
	rc = PMPI_Scatterv(buf, recvcount, displs, mtype,
			   recvbuf, recvcount[rank], mtype, 0, comm);
    }

    if (rc != MPI_SUCCESS) {
        _mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    ulm_free(buf);
    ulm_free(displs);

    return rc;
}
