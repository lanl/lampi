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

#define MAX_CONCURRENT_SCATTER_GATHERS	(int)50

/*
 * This version of MPI_Alltoallv() does not assume that it can post
 * an unlimited number of non-blocking sends and receives. Instead,
 * it assumes that it can post MAX_CONCURRENT_SCATTER_GATHERS at a
 * time. The processes start by splitting themselves into disjoint
 * sets of up to MAX_CONCURRENT_SCATTER_GATHERS processes each. Sending
 * proceeds from process n to n+1 (wrapping accordingly), while receiving
 * proceeds from process n to n-1 (and wrapping accordingly at -1 to
 * the maximum group rank). Each process should be able to proceed
 * with minimal interference from other traffic...depending upon
 * the scatter/gather parameters, communication paths, and the phase
 * of the moon...
 */

extern "C" int ulm_alltoallv(void *sendbuf, int *sendcounts, int *senddispls,
			     ULMType_t *sendtype,
			     void *recvbuf, int *recvcounts, int *recvdispls,
			     ULMType_t *recvtype , int comm)
{
    int proc, grp_rank;
    int comm_size, rc, starting_proc;
    int next_send_proc, next_recv_proc;
    int end_sending_proc, end_receiving_proc;
    ULMRequestHandle_t recvRequestArray[MAX_CONCURRENT_SCATTER_GATHERS];
    ULMRequestHandle_t sendRequestArray[MAX_CONCURRENT_SCATTER_GATHERS];
    int sendInUse[MAX_CONCURRENT_SCATTER_GATHERS];
    int recvInUse[MAX_CONCURRENT_SCATTER_GATHERS];
    unsigned char *buf_loc;
    int tag, i;

    rc = ulm_comm_rank(comm, &grp_rank);
    if (rc != ULM_SUCCESS) {
	return MPI_ERR_COMM;
    }

    rc = ulm_comm_size(comm, &comm_size);
    if (rc != ULM_SUCCESS) {
	return MPI_ERR_COMM;
    }

    rc = ulm_get_system_tag(comm, 1, &tag);
    if (rc != ULM_SUCCESS) {
	return MPI_ERR_INTERN;
    }

    /* integer arithmetic - not a no-op! */
    starting_proc = (grp_rank / MAX_CONCURRENT_SCATTER_GATHERS) * MAX_CONCURRENT_SCATTER_GATHERS;
    next_send_proc = next_recv_proc = starting_proc;
    end_sending_proc = end_receiving_proc = starting_proc;

    for (i = 0; i < MAX_CONCURRENT_SCATTER_GATHERS; i++) {
	sendInUse[i] = recvInUse[i] = 0;
    }

    /* Send to first disjoint set of up to MAX_CONCURRENT_SCATTER_GATHERS processes */
    for (i = 0; i < MAX_CONCURRENT_SCATTER_GATHERS; i++)
    {
	proc = (next_send_proc++);
	next_send_proc = (next_send_proc >= comm_size) ? (next_send_proc - comm_size) : next_send_proc;
	if (((i != 0) && (proc == starting_proc)) || (proc < starting_proc)) {
	    next_send_proc = proc;
	    break;
	}

	buf_loc = (unsigned char *)sendbuf + senddispls[proc] * sendtype->extent;

	sendInUse[i] = 1;
	rc = ulm_isend(buf_loc,  sendcounts[proc], sendtype, proc, tag,
		       comm, &(sendRequestArray[i]), ULM_SEND_STANDARD);
	if (rc != ULM_SUCCESS) break;
    }

    /* Receive from first disjoint set of up to MAX_CONCURRENT_SCATTER_GATHERS processes */
    if (rc == ULM_SUCCESS) {
	for (i = 0; i < MAX_CONCURRENT_SCATTER_GATHERS ; i++) {
	    proc = (next_recv_proc++);
	    next_recv_proc = (next_recv_proc >= comm_size) ? (next_recv_proc - comm_size) : next_recv_proc;
	    if (((i != 0) && (proc == starting_proc)) || (proc < starting_proc)) {
		next_recv_proc = proc;
		break;
	    }

	    end_receiving_proc = proc;
	    buf_loc = (unsigned char *)recvbuf + recvdispls[proc] * recvtype->extent;

	    recvInUse[i] = 1;
	    rc = ulm_irecv(buf_loc, recvcounts[proc], recvtype, proc,
			   tag,  comm,  &(recvRequestArray[i]));
	    if (rc != ULM_SUCCESS) break;
	}
	next_recv_proc = starting_proc - 1;
	if (comm_size > MAX_CONCURRENT_SCATTER_GATHERS) {
	    if (next_recv_proc < 0)
		next_recv_proc = comm_size - 1;
	}
    }

    /* now we test for completion of any of our requests, if done send/recv to the next process */
    if ((rc == ULM_SUCCESS) && (comm_size > MAX_CONCURRENT_SCATTER_GATHERS)) {
	int done = 0;
	while (!done) {
	    for (i = 0; i < MAX_CONCURRENT_SCATTER_GATHERS; i++) {
		int completed = 0;
		ULMStatus_t status;

		if (next_send_proc != end_sending_proc) {
		    if (sendInUse[i])
			rc = ulm_test(&(sendRequestArray[i]), &completed, &status);
		    if (completed || !sendInUse[i]) {
			if (rc != ULM_SUCCESS) {
			    done = 1;
			    break;
			}
			/* send to next process */
			proc = (next_send_proc++);
			next_send_proc = (next_send_proc >= comm_size) ? (next_send_proc - comm_size) : next_send_proc;

			buf_loc = (unsigned char *)sendbuf + senddispls[proc] * sendtype->extent;

			sendInUse[i] = 1;
			rc = ulm_isend(buf_loc,  sendcounts[proc], sendtype, proc, tag,
				       comm, &(sendRequestArray[i]), ULM_SEND_STANDARD);
			if (rc != ULM_SUCCESS) {
			    done = 1;
			    break;
			}
		    }
		}

		if (next_recv_proc != end_receiving_proc) {
		    if (recvInUse[i])
			rc = ulm_test(&(recvRequestArray[i]), &completed, &status);
		    if (completed || !recvInUse[i]) {
			if (rc != ULM_SUCCESS) {
			    if ((rc == ULM_ERR_RECV_MORE_THAN_POSTED) || (rc == ULM_ERR_RECV_LESS_THAN_POSTED)) {
				rc = ULM_SUCCESS;
			    }
			    else {
				done = 1;
				break;
			    }
			}
			/* receive from next process */
			proc = (next_recv_proc--);
			next_recv_proc = (next_recv_proc < 0) ? (comm_size - 1) : next_recv_proc;

			buf_loc = (unsigned char *)recvbuf + recvdispls[proc] * recvtype->extent;

			recvInUse[i] = 1;
			rc = ulm_irecv(buf_loc, recvcounts[proc], recvtype, proc,
				       tag,  comm,  &(recvRequestArray[i]));
			if (rc != ULM_SUCCESS) {
			    done = 1;
			    break;
			}
		    }
		}

	    } // end for loop

	    if ((next_send_proc == end_sending_proc) && (next_recv_proc == end_receiving_proc))
		done = 1;

	} // end while loop
    }

    /* now wait for completion of all outstanding requests */
    if (rc == ULM_SUCCESS) {
	ULMStatus_t status;
	int numRequests = (comm_size <= MAX_CONCURRENT_SCATTER_GATHERS) ? comm_size : MAX_CONCURRENT_SCATTER_GATHERS;
	for (proc = 0; proc < numRequests; proc++) {
	    if (recvInUse[proc])
		rc = ulm_wait(&(recvRequestArray[proc]), &status);
	    if ((rc == ULM_ERR_RECV_MORE_THAN_POSTED) || (rc == ULM_ERR_RECV_LESS_THAN_POSTED))
		rc = ULM_SUCCESS;
	    if (rc != ULM_SUCCESS) break;

	    if (sendInUse[proc])
		rc = ulm_wait(&(sendRequestArray[proc]), &status);
	    if (rc != ULM_SUCCESS) break;
	}
    }

    rc = (rc == ULM_SUCCESS) ? MPI_SUCCESS : _mpi_error(rc);

    if (rc != MPI_SUCCESS) {
        _mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    return rc;
}
