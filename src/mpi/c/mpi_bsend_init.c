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
#include "internal/buffer.h"
#include "internal/state.h"

#pragma weak MPI_Bsend_init = PMPI_Bsend_init
int PMPI_Bsend_init(void *buf, int count, MPI_Datatype type, int dest,
		    int tag, MPI_Comm comm, MPI_Request *request)
{
    ULMRequest_t req;
    ULMType_t *datatype = type;
    int persistent = 1;
    int rc, size;
    void *sendBuffer;
    ULMBufferRange_t *allocation = NULL;

    if (dest == MPI_PROC_NULL) {
	*request = _mpi.proc_null_request;
	return MPI_SUCCESS;
    }

    if (_mpi.check_args) {
        rc = MPI_SUCCESS;
        if (_mpi.finalized) {
            rc = MPI_ERR_INTERN;
        } else if (count < 0) {
            rc = MPI_ERR_COUNT;
        } else if (type == MPI_DATATYPE_NULL) {
            rc = MPI_ERR_TYPE;
        } else if (tag < 0 || tag > MPI_TAG_UB_VALUE) {
            rc = MPI_ERR_TAG;
        } else if (ulm_invalid_comm(comm)) {
            rc = MPI_ERR_COMM;
        } else if (ulm_invalid_destination(comm, dest)) {
            rc = MPI_ERR_RANK;
        }
        if (rc != MPI_SUCCESS) {
            goto ERRHANDLER;
        }
    }

    /* figure out how much buffering is needed for the message */
    rc = PMPI_Pack_size(count, type, comm, &size);
    if (rc != MPI_SUCCESS) {
        goto ERRHANDLER;
    }

    if (size > 0) {
        /*
         *  figure out if there is enough buffer space for the message.
         */

        /* lock buffer pool */
        ATOMIC_LOCK_THREAD(lampiState.bsendData->lock);

        /* get the allocation */
        allocation = ulm_bsend_alloc(size, 0);

        if (allocation == NULL) {
            /* not enough buffer space - check progress, clean prev. alloc. and try again */
            ATOMIC_UNLOCK_THREAD(lampiState.bsendData->lock);
            ulm_make_progress();
            ATOMIC_LOCK_THREAD(lampiState.bsendData->lock);
            ulm_bsend_clean_alloc(0);
            allocation = ulm_bsend_alloc(size, 1);

            if (allocation == NULL) {
                /* unlock buffer pool */
                ATOMIC_UNLOCK(lampiState.bsendData->lock);
                rc = MPI_ERR_BUFFER;
                _mpi_errhandler(comm, rc, __FILE__, __LINE__);

                /* return error code */
                return rc;
            }
        }

	    sendBuffer = (void *)((unsigned char *)lampiState.bsendData->buffer + 
            allocation->offset);

        /* unlock pool */
        ATOMIC_UNLOCK_THREAD(lampiState.bsendData->lock);
    }
    else {
        sendBuffer = buf;
    }

    rc = ulm_isend_init(sendBuffer, size, MPI_BYTE, dest, tag, comm,
                        &req, ULM_SEND_BUFFERED, persistent);
                        
    if ((size > 0) && (rc != ULM_SUCCESS)) {
        /* clean up bsend buffer allocation */
        ATOMIC_LOCK_THREAD(lampiState.bsendData->lock);
        allocation->freeAtZeroRefCount = 1;
        allocation->refCount = 0;
        ulm_bsend_clean_alloc(0);
        ATOMIC_UNLOCK_THREAD(lampiState.bsendData->lock);
        _mpi_errhandler(comm, rc, __FILE__, __LINE__);
        /* return error code */
        return rc;
    }

    /* store buffering info for PMPI_Start */
    ulm_bsend_info_set(req, buf, size, count, datatype);

    /* store request object address in allocation structure */
    if (allocation != NULL) {
        allocation->request = req;
    }

    *request = (MPI_Request) req;

    rc = (rc == ULM_SUCCESS) ? MPI_SUCCESS : _mpi_error(rc);

ERRHANDLER:
    if (rc != MPI_SUCCESS) {
	_mpi_errhandler(comm, rc, __FILE__, __LINE__);
    }

    return rc;
}
