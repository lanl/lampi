/*
 * Copyright 2002-2004. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * LA-MPI: mpirun - main program
 * - parse command line/input file to determine run parameters.
 * - setup accepting socket for clients to connect to and establish
 *   an administrative newtwok.
 * - startup clients
 * - finish setup for server/clients
 * - go into a service loop until time to terminate.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/time.h>
#include <time.h>

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
#include "path/gm/base_state.h"

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

/* bproc_vexecmove_* does not work properly with debuggers if more
 * than one thread is created before the bproc_vexecmove_* call.
 */
#if ENABLE_BPROC
static int use_connect_thread = 0;
#else
static int use_connect_thread = 1;
#endif  /* ENABLE_BPROC */


bool getClientPids(pid_t ** hostarray, int *errorCode)
{
    bool returnValue = true;
    int rank, tag, contacted = 0;
    int alarm_time = (RunParameters.TVDebug) ? -1 : ALARMTIME;

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


static bool exchangeIPAddresses(int *errorCode, adminMessage* server)
{
    int udphosts = 0;
    int tcphosts = 0;

    // we don't do any of this if there is only one host...
    if (RunParameters.NHosts == 1)
        return true;

    for (int i = 0; i < RunParameters.NHosts; i++) {
        for (int j = 0; j < RunParameters.NPathTypes[i]; j++) {
            if (RunParameters.ListPathTypes[i][j] == PATH_UDP) {
                udphosts++;
                break;
            }
            if (RunParameters.ListPathTypes[i][j] == PATH_TCP) {
                tcphosts++;
                break;
            }
        }
    }

    // both TCP and UDP now require that the host addresses be broadcast
    if (udphosts == 0 && tcphosts == 0) {
        return true;
    } else if (udphosts != 0 && udphosts != RunParameters.NHosts) {
        ulm_err(("Error: exchangeIPAddresses %d hosts out of %d using UDP!\n",
                 udphosts, RunParameters.NHosts));
        return false;
    } else if (tcphosts != 0 && tcphosts != RunParameters.NHosts) {
        ulm_err(("Error: exchangeIPAddresses %d hosts out of %d using TCP!\n",
                 tcphosts, RunParameters.NHosts));
        return false;
    }

    size_t size;
    if(RunParameters.NInterfaces == 0)
        size = sizeof(struct sockaddr_in);
    else
        size = RunParameters.NInterfaces * sizeof(struct sockaddr_in);

    int rc = server->allgather(0, 0, size);
    if(rc != ULM_SUCCESS) {
        *errorCode = rc;
        return false;
    }
    return true;
}


bool exchangeUDPPorts(int *errorCode,adminMessage *s)
{
    bool returnValue = true;
    int udphosts = 0, rc;

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


bool exchangeTCPPorts(int *errorCode,adminMessage *s)
{
    bool returnValue = true;
    int tcphosts = 0, rc;

    // we don't do any of this if there is only one host...
    if (RunParameters.NHosts == 1)
        return returnValue;

    for (int i = 0; i < RunParameters.NHosts; i++) {
        for (int j = 0; j < RunParameters.NPathTypes[i]; j++) {
            if (RunParameters.ListPathTypes[i][j] == PATH_TCP) {
                tcphosts++;
                break;
            }
        }
    }
    if( tcphosts != 0 ) {
        rc = s->allgather((void*)NULL,(void *)NULL, sizeof(unsigned short));
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: exchangeTCPPorts() - allgather failed with error %d\n",
                     rc));
            returnValue = false;
            *errorCode = rc;
        }
    }
    return returnValue;
}


