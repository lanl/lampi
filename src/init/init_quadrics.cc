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
#include "internal/options.h"
#include "path/common/pathContainer.h"
#include "path/quadrics/path.h"
#include "path/quadrics/state.h"


void lampi_init_prefork_quadrics(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_quadrics");
    }

    if (s->quadrics) {
        quadricsInitBeforeFork();
    }
}


void lampi_init_postfork_quadrics(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_quadrics");
    }

    if ((s->nhosts == 1) || (s->iAmDaemon)) {
        return;
    }

    if (s->quadrics) {

        int pathHandle;
        quadricsPath *pathAddr;

	quadricsInitAfterFork();

        /*
         * Add Quadrics path to global pathContainer
         */

        pathAddr = (quadricsPath *) (pathContainer()->add(&pathHandle));
        if (!pathAddr) {
            s->error = ERROR_LAMPI_INIT_POSTFORK_PATHS;
            return;
        }
        new(pathAddr) quadricsPath;
        pathContainer()->activate(pathHandle);
    }
}


void lampi_init_prefork_receive_setup_msg_quadrics(lampiState_t *s)
{
    int errorCode; 
    int rank;
    int recvd;
    int tag;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_params_quadrics");
    }

    if ((s->nhosts == 1) || (!s->quadrics)) {
        return;
    }

    /*  
     *  number of bytes per process for the shared memory descriptor
     *  pool RUNPARAMS exchange read the start of input parameters tag
     */

    rank = 0;
    tag = dev_type_params::START_QUADRICS_INPUT;
    if ( false == s->client->receiveMessage(&rank, &tag, &errorCode) )
    {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
        return;
    }

    /* read quadricsDoAck - if quadricsDoAck use reliable acking protocol, else use local
     * send completion semantics */
    if (false ==
        s->client->unpackMessage(&quadricsDoAck,
                                 (adminMessage::packType) sizeof(int),
                                 1)) {
        ulm_err(("Failed unpacking quadricsDoAck...\n"));
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
        return;
    }

    /* read quadricsDoChecksum - indicates what sort of data checks are done on the
     *   receive side */
    if (false ==
        s->client->unpackMessage(&quadricsDoChecksum,
                                 (adminMessage::packType) sizeof(int),
                                 1)) {
        ulm_err(("Failed unpacking quadrics checksum...\n"));
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
        return;
    }

    int done = 0;
    while (!done) {
        /* read next tag */
        recvd = s->client->nextTag(&tag);
        if (recvd != adminMessage::OK) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS;
            return;
        }
        switch (tag) {
        case dev_type_params::END_QUADRICS_INPUT:
            /* done reading input params */
            done = 1;
            break;
        default:
            ulm_err(("Unknown Quadrics setup tag %d...\n", tag));
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
            return;
        }
    }
}


void lampi_init_prefork_receive_setup_params_quadrics(lampiState_t *s)
{
    int errorCode; 
    int recvd;
    int tag;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_params_quadrics");
    }

    if ((s->nhosts == 1) || (!s->quadrics)) {
        return;
    }

    /*
     *  number of bytes per process for the shared memory descriptor
     *  pool RUNPARAMS exchange read the start of input parameters tag
     */


#ifdef USE_CT
    lampi_init_prefork_receive_setup_msg_quadrics(s);
    return;
#endif

    recvd = s->client->receive(-1, &tag, &errorCode);
    if ((recvd == adminMessage::OK) &&
        (tag == dev_type_params::START_QUADRICS_INPUT)) {
    } else {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
        return;
    }

    /* read quadricsDoAck - if quadricsDoAck use reliable acking protocol, else use local
     * send completion semantics */
    if (false ==
        s->client->unpack(&quadricsDoAck,
                          (adminMessage::packType) sizeof(int), 1)) {
        ulm_err(("Failed unpacking quadricsDoAck...\n"));
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
        return;
    }

    /* read quadricsDoChecksum - indicates what sort of data checks are done on the
     *   receive side */
    if (false ==
        s->client->unpack(&quadricsDoChecksum,
                          (adminMessage::packType) sizeof(int), 1)) {
        ulm_err(("Failed unpacking quadrics checksum...\n"));
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
        return;
    }

    int done = 0;
    while (!done) {
        /* read next tag */
        recvd = s->client->receive(-1, &tag, &errorCode);
        if (recvd != adminMessage::OK) {
            ulm_err(("Did not receive OK when reading next tag for Quadrics setup. "
                     "recvd = %d, errorcode = %d.\n", recvd, errorCode));
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
            return;
        }
        switch (tag) {
        case dev_type_params::END_QUADRICS_INPUT:
            /* done reading input params */
            done = 1;
            break;
        default:
            ulm_err(("Unknown Quadrics setup tag %d...\n", tag));
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_QUADRICS;
            return;
        }
    }
}
