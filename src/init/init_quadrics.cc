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

#ifdef USE_ELAN_COLL /* Start USE_ELAN_COLL, --Weikuan */
void lampi_init_postfork_coll_setup(lampiState_t *s)
{
    /* if daemon - return */

    if ((s->nhosts == 1) || (s->iAmDaemon)) {
        return;
    }

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_coll_setup");
    }

    if (!quadricsHW) // quadrics hardware bcast disabled by user or previous error
      return;

    Broadcaster   * bcaster;
    maddr_vm_t  main_base = quadrics_Glob_Mem_Info->globMainMem;
    sdramaddr_t elan_base = quadrics_Glob_Mem_Info->globElanMem;
    int returnCode = ULM_SUCCESS;

    //! initialize array to broadcasters 

    broadcasters_array_len = getenv("LAMPI_NUM_BCASTERS") ? 
      atoi (getenv("LAMPI_NUM_BCASTERS") ) :
      NR_BROADCASTER;

    broadcasters_array_len = 
      (broadcasters_array_len > NR_BROADCASTER)
      ? broadcasters_array_len : NR_BROADCASTER;

    if ( !(broadcasters_array_len >= 8 && broadcasters_array_len <= 64))
    {
      ulm_exit((-1, "Please set LAMPI_NUM_BCASTERS between 8 and 64.\n"));
    }

    quadrics_broadcasters = ulm_new(Broadcaster *, broadcasters_array_len);
    if (!quadrics_broadcasters) {
        ulm_exit((-1, "Unable to allocate space for broadcasters\n"));
    }

    /* Need to link the broadcaster to the quadrics path,
     * and have the push function also push the broadcast traffic */
    for (int i = 0; i < broadcasters_array_len; i++) {
        quadrics_broadcasters[i] = (Broadcaster*) ulm_new(Broadcaster, 1);
	if (!quadrics_broadcasters[i])
	  ulm_exit((-1, "Unable to allocate space for Broadcaster \n"));
	else
	{
	  quadrics_broadcasters[i]->id = i;
	  quadrics_broadcasters[i]->inuse = 0;
	  returnCode = quadrics_broadcasters[i]->init_bcaster(
	      main_base, elan_base);
	  if ( returnCode != ULM_SUCCESS)
	    ulm_exit((-1, "Unable to allocate resource for Broadcaster \n"));
	}
	main_base = (char *)main_base + 
	  (COMM_BCAST_MEM_SIZE + BCAST_CTRL_SIZE);
	elan_base += (COMM_BCAST_ELAN_SIZE);
    }

    if (usethreads())
        communicatorsLock.lock();

    /* Assign the first broadcaster to the base group */
    Communicator * cp = communicators[ULM_COMM_WORLD];
    bcaster = quadrics_broadcasters[0];

    /* Link back to the communicator */
    bcaster->comm_index = cp->contextID;

    /* Enable the hardware multicast */
    returnCode = bcaster->hardware_coll_init();

    busy_broadcasters[0].cid = ULM_COMM_WORLD;
    busy_broadcasters[0].pid = 0;
    bcaster->inuse = 1;

    if ( returnCode == ULM_SUCCESS)
    {
      if (myproc()==0)         
         ulm_err(("Enabling hw bcast on ULM_COMM_WORLD=%i\n",ULM_COMM_WORLD));
      cp->hw_bcast_enabled = QSNET_COLL ;
      cp->bcaster          = bcaster;
    }


    if (usethreads())
        communicatorsLock.unlock();

    /*return returnCode;*/
}
#endif

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

	/* Add Quadrics path to global pathContainer */

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

    /* read quadricsHW - if quadricsHW use hardware collectives
     */
    if (false ==
        s->client->unpackMessage(&quadricsHW,
                                 (adminMessage::packType) sizeof(int),
                                 1)) {
        ulm_err(("Failed unpacking quadricsHW...\n"));
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


#ifdef ENABLE_CT
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

    if (false ==
        s->client->unpack(&quadricsHW,
                       (adminMessage::packType) sizeof(int), 1)) {
        ulm_err(("Failed unpacking quadricsHW...\n"));
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
