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

#include "internal/options.h"
#include "internal/log.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

/*
 * ulm_barrier_interhost - barrier between hosts
 *
 * \param comm		The communicator
 * \return		ULM error code
 *
 * Performs a barrier on representative processes on each host in the
 * communicator using a logarithmic simultaneous fan in / fan out
 * algorithm (an allreduce with a noop function).  Point to point
 * communications are used between all representative processes.
 *
 * Processes other than the representative processes return
 * immediately.
 */
extern "C" int ulm_barrier_interhost(int comm)
{
    ULMRequest_t r_req;
    ULMRequest_t s_req;
    ULMStatus_t status;
    Group *group;
    int mask;
    int n;
    int nhost2;
    int nhost;
    int peer;
    int peer_host;
    int r_tmp;
    int rc;
    int s_tmp;
    int self;
    int self_host;
    int tag;

    /*
     * Algorithm
     *
     * Notation:
     *
     * nhost     - the number of hosts
     * nhost2    - the largest 2^n such that 2^n <= nhost
     * self      - our group proc ProcID
     * self_host - our group host rank
     * peer      - our peer's group proc ProcID
     * peer_host - our peer's group host rank
     *
     * There are three phases:
     *
     * Setup: hosts with self >= nhost2 send their data to self - nhost2
     *
     * Main: all hosts with self < nhost2 recursively exchange data
     * with peer = self ^ 2^n for each n such that 2^n is less than
     * nhost2.
     *
     * Final: hosts with self < nhost2 send to self + nhost2.
     *
     * If nhost is a power of 2, then only the main phase is necessary.
     */

    group = communicators[comm]->localGroup;
    nhost = group->numberOfHostsInGroup;
    if (nhost == 1) {
        return ULM_SUCCESS;
    }
    tag = communicators[comm]->get_base_tag(1);
    self = group->ProcID;
    self_host = group->hostIndexInGroup;
    if (self != group->groupHostData[self_host].groupProcIDOnHost[0]) {
        return ULM_SUCCESS;                     /* not participating */
    }
    rc = ULM_SUCCESS;

    nhost2 = 1;
    n = nhost;
    while (n >>= 1) {
	nhost2 <<= 1;
    }

    /*
     * Setup phase: self >= nhost2 sends to self - nhost2
     */

    if (nhost != nhost2) {
	if (self_host < nhost2 && (peer_host = self_host + nhost2) < nhost) {
            peer = group->groupHostData[peer_host].groupProcIDOnHost[0];
	    rc = ulm_recv(&r_tmp, sizeof(int), NULL, peer, tag, comm, &status);
	    if (rc != ULM_SUCCESS) {
		return rc;
	    }
	} else if ((peer_host = self_host - nhost2) >= 0) {
            peer = group->groupHostData[peer_host].groupProcIDOnHost[0];
	    rc = ulm_send(&s_tmp, sizeof(int), NULL, peer, tag, comm,
			  ULM_SEND_SYNCHRONOUS);
	    if (rc != ULM_SUCCESS) {
		return rc;
	    }
	}
    }

    /*
     * Main phase: recursive exchange between nhost2 procs
     */

    if (self_host < nhost2) {
        for (mask = 1; mask < nhost2; mask <<= 1) {
	    peer_host = self_host ^ mask;
            peer = group->groupHostData[peer_host].groupProcIDOnHost[0];
	    rc = ulm_irecv(&r_tmp, sizeof(int), NULL,
			   peer, tag, comm, &r_req);
	    if (rc != ULM_SUCCESS) {
		return rc;
	    }
	    rc = ulm_isend(&s_tmp, sizeof(int), NULL,
			   peer, tag, comm, &s_req, ULM_SEND_SYNCHRONOUS);

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
     * Final phase: self >= nhost2 receives from self - nhost2
     */

    if (nhost != nhost2) {
	if (self_host < nhost2 && (peer_host = self_host + nhost2) < nhost) {
            peer = group->groupHostData[peer_host].groupProcIDOnHost[0];
	    rc = ulm_send(&s_tmp, sizeof(int), NULL,
			  peer, tag, comm, ULM_SEND_SYNCHRONOUS);
	    if (rc != ULM_SUCCESS) {
		return rc;
	    }
	} else if ((peer_host = self_host - nhost2) >= 0) {
            peer = group->groupHostData[peer_host].groupProcIDOnHost[0];
            rc = ulm_recv(&r_tmp, sizeof(int), NULL, peer, tag, comm, &status);
	    if (rc != ULM_SUCCESS) {
		return rc;
	    }
	}
    }

    return ULM_SUCCESS;
}
