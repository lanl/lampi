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

/*
 * LA-MPI: mpirun - main program
 * - parse command line/input file to determine run parameters.
 * - setup accepting socket for clients to connect to and establish
 *   an administrative newtwok.
 * - startup clients
 * - finish setup for server/clients
 * - go into a service loop until time to terminate.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef __APPLE__
#include <sys/time.h>
#else
#include <time.h>
#endif

#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <assert.h>
#include <setjmp.h>
#include <errno.h>

#define ULM_GLOBAL_DEFINE

#include "init/environ.h"

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/adminMessage.h"
#include "client/SocketGeneric.h"
#include "client/SocketServer.h"
#include "run/JobParams.h"
#include "run/TV.h"
#include "run/Run.h"
#include "run/globals.h"
#include "internal/new.h"
#include "path/udp/UDPNetwork.h"  /* for UDPGlobals::NPortsPerProc */


#include "ctnetwork/CTNetwork.h"
#include "path/gm/base_state.h"

#include "internal/Private.h"
/*
 * Global variables instantiated in this file
 */
ULMRunParams_t RunParameters;   // Job description
int *HostDoneWithSetup = NULL;  // Lists hosts are done with setup
int *ListHostsStarted;          // List of host for which SpawnUserApp ran ok
int HostsAbNormalTerminated = 0;        // Number of hosts that terminated abnormally
int HostsNormalTerminated = 0;  // Number of hosts that terminated normally
int NHostsStarted;              // Number of hosts succesfully spawned
int ULMRunSetupDone = 0;        // When 1, setup is done
int ULMRunSpawnedClients = 0;   //
ssize_t *StderrBytesRead;       // number of bytes read per stderr socket
ssize_t *StdoutBytesRead;       // number of bytes read per stdout socket

static adminMessage *server = NULL;

#ifdef __osf__
bool broadcastQuadricsFlags(int *errorCode)
{
    bool returnValue = true;
    int quadricshosts = 0;

    // we don't do any of this if there is only one host...
    if (RunParameters.NHosts == 1)
        return returnValue;

    for (int i = 0; i < RunParameters.NHosts; i++) {
        for (int j = 0; j < RunParameters.NPathTypes[i]; j++) {
            if (RunParameters.ListPathTypes[i][j] == PATH_QUADRICS) {
                quadricshosts++;
                break;
            }
        }
    }

    if (quadricshosts == 0) {
        return returnValue;
    } else if (quadricshosts != RunParameters.NHosts) {
        ulm_err(("Error: broadcastQuadricsFlags %d hosts out of %d using Quadrics!\n",
                 quadricshosts, RunParameters.NHosts));
        returnValue = false;
        return returnValue;
    }

    server->reset(adminMessage::SEND);
    server->pack(&(RunParameters.quadricsDoAck), (adminMessage::packType)sizeof(bool), 1);
    server->pack(&(RunParameters.quadricsDoChecksum), (adminMessage::packType)sizeof(bool), 1);
    if ( !server->broadcast(adminMessage::QUADRICSFLAGS, errorCode) )
    {
        ulm_err(("Error: Unable to broadcast QUADRICSFLAGS message (error %d)\n", *errorCode));
        Abort();
    }

    return returnValue;
}

#endif

bool getClientPidsMsg(pid_t ** hostarray, int *errorCode)
{
    bool returnValue = true;
    int rank, tag, contacted = 0;
    int alarm_time = (RunParameters.TVDebug) ? -1 : ALARMTIME;

    //ASSERT: the CTClient's channel ID has been sent to the daemons.
    while (contacted < RunParameters.NHosts)
	{
        rank = -1;
        tag = adminMessage::CLIENTPIDS;
        if ( true == server->receiveMessage(&rank, &tag, errorCode,
                                           (alarm_time ==
                                            -1) ? 0 : alarm_time * 1000) )
		{
            if ((tag == adminMessage::CLIENTPIDS) &&
                (server->
                 unpackMessage(hostarray[rank],
                               (adminMessage::packType) sizeof(pid_t),
                               RunParameters.ProcessCount[rank], alarm_time))) {
                contacted++;
                break;
            }
		}
		else
            returnValue = false;

        if (!returnValue) {
            break;
        }
    }

    return returnValue;
}


bool getClientPids(pid_t ** hostarray, int *errorCode)
{
    bool returnValue = true;
    int rank, tag, contacted = 0;
    int alarm_time = (RunParameters.TVDebug) ? -1 : ALARMTIME;

#ifdef USE_CT
    return getClientPidsMsg(hostarray, errorCode);
#endif

    while (contacted < RunParameters.NHosts) {
        int recvd = server->receiveFromAny(&rank, &tag, errorCode,
                                           (alarm_time ==
                                            -1) ? -1 : alarm_time * 1000);
        switch (recvd) {
        case adminMessage::OK:
            if ((tag == adminMessage::CLIENTPIDS) &&
                (server->
                 unpack(hostarray[rank],
                        (adminMessage::packType) sizeof(pid_t),
                        RunParameters.ProcessCount[rank], alarm_time))) {
                contacted++;
                break;
            }
            // otherwise fall through...
        default:
            returnValue = false;
            break;
        }
        if (!returnValue) {
            break;
        }
    }

    return returnValue;
}


