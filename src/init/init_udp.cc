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

#include "init/init.h"
#include "internal/new.h"
#include "internal/options.h"
#include "path/common/pathContainer.h"
#include "path/udp/path.h"
#include "ctnetwork/CTNetwork.h"
#include "internal/Private.h"

void lampi_init_prefork_udp(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_udp");
    }
    if  (s->nhosts == 1)
        return;
    if  (s->udp) {
        if( UDPNetwork::initPreFork() != ULM_SUCCESS) {
            s->error = ERROR_LAMPI_INIT_PREFORK_UDP;
            return;
         }
    }
}


void lampi_init_postfork_udp(lampiState_t * s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_udp");
    }

    if (s->nhosts == 1) {
        return;
    }

    if (s->udp) {

        int pathHandle;
        udpPath *pathAddr;
        int rc;

        /*
         * Finalize UDP setup.  This has to occur ofter the fork so that
         * each process has its own unique ports for UDP.
         */

        UDPNetwork::initPostFork(s->if_count, s->h_addrs);
        if (!s->iAmDaemon) {
            rc = UDPGlobals::UDPNet->initialize(myproc());
            if (rc != ULM_SUCCESS) {
                s->error = ERROR_LAMPI_INIT_POSTFORK_UDP;
                return;
            }
        }

        rc = s->client->allgather(UDPGlobals::UDPNet->portID,
                                  UDPGlobals::UDPNet->procPorts,
                                  UDPGlobals::NPortsPerProc *
                                  sizeof(unsigned short));
        if (rc != ULM_SUCCESS) {
            s->error = ERROR_LAMPI_INIT_POSTFORK_UDP;
            return;
        }
        
        if (!s->iAmDaemon) {
            /*
             * Add UDP path to global pathContainer
             */

            pathAddr = (udpPath *) (pathContainer()->add(&pathHandle));
            if (!pathAddr) {
                s->error = ERROR_LAMPI_INIT_PATH_CONTAINER;
                return;
            }
            new(pathAddr) udpPath;
            pathContainer()->activate(pathHandle);
        }
    }                           /* if (s->udp) */
}
