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

#include <stdio.h>
#include <string.h>

#include "ulm/ulm.h"
#include "queue/globals.h"
#include "internal/malloc.h" 

/* Macros ************************************************************ */

/*
 * ! Break out of loop on ULM error
 */

#define BREAK_ON_ERR(RC)	if ((RC) != ULM_SUCCESS) goto CLEANUP;
#define REDUCE_SMALL_BUFSIZE	2048

/* Functions ******************************************************** */
/*!
 * A scan, or prefix reduction, operation
 * \param sendbuf	Array of data objects to scan
 * \param recvbuf	Array of scanned data objects
 * \param count		Number of data objects
 * \param type		Data type (defaults to byte)
 * \param op		Operation structure
 * \param root		The group ProcID of the root process
 * \param sense		The direction >= 0 means forward, < 0 means back
 * \param comm		The communicator ID
 * \return		ULM error code
 *
 *  Description
 *
 *  A scan, or prefix reduction, is a mapping between n-tuples defined
 *  by
 *
 *    (a[0], a[1], ..., a[n-1])  |--> (b[0], b[1], ..., b[n-1])
 *
 *  where
 *
 *    b[0] = a[0]
 *    b[i] = f(a[i], b[i - 1])         0 < i < n
 *
 *  and f(x, y) is an associative binary operation (for example,
 *  addition).
 *
 *  The scan operation arises in many algorithms which require an
 *  ordering or sorting phase.
 *
 *  More generally, we refer to the above as a forward scan with root
 *  0, while a forward scan with root m (0 < m < n) is defined by
 *
 *    b[m] = a[m]
 *    b[0] = f(a[0], b[n - 1])
 *    b[i] = f(a[i], b[i - 1])         i != 0
 *
 *  Similarly, a reverse scan with root m (0 < m < n) is defined by
 *  reflexion as
 *
 *    b[M] = a[M]
 *    b[n - 1] = f(a[n - 1], b[0])
 *    b[i] = f(a[i], b[i + 1])         i != n - 1
 *
 *  where M = m - 1, m > 0
 *          = n - 1, m = 0
 *
 *  _ulm_scan() performs any of these scan operations on n-tuples of
 *  objects distributed across a ULM group, where n is the number of
 *  processes in the group.  Each object is itself an array, so that
 *  the scan can be applied to many n-tuples in parallel.
 *
 *
 *  Algorithm
 *
 *  The scan is performed recursively, where proc n communicates with
 *  n +/- 2^0, then with n +/- 2^1, etc.
 *
 *  This implementation  uses a simple one phase  algorithm, taking no account
 *  of on-host communication possibilities.  This is not only simple, but
 *  conforms with the MPI specification which recommends that the function
 *  should produce reproducible results independent of the mapping of processes
 *  onto the system.
 *
 *
 */
int _ulm_scan(const void *sendbuf,
	      void *recvbuf,
	      int count,
	      ULMType_t *type,
	      ULMOp_t *op,
	      int root,
	      int sense,
	      int comm)
{
    int nproc;
    int shift;
    int tag;
    size_t bufsize;
    void *buf;
    unsigned char small_buf[REDUCE_SMALL_BUFSIZE];
    int self;
    int self_r;			/* ProcID of self relative to the scan */
    int rc;
    ULMFunc_t *func;
    void *arg;

    /*
     * Fast return for trivial data
     */

    if (count == 0) {
	return (ULM_SUCCESS);
    }

    /*
     * Type, function and operation set-up
     */

    /*
     * non-null type: send/recv uses datatype count
     * null type: send/recv uses byte count
     */
    if (type) {
	bufsize = count * type->extent;
    } else {
	bufsize = count;
    }

    /*
     * Pre-defined operations have a vector of function
     * pointers corresponding to the basic types.
     *
     * User-defined operations have a vector of length 1.
     */
    if (op->isbasic) {
	if (type == NULL || type->isbasic == 0) {
	    ulm_err(("Error: ulm_reduce: basic operation, non-basic datatype\n"));
	    return ULM_ERROR;
	}
	func = op->func[type->op_index];
    } else {
	func = op->func[0];
    }

    /*
     * For fortran defined functions, pass a pointer to the fortran type
     * handle the function argument, else pass a pointer to the type
     * struct.
     */
    if (op->fortran) {
	arg = (void *) &(type->fhandle);
    } else {
	arg = (void *) &type;
    }

    /*
     * Temporary buffer for receiving incremental data
     */
    if (bufsize > REDUCE_SMALL_BUFSIZE) {
	buf = (void *) ulm_malloc(bufsize);
	if (buf == NULL) {
	    return (ULM_ERR_OUT_OF_RESOURCE);
	}
    } else {
	buf = (void *) small_buf;
    }

    nproc = communicators[comm]->remoteGroup->groupSize;
    self = communicators[comm]->localGroup->ProcID;
    tag = communicators[comm]->get_base_tag(1);

    self_r = (self - root + nproc) % nproc;

    memmove(recvbuf, sendbuf, bufsize);

    for (shift = 1; shift < nproc; shift <<= 1) {

	ULMRequest_t req;
	ULMStatus_t status;
	int send_peer;
	int recv_peer;
	int send_peer_r;
	int recv_peer_r;

	rc = ULM_ERROR;

	if (sense >= 0) {
	    send_peer_r = self_r + shift;
	    recv_peer_r = self_r - shift;
	} else {
	    send_peer_r = self_r - shift;
	    recv_peer_r = self_r + shift;
	}

	if (send_peer_r >= 0 && send_peer_r < nproc) {

	    send_peer = (send_peer_r + nproc - root) % nproc;

	    BREAK_ON_ERR(ulm_isend(recvbuf, count, type, send_peer,
				   tag, comm, &req, ULM_SEND_STANDARD));
	    BREAK_ON_ERR(ulm_wait(&req, &status));

	}
	if (recv_peer_r >= 0 && recv_peer_r < nproc) {

	    recv_peer = (recv_peer_r + nproc - root) % nproc;

	    BREAK_ON_ERR(ulm_irecv(buf, count, type, recv_peer,
				   tag, comm, &req));
	    BREAK_ON_ERR(ulm_wait(&req, &status));

	    func(buf, recvbuf, &count, arg);
	}

	rc = ULM_SUCCESS;
    }

    rc = ULM_SUCCESS;

    /*
     * Free resources and exit
     */

CLEANUP:
    if (bufsize > REDUCE_SMALL_BUFSIZE && buf) {
	ulm_free(buf);
    }
    return (rc);
}

/* Wrapper for mpi-compliant scan */
extern "C"
int ulm_scan(const void *sendbuf,
	     void *recvbuf,
	     int count,
	     ULMType_t *type,
	     ULMOp_t *op,
	     int comm)
{
    return _ulm_scan(sendbuf, recvbuf, count, type, op, 0, 1, comm);
}