bool releaseClients(int *errorCode)
{
    bool returnValue = true;
    int rank, tag, contacted = 0, goahead;

#ifdef USE_CT
    ulm_dbg(("\nmpirun: synching %d members before releasing daemons...\n", RunParameters.NHosts + 1));
	server->synchronize(RunParameters.NHosts + 1);
    ulm_dbg(("\nmpirun: done synching %d members before releasing daemons...\n", RunParameters.NHosts + 1));
	return returnValue;
#endif

    while (contacted < RunParameters.NHosts) {
        int recvd = server->receiveFromAny(&rank, &tag, errorCode,
                                           (RunParameters.TVDebug) ? -1 : ALARMTIME * 1000);
        switch (recvd) {
        case adminMessage::OK:
            if (tag == adminMessage::BARRIER) {
                contacted++;
            } else {
                returnValue = false;
            }
            break;
        case adminMessage::TIMEOUT:
            if (RunParameters.TVDebug == 0) {
                returnValue = false;
            }
            break;
        default:
            returnValue = false;
            break;
        }
        if (!returnValue) {
            break;
        }
    }

    server->reset(adminMessage::SEND);
    goahead = (returnValue) ? 1 : 0;
    server->pack(&goahead, (adminMessage::packType) sizeof(int), 1);
    returnValue = server->broadcast(adminMessage::BARRIER, errorCode);
    returnValue = (returnValue && goahead) ? true : false;

    return returnValue;
}


bool broadcastUDPAddresses(int *errorCode)
{
    bool returnValue = true;
    int udphosts = 0;

    // we don't do any of this if there is only one host...
    if (RunParameters.NHosts == 1)
        return returnValue;

    for (int i = 0; i < RunParameters.NHosts; i++) {
        for (int j = 0; j < RunParameters.NPathTypes[i]; j++) {
            if (RunParameters.ListPathTypes[i][j] == PATH_UDP) {
                udphosts++;
                break;
            }
        }
    }

    if (udphosts == 0) {
        return returnValue;
    } else if (udphosts != RunParameters.NHosts) {
        ulm_err(("Error: broadcastUDPAddresses %d hosts out of %d using UDP!\n",
                 udphosts, RunParameters.NHosts));
        returnValue = false;
        return returnValue;
    }

    if (!server->reset(adminMessage::SEND, RunParameters.NHosts * ULM_MAX_HOSTNAME_LEN)) {
        ulm_err(("Error: broadcastUDPAddresses: unable to allocate %d bytes for send buffer!\n",
                 RunParameters.NHosts * ULM_MAX_HOSTNAME_LEN));
        returnValue = false;
        return returnValue;
    }

    for (int i = 0; i < RunParameters.NHosts; i++) {
        HostName_t tmp;
        returnValue = returnValue
            && server->peerName(i, tmp, ULM_MAX_HOSTNAME_LEN);
        returnValue = returnValue
            && server->pack(tmp, adminMessage::BYTE, ULM_MAX_HOSTNAME_LEN);
    }
    if (returnValue) {
        returnValue =
            server->broadcast(adminMessage::UDPADDRESSES, errorCode);
    }

    return returnValue;
}


bool exchangeUDPPorts(int *errorCode,adminMessage *s)
{
    bool returnValue = true;
    int udphosts = 0, rc;

    // we don't do any of this if there is only one host...
    if (RunParameters.NHosts == 1)
        return returnValue;

#ifdef USE_CT
    return returnValue;
#endif

    for (int i = 0; i < RunParameters.NHosts; i++) {
        for (int j = 0; j < RunParameters.NPathTypes[i]; j++) {
            if (RunParameters.ListPathTypes[i][j] == PATH_UDP) {
                udphosts++;
                break;
            }
        }
    }
    if( udphosts != 0 ) {
        rc = s->allgather((void*)NULL,(void *)NULL,
                          UDPGlobals::NPortsPerProc*sizeof(unsigned short));
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: exchangeUDPPorts() - allgather failed with error %d\n",
                     rc));
            returnValue = false;
            *errorCode = rc;
        }
    }

    return returnValue;
}


