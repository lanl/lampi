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
 * ulm_barrier - barrier
 *
 * \param comm		The communicator
 * \return		ULM error code
 */
extern "C" int ulm_barrier(int comm)
{
    int rc;
    enum { USE_P2P = 0 };

    extern ulm_barrier_t ulm_barrier_interhost;
    extern ulm_barrier_t ulm_barrier_p2p;

    if (USE_P2P) {

        rc = ulm_barrier_p2p(comm);

    } else {

        Communicator *communicator;
        Group *group;
        int host;
        int nhost;
        int nproc_onhost;

        communicator = communicators[comm];
        group = communicator->localGroup;
        nhost = group->numberOfHostsInGroup;
        host = group->hostIndexInGroup;
        nproc_onhost = group->groupHostData[host].nGroupProcIDOnHost;
        rc = ULM_SUCCESS;

        if (nproc_onhost > 1) {
            communicator->smpBarrier(communicator->barrierData);
        }

        if (nhost > 1) {
            rc = ulm_barrier_interhost(comm);
            if (rc != ULM_SUCCESS) {
                return rc;
            }
            if (nproc_onhost > 1) {
                communicator->smpBarrier(communicator->barrierData);
            }
        }
    }

    return rc;
}
