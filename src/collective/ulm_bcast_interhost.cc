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
#include "internal/options.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

/*!
 * ulm_bcast - interhost, logarithmic algorithm
 *
 * \param comm          Communicator
 * \param buf           Data buffer
 * \param count         Number of items to broadcast
 * \param type          Data type of items
 * \param root          Source process ID
 * \return              ULM return code
 *
 * Broadcast a buffer to representative processes on each host in a
 * communicator. The root process is the representative process for
 * its host.
 *
 * Processes other than the representative processes return
 * immediately.
 */
int ulm_bcast_interhost(void *buf, size_t count, ULMType_t * type,
                        int root, int comm)
{
    ULMStatus_t status;
    Group *group;
    int mask, nhost, peer, peer_host, peer_host_r, rc;
    int self, self_host_r, tag;
    int comm_root;
    int my_host_index, root_host_index;
    int hi;

    if (count == 0) {
        return ULM_SUCCESS;
    }

    /*
     * Algorithm: logarithmic fan out over hosts
     *
     * A representative process is used on each host, namely "root" on
     * the host with that process, and the lowest ranked process on
     * every other host.
     *
     * Host ranks shifted relative to root host:
     *   X_r = (X - root_host + nhost) % nhost
     *   X = (X_r + root_host) % nhost
     */
    rc = ulm_get_info(comm, ULM_INFO_NUMBER_OF_HOSTS, &nhost, sizeof(int));

    rc = ulm_get_info(comm, ULM_INFO_PROCID, &self, sizeof(int));

    if (nhost == 1) {
        return ULM_SUCCESS;
    }

    group = communicators[comm]->localGroup;
    hi = group->hostIndexInGroup;
    comm_root = group->groupHostData[hi].groupProcIDOnHost[0];
    root_host_index =
        group->groupHostDataLookup[group->mapGroupProcIDToHostID[root]];
    my_host_index = group->hostIndexInGroup;
    tag = communicators[comm]->get_base_tag(1);

    if (group->mapGroupProcIDToHostID[root] ==
        group->mapGroupProcIDToHostID[self]) {
        comm_root = root;
    }
    if (self != comm_root) {
        return ULM_SUCCESS;
    }

    self_host_r = (my_host_index - root_host_index + nhost) % nhost;
    rc = ULM_SUCCESS;
    for (mask = 1; mask < nhost; mask <<= 1) {
        if (self_host_r >= 2 * mask) {  /* not participating yet */
            continue;
        }
        peer_host_r = self_host_r ^ mask;
        if (peer_host_r >= nhost) {     /* all done */
            break;
        }
        peer_host = (peer_host_r + root_host_index) % nhost;
        if (peer_host == root_host_index) {
            peer = root;
        } else {
            peer = group->groupHostData[peer_host].groupProcIDOnHost[0];
        }
        if (self_host_r < peer_host_r) {        /* send to peer */
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

            if (0) {            /* debug */
                ulm_warn(("comm %d tag %d peer %d\n", comm, tag, peer));
            }
        }
    }

    return rc;
}