bool exchangeGMInfo(int *errorCode,adminMessage *s)
{
    bool returnValue = true;
    int gmhosts = 0, i, j;
    int rc, maxDevs, tag;
    int *nDevsPerProc;
    int alarm_time = (RunParameters.TVDebug) ? -1 : ALARMTIME * 1000;

    // we don't do any of this if there is only one host...
    if (RunParameters.NHosts == 1)
        return returnValue;

#ifdef USE_CT
    return returnValue;
#endif

    for (i = 0; i < RunParameters.NHosts; i++) {
        for (j = 0; j < RunParameters.NPathTypes[i]; j++) {
            if (RunParameters.ListPathTypes[i][j] == PATH_GM) {
                gmhosts++;
                break;
            }
        }
    }
    if(gmhosts != 0) {
        nDevsPerProc = (int *) ulm_malloc(sizeof(int) * s->totalNumberOfProcesses());
        if (!nDevsPerProc) {
            ulm_err(("Error: Out of memory\n"));
            returnValue = false;
            return returnValue;
        }

        rc = s->allgather((void *) NULL, (void *) NULL, sizeof(int));
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: exchangeGMInfo() - first allgather failed (%d)\n", rc));
            returnValue = false;
            *errorCode = rc;
            return returnValue;
        }

        /* receive maxDevs from host 0 */
        maxDevs = 0;
        s->reset(adminMessage::RECEIVE);
        rc = s->receive(0, &tag, errorCode, alarm_time);
        switch (rc) {
        case adminMessage::OK:
            if (tag != adminMessage::GMMAXDEVS) {
                ulm_err(("Error: exchangeGMInfo() - did not get GMMAXDEVS msg., got %d\n", tag));
                returnValue = false;
                *errorCode = rc;
                return returnValue;
            }
            if (!s->unpack(&maxDevs, adminMessage::INTEGER, 1, alarm_time)) {
                ulm_err(("Error: exchangeGMInfo() - can't unpack maxDevs\n"));
                returnValue = false;
                *errorCode = rc;
                return returnValue;
            }
            break;
        default:
            ulm_err(("Error: exchangeGMInfo() - can't receive maxDevs from host 0\n"));
            returnValue = false;
            *errorCode = rc;
            return returnValue;
        }

        if (maxDevs <= 0) {
            ulm_err(("Error: exchangeGMInfo() - No GM devices configured (maxDevs = %d)\n", maxDevs));
            returnValue = false;
            *errorCode = rc;
            return returnValue;
        }

        rc = s->allgather((void *) NULL, (void *) NULL,
                          maxDevs * sizeof(localBaseDevInfo_t));
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: exchangeGMInfo() - second allgather failed (%d)\n", rc));
            returnValue = false;
            *errorCode = rc;
            return returnValue;
        }

        ulm_free(nDevsPerProc);
    }

    return returnValue;
}


