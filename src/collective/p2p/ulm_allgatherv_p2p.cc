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

extern "C" int ulm_allgatherv_p2p(void *sendbuf, int sendcount,
                                  ULMType_t *sendtype, void *recvbuf,
                                  int *recvcount, int *displs,
                                  ULMType_t *recvtype, int comm)
{
    int rc, proc, nprocs;
    unsigned char *buf;
    extern ulm_gatherv_t ulm_gatherv_p2p;
    extern ulm_bcast_t ulm_bcast_p2p;

    ulm_get_info(comm, ULM_INFO_NUMBER_OF_PROCS, &nprocs, sizeof(int));

    /* First call pt-2-pt allgatherv */
    rc = ulm_gatherv_p2p(sendbuf, sendcount, sendtype, recvbuf, 
                         recvcount, displs, recvtype, 0, comm);
    if (rc != ULM_SUCCESS) {
        return rc;
    }

    /* broadcast to the recievers */
    for (proc = 0; proc < nprocs; proc++) {
        buf = ((unsigned char *) recvbuf) 
            + displs[proc] * recvtype->extent;
        rc = ulm_bcast_p2p(buf, recvcount[proc], recvtype, 0, comm);
        if (rc != ULM_SUCCESS) {
            return rc;
        }
    }

    return ULM_SUCCESS;
}
