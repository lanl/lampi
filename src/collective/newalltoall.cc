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



#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/mpi.h"
#include "client/ULMClient.h"

extern "C" int ulm_alltoall(void *sendbuf, int sendcount, ULMType_t *sendtype,
			    void *recvbuf, int recvcnt, ULMType_t *recvtype,
			    int comm)
{
    int proc;
    int comm_size, rc;
    ULMRequest_t recvRequest, *sendRequest;
    ULMStatus_t status;
    unsigned char *buf_loc;
    int tag;

    rc = ulm_comm_size(comm, &comm_size);
    if (rc != ULM_SUCCESS) {
	return MPI_ERR_COMM;
    }
    rc = ulm_get_system_tag(comm, 1, &tag);
    if (rc != ULM_SUCCESS) {
	return MPI_ERR_INTERN;
    }

    sendRequest = (ULMRequest_t *)ulm_malloc(sizeof(ULMRequest_t) * comm_size);
    if (!sendRequest) {
	ulm_exit((-1, "ulm_alltoall: unable to allocate send request "
                  "array memory!\n"));
    }

    /* Send to all processes */
    buf_loc = (unsigned char *) sendbuf;
    for (proc = 0; proc < comm_size; proc++) {
	rc = ulm_isend(buf_loc + proc*sendcount * sendtype->extent,
		       sendcount, sendtype, proc, tag,
		       comm, &(sendRequest[proc]), ULM_SEND_STANDARD);
	if (rc != ULM_SUCCESS) break;
    }

    /* Receive from all processes */
    buf_loc = (unsigned char *) recvbuf;
    if (rc == ULM_SUCCESS) {
	for (proc = 0; proc < comm_size; proc++) {
	    rc = ulm_irecv(buf_loc+proc*recvcnt*recvtype->extent,
			   recvcnt, recvtype, proc,
			   tag,  comm,  &recvRequest);
	    if (rc != ULM_SUCCESS) break;

	    rc = ulm_wait(&recvRequest, &status);
	    if (rc != ULM_SUCCESS) break;

	}

	if (rc == ULM_SUCCESS) {
	    for (proc = 0; proc < comm_size; proc++) {
		rc = ulm_wait(&(sendRequest[proc]), &status);
		if (rc != ULM_SUCCESS) break;
	    }
	}
    }

    rc = (rc == ULM_SUCCESS) ? MPI_SUCCESS : _mpi_error(rc);

    if (rc != MPI_SUCCESS) {
        _mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    ulm_free(sendRequest);

    return rc;
}