bool exchangeIBInfo(int *errorCode,adminMessage *s)
{
    bool returnValue = true;
    int ibhosts = 0, i, j;
    int rc, active[3], tag;
    int alarm_time = (RunParameters.TVDebug) ? -1 : ALARMTIME * 1000;

    // we don't do any of this if there is only one host...
    if (RunParameters.NHosts == 1)
        return returnValue;

    for (i = 0; i < RunParameters.NHosts; i++) {
        for (j = 0; j < RunParameters.NPathTypes[i]; j++) {
            if (RunParameters.ListPathTypes[i][j] == PATH_IB) {
                ibhosts++;
                break;
            }
        }
    }

    if(ibhosts != 0) {

        rc = s->allgather((void *) NULL, (void *) NULL, 2 * sizeof(int));
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: exchangeIBInfo() - allgather of max active info failed (%d)\n", rc));
            returnValue = false;
            *errorCode = rc;
            return returnValue;
        }

        /* receive max active HCA/port info. from host 0 */
        active[0] = active[1] = active[2] = 0;
        s->reset(adminMessage::RECEIVE);
        rc = s->receive(0, &tag, errorCode, alarm_time);
        switch (rc) {
        case adminMessage::OK:
            if (tag != adminMessage::IBMAXACTIVE) {
                ulm_err(("Error: exchangeIBInfo() - did not get IBMAXACTIVE msg., got %d\n", tag));
                returnValue = false;
                *errorCode = rc;
                return returnValue;
            }
            // active[0] = max active HCAs of any process
            // active[1] = max active ports of any process
            // active[2] = sizeof(ib_ud_peer_info_t) -- to avoid bringing in unnecessary headers...
            if (!s->unpack(active, adminMessage::INTEGER, 3, alarm_time)) {
                ulm_err(("Error: exchangeIBInfo() - can't unpack max active info\n"));
                returnValue = false;
                *errorCode = rc;
                return returnValue;
            }
            break;
        default:
            ulm_err(("Error: exchangeIBInfo() - can't receive max active info from host 0\n"));
            returnValue = false;
            *errorCode = rc;
            return returnValue;
        }

        if ((active[0] == 0) || (active[1] == 0)) {
            ulm_warn(("Warning: exchangeIBInfo() - No active InfiniBand HCAs or ports found (HCAs = %d, ports = %d)\n", 
                active[0], active[1]));
            return returnValue;
        }

        rc = s->allgather((void *) NULL, (void *) NULL,
                          active[0] * active[1] * active[2]);
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: exchangeIBInfo() - allgather of all process' active HCA/port info failed (%d)\n", rc));
            returnValue = false;
            *errorCode = rc;
            return returnValue;
        }
    }

    return returnValue;
}

bool exchangeGMInfo(int *errorCode, adminMessage *s)
{
    bool returnValue = true;
    int gmhosts = 0, i, j;
    int rc, maxDevs, tag;
    int alarm_time = (RunParameters.TVDebug) ? -1 : ALARMTIME * 1000;

    // we don't do any of this if there is only one host...
    if (RunParameters.NHosts == 1)
        return returnValue;

    for (i = 0; i < RunParameters.NHosts; i++) {
        for (j = 0; j < RunParameters.NPathTypes[i]; j++) {
            if (RunParameters.ListPathTypes[i][j] == PATH_GM) {
                gmhosts++;
                break;
            }
        }
    }

    if(gmhosts != 0) {

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
    }

    return returnValue;
}

void *server_connect(void *arg) 
{
    int nprocs = *(int *)arg;
    int connectTimeOut = MIN_CONNECT_ALARMTIME + (PERPROC_CONNECT_ALARMTIME * nprocs);
    sigset_t signals;

    /* enable SIGALRM for this thread */
    sigemptyset(&signals);
    sigaddset(&signals, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &signals, (sigset_t *)NULL);

    if (!server->serverConnect(RunParameters.ProcessCount,
                               RunParameters.HostList,
                               RunParameters.NHosts, connectTimeOut)) {
        ulm_err(("Error: Server/client connection failed.\n"));
        if ( use_connect_thread )
        {
            pthread_exit((void *)0);            
        }
    }
    if ( use_connect_thread )
    {
        pthread_exit((void *)1);
    }

    return NULL;
}

