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

#include <stdio.h>
#include "ulm/ulm.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/options.h"
#include "internal/types.h"
#include "queue/globals.h"

extern "C" int ulm_gatherv_p2p(void *sendbuf, int sendcount,
                               ULMType_t *sendtype, void *recvbuf,
                               int *recvcounts, int *displs,
                               ULMType_t *recvtype, int root, int comm)
{
    int proc, nproc, self, rc, tag, completed;
    ULMRequest_t sreq, *rreq;
    ULMStatus_t sstat;
    unsigned char *ptr;

    ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));
    ulm_get_info(comm, ULM_INFO_NUMBER_OF_PROCS, &nproc, sizeof(int));
    rc = ulm_get_system_tag(comm, 1, &tag);
    if (rc != ULM_SUCCESS) {
	return MPI_ERR_INTERN;
    }
    
    if (self == root) {
        /* first, send to myself */
        rc = ulm_isend(sendbuf, sendcount, sendtype, root, tag, comm, &sreq,
                       ULM_SEND_STANDARD);
        if (rc != ULM_SUCCESS) {
            return rc;
        }

        /* allocate array of request handles */
        rreq = (ULMRequest_t *) 
            malloc(nproc * sizeof(ULMRequest_t));
        if (rreq == NULL) {
            return ULM_ERROR;
        }

        /* post all receives */
        for (proc = 0; proc < nproc; proc++) {
            ptr = (unsigned char *) recvbuf + displs[proc] * recvtype->extent;
            rc = ulm_irecv(ptr, recvcounts[proc], recvtype,
                           proc, tag, comm, &rreq[proc]);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
        }

        /* wait on all receives */
        completed = 0;
        while (!completed) {
            rc = ulm_testall(rreq, nproc, &completed);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
        }

        /* wait on send to myself */
        rc = ulm_wait(&sreq, &sstat);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
        
        free(rreq); rreq = 0;
    } else {
        rc = ulm_isend(sendbuf, sendcount, sendtype, root, tag, comm, &sreq,
                       ULM_SEND_STANDARD);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
        rc = ulm_wait(&sreq, &sstat);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
    }

    return ULM_SUCCESS;
}
