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

#include <stdlib.h>
#include "internal/log.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

/*
 * ulm_barrier - point to point algorithm
 *
 * \param comm		The communicator
 * \return		ULM error code
 *
 * Performs a barrier on the processes in the communicator using a
 * logarithmic "recursive doubling" approach.  Point to point
 * communications are used between all processes.
 */
extern "C" int ulm_barrier_p2p(int comm)
{
    ULMRequest_t r_req;
    ULMRequest_t s_req;
    ULMStatus_t status;
    int mask;
    int n;
    int nproc2;
    int nproc;
    int peer;
    int r_tmp;
    int rc;
    int s_tmp;
    int self;
    int tag;

    ulm_dbg(("ulm_barrier_p2p\n"));

    /*
     * Algorithm
     *
     * Notation:
     *
     * nproc  - the number of procs
     * nproc2 - the largest 2^n such that 2^n <= nproc
     * self   - our group proc ProcID
     * peer   - our peer's group proc ProcID
     *
     * There are three phases:
     *
     * Setup: procs with self >= nproc2 send their data to self - nproc2
     * where it is accumulated.
     *
     * Main: all procs with self < nproc2 recursively exchange and
     * accumulate data with peer = self ^ 2^n for each n such that 2^n
     * is less than nproc2.
     *
     * Final: procs with self < nproc2 send the result to self + nproc2.
     *
     * If nproc is a power of 2, then only the main phase is necessary.
     */

    r_tmp = s_tmp = 0;
    nproc = communicators[comm]->localGroup->groupSize;
    self = communicators[comm]->localGroup->ProcID;
    tag = communicators[comm]->get_base_tag(1);

    nproc2 = 1;
    n = nproc;
    while (n >>= 1) {
        nproc2 <<= 1;
    }

    /*
     * Setup phase: self >= nproc2 sends to self - nproc2
     */

    if (nproc != nproc2) {
        if (self < nproc2 && (peer = self + nproc2) < nproc) {
            rc = ulm_recv(&r_tmp, sizeof(int), NULL, peer, tag, comm,
                          &status);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
        } else if ((peer = self - nproc2) >= 0) {
            rc = ulm_send(&s_tmp, sizeof(int), NULL, peer, tag, comm,
                          ULM_SEND_STANDARD);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
        }
    }

    /*
     * Main phase: recursive exchange between nproc2 procs
     */

    if (self < nproc2) {
        for (mask = 1; mask < nproc2; mask <<= 1) {
            peer = self ^ mask;
            rc = ulm_irecv(&r_tmp, sizeof(int), NULL,
                           peer, tag, comm, &r_req);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
            rc = ulm_isend(&s_tmp, sizeof(int), NULL,
                           peer, tag, comm, &s_req, ULM_SEND_STANDARD);

            if (rc != ULM_SUCCESS) {
                return rc;
            }
            rc = ulm_wait(&s_req, &status);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
            rc = ulm_wait(&r_req, &status);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
        }
    }

    /*
     * Final phase: self >= nproc2 receives from self - nproc2
     */

    if (nproc != nproc2) {
        if (self < nproc2 && (peer = self + nproc2) < nproc) {
            rc = ulm_send(&s_tmp, sizeof(int), NULL,
                          peer, tag, comm, ULM_SEND_STANDARD);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
        } else if ((peer = self - nproc2) >= 0) {
            rc = ulm_recv(&r_tmp, sizeof(int), NULL, peer, tag, comm,
                          &status);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
        }
    }

    return ULM_SUCCESS;
}
