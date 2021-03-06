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
#include "internal/collective.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Alltoall = PMPI_Alltoall
#endif

int PMPI_Alltoall(void *sendbuf, int sendcount, MPI_Datatype senddatatype,
		  void *recvbuf, int recvcount, MPI_Datatype recvdatatype,
		  MPI_Comm comm)
{
    int rc;
    ULMType_t *sendtype;
    ULMType_t *recvtype;
    ulm_alltoall_t  *alltoall;
    
    if (_mpi.check_args) {
        rc = MPI_SUCCESS;
        if (_mpi.finalized) {
            rc = MPI_ERR_INTERN;
        } else if (senddatatype == MPI_DATATYPE_NULL) {
            rc = MPI_ERR_TYPE;
        } else if (sendcount < 0) {
            rc = MPI_ERR_COUNT;
        } else if (recvdatatype == MPI_DATATYPE_NULL) {
            rc = MPI_ERR_TYPE;
        } else if (recvcount < 0) {
            rc = MPI_ERR_COUNT;
        } else if (ulm_invalid_comm(comm)) {
            rc = MPI_ERR_COMM;
        }
        if (rc != MPI_SUCCESS) {
            goto ERRHANDLER;
        }
    }

    sendtype = (ULMType_t *) senddatatype;
    recvtype = (ULMType_t *) recvdatatype;
    rc = ulm_comm_get_collective(comm, ULM_COLLECTIVE_ALLTOALL, (void **)&alltoall);
    if ( ULM_SUCCESS == rc )
    {
        rc = alltoall(sendbuf, sendcount, sendtype, recvbuf,
                                      recvcount, recvtype, comm);        
    }
    rc = (rc == ULM_SUCCESS) ? MPI_SUCCESS : _mpi_error(rc);

ERRHANDLER:
    if (rc != MPI_SUCCESS) {
	_mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    return rc;
}
