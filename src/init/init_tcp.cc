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
#include "path/tcp/tcppath.h"
#include "path/udp/UDPNetwork.h"
#include "ctnetwork/CTNetwork.h"
#include "internal/Private.h"


void lampi_init_prefork_receive_setup_params_tcp(lampiState_t * s)
{
    int errorCode;
    int recvd;
    int tag;
    
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_params_tcp");
    }
    
    if ((s->nhosts == 1) || (!s->tcp)) {
        return;
    }
    
    recvd = s->client->receive(-1, &tag, &errorCode);
    if ((recvd == adminMessage::OK) &&
        (tag == dev_type_params::START_TCP_INPUT)) {
    } else { 
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_TCP;
        return;
    }
    
    if(TCPPath::initSetupParams(s->client) != ULM_SUCCESS) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_TCP;
        return;
    }

    int done = 0;
    while (!done) {
        /* read next tag */
        recvd = s->client->receive(-1, &tag, &errorCode);
        if (recvd != adminMessage::OK) {
            ulm_err(("Did not receive OK when reading next tag for TCP setup. "
                     "recvd = %d, errorcode = %d.\n", recvd, errorCode));
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_TCP;
            return;
        }
        switch (tag) {
        case dev_type_params::END_TCP_INPUT:
            /* done reading input params */
            done = 1;
            break;
        default:
            ulm_err(("Unknown TCP setup tag %d...\n", tag));
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_TCP;
            return;
        }
    }
}


void lampi_init_prefork_tcp(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_tcp");
    }
    if(s->tcp) {
        /*
         * PreFork TCP initilization.
         */

        if (TCPPath::initPreFork() != ULM_SUCCESS) {
            s->error = ERROR_LAMPI_INIT_PREFORK_TCP;
            return;
        }
    }
}


void lampi_init_postfork_tcp(lampiState_t * s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_tcp");
    }

    if (s->nhosts == 1) {
        return;
    }

    if (s->tcp)  {

        /*
         * Create TCP Path
         */

        TCPPath *tcpPath = TCPPath::singleton();
        if (!tcpPath) {
            s->error = ERROR_LAMPI_INIT_PATH_CONTAINER;
            return;
        }

        /*
         * Initialization common to daemon/clients
         */
        int rc = tcpPath->initPostFork();
        if(rc != ULM_SUCCESS) {
            s->error = ERROR_LAMPI_INIT_POSTFORK_TCP;
            return;
        }

        /*
         * Initiliziation specific to clients
         */
        if(!s->iAmDaemon) {
            rc = tcpPath->initClient(s->if_count, s->h_addrs);
            if(rc != ULM_SUCCESS) {
                s->error = ERROR_LAMPI_INIT_POSTFORK_TCP;
                return;
            }
        }

        /*
         * Distribute TCP listen ports. Daemon must participate.
         */
        rc = tcpPath->exchangeListenPorts(s->client);
        if (rc != ULM_SUCCESS) {
            s->error = ERROR_LAMPI_INIT_POSTFORK_TCP;
            return;
        }
        
        if (!s->iAmDaemon) {
            pathContainer()->activate(tcpPath->getHandle());
        }
    }
}