int main(int argc, char **argv)
{
    int i, rc, NULMArgs, ErrorReturn, FirstAppArgument;
    int *IndexULMArgs, connectTimeOut;
    int ReceivingSocket;
    unsigned int AuthData[3];
    int errorCode = 0;
    double t;
#ifdef USE_CT
    FILE	*fp;
#endif
    
    /* setup process characteristics */
    lampirun_init_proc();

    /*
     * Read in environment
     */
    lampi_environ_init();


    /*
     * initialize admin network
     */

#ifdef USE_CT
    CTNetworkInit();
#endif

    /*
     * Read in mpirun arguments
     */
    rc = MPIrunProcessInput(argc, argv, &NULMArgs, &IndexULMArgs,
                            &RunParameters, &FirstAppArgument);
    if (rc < 0) {
        ulm_err(("Error: Parsing command line\n"));
        Abort();
    }

    /*
     * Setup authorization string
     */
    ULMInitSocketAuth();
    ulm_GetAuthString(AuthData);

    /* calculate the total number of processes */
    int nprocs = 0;
    for (i = 0; i < RunParameters.NHosts; i++) {
        nprocs += RunParameters.ProcessCount[i];
    }

    /* create a new server admin connection/message object */
    server = new adminMessage;
	RunParameters.server = server;
	
    /* intialize the object as a server */
    if (!server
        || !server->serverInitialize((int *) AuthData, nprocs,
                                     &ReceivingSocket)) {
        ulm_err(("Error: Unable to initialize server socket (auth[0] %d"
                 " auth[1] %d auth[2] %d nprocs %d port %d)\n",
                 AuthData[0], AuthData[1], AuthData[2], nprocs,
                 ReceivingSocket));
        Abort();
    }

    /*
     * Spawn user app
     */
    rc = SpawnUserApp(AuthData, ReceivingSocket, &ListHostsStarted,
                      &RunParameters, FirstAppArgument, argc, argv);
    if( rc != ULM_SUCCESS ) {
        ulm_err(("Error: Can't spawn application (%d)\n", rc));
        Abort();
    }
    
    /* at this stage all remote process have been spawned, but their state
     *   is unknown
     */

    t = second();
    /*
     * Finish setting up administrative connections - time out proportional
     *   to number of hosts
     */
    ulm_dbg(("\nmpirun: collecting daemon info...\n"));
    timing_cur = second();
    sprintf(timing_out[timing_scnt++], "Collecting daemon info (t = 0).\n");
    connectTimeOut = CONNECT_ALARMTIME * RunParameters.NHosts;
    if (!server->serverConnect(RunParameters.ProcessCount,
                               RunParameters.HostList,
                               RunParameters.NHosts, connectTimeOut)) {
        ulm_err(("Error: Server/client connection failed\n"));
        Abort();
    }

    timing_stmp = second();
    sprintf(timing_out[timing_scnt++], "Done collecting daemon info (t = %lf).\n", timing_stmp - timing_cur);
    timing_cur = timing_stmp;

    if (server->nhosts() < 1) {
        ulm_err(("Error: nhosts (%d) less than one!\n", NHostsStarted));
        Abort();
    }
#ifdef USE_CT
	// send msg to link the admin network
    ulm_dbg(("\nmpirun: linking network...\n"));
    if ( !server->linkNetwork() )
	{
		ulm_err(("mpirun_main: error in linking admin network!\n"));
	    Abort();
    }
#endif


    /*
     * mark the remote process as started
     *   At this stage all daemon process have started up, and connected
     *     back to mpirun.
     */
    ULMRunSpawnedClients = 1;

    /*
     * reset host information based on the number of hosts that
     *   were actually started
     */
    fix_RunParameters(&RunParameters, server->nhosts());
    /* temporary fix */
    int *ClientSocketFDList =
        (int *) ulm_malloc(sizeof(int) * RunParameters.NHosts);
#ifdef USE_CT
    for (i = 0; i < RunParameters.NHosts; i++) {
        ClientSocketFDList[i] = 0;
    }
#else
    for (i = 0; i < RunParameters.NHosts; i++) {
        ClientSocketFDList[i] = server->clientRank2FD(i);
    }
#endif
    RunParameters.Networks.TCPAdminstrativeNetwork.SocketsToClients =
        ClientSocketFDList;
    /* end temporary fix */

    /* totalview debugging of client "daemons" */
    MPIrunTVSetUp(&RunParameters, server);

    /*
     * send initial input parameters
     */
    ulm_dbg(("\nmpirun: sending initial input to daemons...\n"));
    rc = SendInitialInputDataToClients(&RunParameters, server);
    if( rc != ULM_SUCCESS ) {
	    ulm_err(("Error: While sending initial input data to clients (%d)\n", rc));
	    Abort();
    }

    /*
     * at this stage we assume that all app processes have been
     *   created on the remote hosts
     * */

    /* totalview debugging of all client processes */
    if (RunParameters.TVDebug && RunParameters.TVDebugApp) {
        /* allocate private memory to hold client process ids */
        pid_t **clientpids = ulm_new(pid_t *, RunParameters.NHosts);
        for (int i = 0; i < RunParameters.NHosts; i++) {
            clientpids[i] = ulm_new(pid_t, RunParameters.ProcessCount[i]);
        }

        if (!getClientPids(clientpids, &errorCode)) {
            ulm_err(("Error: Can't get client process IDs (%d)\n", errorCode));
            Abort();
        }
        MPIrunTVSetUpApp(clientpids);

        /* free memory */
        for (int i = 0; i < RunParameters.NHosts; i++) {
            ulm_delete(clientpids[i]);
        }
        ulm_delete(clientpids);
    }

    /* UDP port information exchange - postfork */
    if (!exchangeUDPPorts(&ErrorReturn, server)) {
        ulm_err(("Error: While exchangind UDP ports (%d)\n", ErrorReturn));
        Abort();
    }

    /* GM info information exchange - postfork */
    if (!exchangeGMInfo(&ErrorReturn, server)) {
        ulm_err(("Error: While echanging GM information (%d)\n",
                 ErrorReturn));
        Abort();
    }

    /* release all processes explicitly with barrier admin. message */
    if (!releaseClients(&ErrorReturn)) {
        ulm_err(("Error: Timed out while waiting to release clients (%d)\n",
                 ErrorReturn));
        Abort();
    }

    /* wait for child prun process to complete */
    t = second() - t;
#ifdef USE_CT
    if ( (fp = fopen("./startup_time.out", "a")) )
    {
        for ( int k = 0; k  < timing_scnt; k++ )
            fprintf(fp, "%s", timing_out[k]);
            
        fprintf(fp, "%lf\n", t);
        fflush(fp);
        fclose(fp);
    }
#endif
    
    if (RunParameters.Networks.UseQSW) {
        int term_status;
        wait(&term_status);
    } else {
        MPIrunDaemonize(StderrBytesRead, StdoutBytesRead, &RunParameters);
    }

    return EXIT_SUCCESS;
}
