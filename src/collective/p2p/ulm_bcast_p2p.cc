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

/*
 * ulm_bcast - point-to-point, logarithmic algorithm
 */
extern "C" int ulm_bcast_p2p(void *buf, int count, ULMType_t *type,
                             int root, int comm)
{
    ULMStatus_t status;
    int mask, nproc, peer, peer_r, rc, self, self_r, tag;

    ulm_dbg(("ulm_bcast_p2p\n"));

    if (count == 0) {
        return ULM_SUCCESS;
    }

    /*
     * Algorithm: logarithmic fan out
     *
     * Process ranks shifted relative to root:
     *   X_r = (X - root + nproc) % nproc
     *   X = (X_r + root) % nproc
     */

    nproc = communicators[comm]->localGroup->groupSize;
    if (nproc == 1) {
        return ULM_SUCCESS;
    }
    tag = communicators[comm]->get_base_tag(1);
    self = communicators[comm]->localGroup->ProcID;
    self_r = (self - root + nproc) % nproc;
    rc = ULM_SUCCESS;
    for (mask = 1; mask < nproc; mask <<= 1) {
        if (self_r >= 2 * mask) {       /* not participating yet */
            continue;
        }
        peer_r = self_r ^ mask;
        if (peer_r >= nproc) {  /* all done */
            break;
        }
        peer = (peer_r + root) % nproc;
        if (self_r < peer_r) {  /* send to peer */
            rc = ulm_send(buf, count, type, peer, tag, comm,
                          ULM_SEND_STANDARD);
            if (rc != ULM_SUCCESS) {
                break;
            }
        } else {                /* receive from peer */
            rc = ulm_recv(buf, count, type, peer, tag, comm, &status);
            if (rc != ULM_SUCCESS) {
                break;
            }
        }
    }

    return rc;
}
