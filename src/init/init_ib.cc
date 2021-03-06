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

#include "init/init.h"
#include "path/common/pathContainer.h"
#include "path/ib/init.h"
#undef PAGESIZE
#include "path/ib/path.h"


void lampi_init_prefork_receive_setup_params_ib(lampiState_t * s)
{
    int errorCode;
    int recvd;
    int tag;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_params_ib");
    }

    if ((s->nhosts == 1) || (!s->ib)) {
        return;
    }

    recvd = s->client->receive(-1, &tag, &errorCode);
    if ((recvd == adminMessage::OK) &&
        (tag == dev_type_params::START_IB_INPUT)) {
    } else {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_IB;
        return;
    }

    if (false ==
        s->client->unpack(&ib_state.ack,
                          (adminMessage::packType) sizeof(bool), 1)) {
        ulm_err(("Failed unpacking ib_state.ack\n"));
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_IB;
        return;
    }

    if (false ==
        s->client->unpack(&ib_state.checksum,
                          (adminMessage::packType) sizeof(bool), 1)) {
        ulm_err(("Failed unpacking ib_state.checksum\n"));
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_IB;
        return;
    }

    int done = 0;
    while (!done) {
        /* read next tag */
        recvd = s->client->receive(-1, &tag, &errorCode);
        if (recvd != adminMessage::OK) {
            ulm_err(("Did not receive OK when reading next tag for IB setup. "
                     "recvd = %d, errorcode = %d.\n", recvd, errorCode));
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_IB;
            return;
        }
        switch (tag) {
        case dev_type_params::END_IB_INPUT:
            /* done reading input params */
            done = 1;
            break;
        default:
            ulm_err(("Unknown IB setup tag %d...\n", tag));
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_IB;
            return;
        }
    }
}

void lampi_init_postfork_ib(lampiState_t * s)
{
    if ((s->nhosts == 1) || (s->error))
        return;

    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_ib");
    }

    /* return if InfiniBand is not being used */
    if (!s->ib) {
        return;
    }

    /* set up IB communications */
    ibSetup(s);
    if ((s->error) || (!s->ib)) {
        return;
    }

    if (!s->iAmDaemon) {
        int pathHandle;
        ibPath *pathAddr;

        /*
         * Add InfiniBand path to global pathContainer
         */
        pathAddr = (ibPath *) (pathContainer()->add(&pathHandle));
        if (!pathAddr) {
            s->error = ERROR_LAMPI_INIT_POSTFORK_PATHS;
            return;
        }
        new(pathAddr) ibPath;
        pathContainer()->activate(pathHandle);
    }
}