int mpirun(int argc, char **argv)
{
    int i, rc, NULMArgs, ErrorReturn, FirstAppArgument;
    int *IndexULMArgs;
    int ReceivingSocket;
    unsigned int AuthData[3];
    int errorCode = 0;
    pthread_t sc_thread;
    void *sc_thread_return;
    sigset_t newsignals, oldsignals;
    
    /* setup process characteristics */
    lampirun_init_proc();
    
    /* print out banner message */
    fprintf(stderr, "*** LA-MPI: mpirun (" PACKAGE_VERSION ")\n");

    /*
     * Read in environment
     */
    lampi_environ_init();

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

    /* set signal mask to block SIGALRM */
    sigemptyset(&newsignals);
    sigaddset(&newsignals, SIGALRM);
    sigprocmask(SIG_BLOCK, &newsignals, &oldsignals);

    /* spawn thread to do adminMessage::serverConnect() processing */
    server->cancelConnect_m = false;
    if ( use_connect_thread )
    {
        if (pthread_create(&sc_thread, (pthread_attr_t *)NULL, server_connect, (void *)&nprocs) != 0) {
            ulm_err(("Error: can't create serverConnect() thread!\n"));
            Abort();
        }        
    }
    
    /*
     * Spawn user app
     */
    rc = SpawnUserApp(AuthData, ReceivingSocket, &ListHostsStarted,
                      &RunParameters, FirstAppArgument, argc, argv);
    if( rc != ULM_SUCCESS ) {
        char **p;

        ulm_err(("Error: Can't spawn application\n"));
        ulm_warn(("Command:"));
        for (p = argv; *p; p++) {
            fprintf(stderr, " %s", *p);
        }
        fprintf(stderr, "\n");
        ulm_warn(("Are PATH and LD_LIBRARY_PATH correct?\n"));

        server->cancelConnect_m = true;
        if ( use_connect_thread )
        {
            pthread_join(sc_thread, (void **)NULL);
        }
        Abort();
    }

    /* 2/4/04 RTA:
     * It appears that creating more than one thread before a bproc call
     * causes the bproc_vexecmove_* to only spawn one remote process.
     * So until bproc is fixed, move the server_connect() call to after
     * spawning user app.
     */
    if ( 0 == use_connect_thread )
    {
        server_connect((void *)&nprocs);        
    }
    
    /* at this stage all remote process have been spawned, but their state
     *   is unknown
     */

    /*
     * Finish setting up administrative connections - time out proportional
     *   to number of processes (not number of hosts, since we don't always 
     *   know the number of hosts)...with minimum connect timeout 
     */
    if ( use_connect_thread )
    {
        /* join with serverConnect() processing thread */
        if (pthread_join(sc_thread, &sc_thread_return) == 0) {
            int return_value = (int)sc_thread_return;
            if (return_value == 0) {
                ulm_err(("Error: serverConnect() thread failed!\n"));
                Abort();
            }
        }
        else {
            ulm_err(("Error: pthread_join() with serverConnect() thread failed!\n"));
            Abort();
        }        
    }
        
    /* set signal mask to unblock SIGALRM */
    sigprocmask(SIG_SETMASK, &oldsignals, (sigset_t *)NULL);

    if (server->nhosts() < 1) {
        ulm_err(("Error: nhosts (%d) less than one!\n", NHostsStarted));
        Abort();
    }

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
    for (i = 0; i < RunParameters.NHosts; i++) {
        ClientSocketFDList[i] = server->clientRank2FD(i);
    }

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

    /* IP address information exchange - postfork */
    if (!exchangeIPAddresses(&ErrorReturn, server)) {
        ulm_err(("Error: While exchanging IP addresses (%d)\n", ErrorReturn));
        Abort();
    }

    /* UDP port information exchange - postfork */
    if (!exchangeUDPPorts(&ErrorReturn, server)) {
        ulm_err(("Error: While exchangind UDP ports (%d)\n", ErrorReturn));
        Abort();
    }

    /* TCP port information exchange - postfork */
    if (!exchangeTCPPorts(&ErrorReturn, server)) {
        ulm_err(("Error: While exchangind TCP ports (%d)\n", ErrorReturn));
        Abort();
    }

    /* GM info information exchange - postfork */
    if (!exchangeGMInfo(&ErrorReturn, server)) {
        ulm_err(("Error: While exchanging GM information (%d)\n",
                 ErrorReturn));
        Abort();
    }

    /* InfiniBand information exchange - postfork */
    if (!exchangeIBInfo(&ErrorReturn, server)) {
        ulm_err(("Error: While exchanging InfiniBand (IB) information (%d)\n",
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
    if (RunParameters.UseRMS) {
        int term_status;
        wait(&term_status);
    } else {
        MPIrunDaemonize(StderrBytesRead, StdoutBytesRead, &RunParameters);
    }

    return EXIT_SUCCESS;
}


