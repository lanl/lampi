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

#include <stdlib.h>
#include <elan3/elan3.h>
#include "internal/options.h"
#include "internal/log.h"
#include "internal/linkage.h"
#include "internal/collective.h"
#include "internal/types.h"
#include "internal/type_copy.h"
#include "queue/globals.h"
#include "path/quadrics/state.h"

CDECL
int ulm_bcast_quadrics(void *buf, int count, ULMType_t * type,
                       int root, int comm)
{
    int self;
    int nhosts;
    long long tag;
    int rc;
    ULMStatus_t status;
    size_t mcast_buf_sz;
    int vpid;
    unsigned char *mcast_buf;

    ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));
    ulm_get_info(comm, ULM_INFO_NUMBER_OF_HOSTS, &nhosts, sizeof(int));

    if (nhosts == 1) {
        /* if communicator is just on one host, use our shared
         * memory bcast */
        goto DEFAULT;
    }

    /* 
     *make sure procs are contiguous.  Also, when called for
     * the first time, getMcastVpid/getMcastBuf must be called by 
     * everyone in the communicator.
     */
    rc = communicators[comm]->getMcastVpid(0, &vpid);
    if (rc != ULM_SUCCESS || vpid < 0) {
        fprintf(stderr, "comm=%d, proc=%d returned vpid=%d, rc=%d \n",
                comm, self, vpid, rc);
        fflush(stderr);
        goto DEFAULT;
    }

    mcast_buf = (unsigned char *) 
        communicators[comm]->getMcastBuf(0, &mcast_buf_sz);
    if (mcast_buf == 0) {
        fprintf(stderr, "comm=%d, proc=%d couldn't fetch mcast buf \n",
                comm, self);
        fflush(stderr);
        goto DEFAULT;
    }

    /* make sure packed message will fit within the multicast
     * buffer */
    if (count * type->packed_size > mcast_buf_sz) {
        goto DEFAULT;
    }

    tag = communicators[comm]->get_base_tag(1);

    if (self == root) {
        rc = ulm_send(buf, count, type, -1, tag, comm, ULM_SEND_MULTICAST);
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error in multicast send, returned %d \n", rc));
            return rc;
        }
    } else {
        rc = ulm_recv(buf, count, type, root, tag, comm, &status);
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error in multicast recv, returned %d \n", rc));
            return rc;
        }
    }

    return ULM_SUCCESS;

DEFAULT:
    return ulm_bcast(buf, count, type, root, comm);
}
