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
#include <errno.h>

#define ULM_GLOBAL_DEFINE
#include "init/environ.h"

#include "internal/profiler.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/types.h"
#include "client/adminMessage.h"
#include "client/SocketGeneric.h"
#include "path/udp/UDPNetwork.h"        /* for UDPGlobals::NPortsPerProc */
#include "path/gm/base_state.h"
#include "run/Run.h"
#include "run/TV.h"

/*
 * bproc_vexecmove_* does not work properly with debuggers if more
 * than one thread is created before the bproc_vexecmove_* call.
 */
#if ENABLE_BPROC
static int use_connect_thread = 0;
#else
static int use_connect_thread = 1;
#endif                          /* ENABLE_BPROC */


/*
 * Get authorization data for admin communications used
 */
static void getAuthData(unsigned int *auth)
{
    auth[0] = getpid();
    auth[1] = getuid();
    auth[2] = time(NULL);
}


/*
 * Collect PIDs of application processes
 *
 * TODO:
 * - collect daemon pid (as last element in packed array)
 * - allocate space inside this function 
 */
static bool getClientPids(pid_t **hostarray, int *errorCode)
{
    adminMessage *s = RunParams.server;
    bool returnValue = true;
    int rank, tag, contacted = 0;
    int alarm_time = (RunParams.TVDebug) ? -1 : ALARMTIME;

    while (contacted < RunParams.NHosts) {
        int recvd = s->receiveFromAny(&rank, &tag, errorCode,
                                      (alarm_time ==
                                       -1) ? -1 : alarm_time * 1000);
        switch (recvd) {
        case adminMessage::OK:
            if ((tag == adminMessage::CLIENTPIDS) &&
                (s->unpack(hostarray[rank],
                           (adminMessage::packType) sizeof(pid_t),
                           RunParams.ProcessCount[rank], alarm_time))) {
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

    if (RunParams.Verbose) {
        for (int h = 0; h < RunParams.NHosts; h++) {
            fprintf(stderr, "LA-MPI: *** Host %d (%s) has PIDs",
                    h, RunParams.HostList[h]);
            for (int p = 0; p < RunParams.ProcessCount[h]; p++) {
                fprintf(stderr, " %d", hostarray[h][p]);
            }
            fprintf(stderr, "\n");
        }
    }

    /* release clients */

    if (returnValue) {
        int goahead = (returnValue) ? 1 : 0;

        s->reset(adminMessage::SEND);
        s->pack(&goahead, (adminMessage::packType) sizeof(int), 1);
        returnValue = s->broadcast(adminMessage::BARRIER, errorCode);
        returnValue = (returnValue && goahead) ? true : false;
    }

    return returnValue;
}


static bool releaseClients(int *errorCode)
{
    adminMessage *s = RunParams.server;
    bool returnValue = true;
    int rank, tag, contacted = 0, goahead;

    while (contacted < RunParams.NHosts) {
        int recvd = s->receiveFromAny(&rank, &tag, errorCode,
                                      (RunParams.
                                       TVDebug) ? -1 : ALARMTIME * 1000);
        switch (recvd) {
        case adminMessage::OK:
            if (tag == adminMessage::BARRIER) {
                contacted++;
            } else {
                returnValue = false;
            }
            break;
        case adminMessage::TIMEOUT:
            if (RunParams.TVDebug == 0) {
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

    s->reset(adminMessage::SEND);
    goahead = (returnValue) ? 1 : 0;
    s->pack(&goahead, (adminMessage::packType) sizeof(int), 1);
    returnValue = s->broadcast(adminMessage::BARRIER, errorCode);
    returnValue = (returnValue && goahead) ? true : false;

    return returnValue;
}


static bool exchangeIPAddresses(int *errorCode)
{
    adminMessage *s = RunParams.server;
    int udphosts = 0;
    int tcphosts = 0;

    // we don't do any of this if there is only one host...
    if (RunParams.NHosts == 1)
        return true;

    for (int i = 0; i < RunParams.NHosts; i++) {
        for (int j = 0; j < RunParams.NPathTypes[i]; j++) {
            if (RunParams.ListPathTypes[i][j] == PATH_UDP) {
                udphosts++;
                break;
            }
            if (RunParams.ListPathTypes[i][j] == PATH_TCP) {
                tcphosts++;
                break;
            }
        }
    }

    // both TCP and UDP now require that the host addresses be broadcast
    if (udphosts == 0 && tcphosts == 0) {
        return true;
    } else if (udphosts != 0 && udphosts != RunParams.NHosts) {
        ulm_err(("Error: exchangeIPAddresses %d hosts out of %d using UDP!\n", udphosts, RunParams.NHosts));
        return false;
    } else if (tcphosts != 0 && tcphosts != RunParams.NHosts) {
        ulm_err(("Error: exchangeIPAddresses %d hosts out of %d using TCP!\n", tcphosts, RunParams.NHosts));
        return false;
    }

    size_t size;
    if (RunParams.NInterfaces == 0)
        size = sizeof(struct sockaddr_in);
    else
        size = RunParams.NInterfaces * sizeof(struct sockaddr_in);

    int rc = s->allgather(0, 0, size);
    if (rc != ULM_SUCCESS) {
        *errorCode = rc;
        return false;
    }
    return true;
}


static bool exchangeUDPPorts(int *errorCode)
{
    adminMessage *s = RunParams.server;
    bool returnValue = true;
    int udphosts = 0, rc;

    // we don't do any of this if there is only one host...
    if (RunParams.NHosts == 1)
        return returnValue;

    for (int i = 0; i < RunParams.NHosts; i++) {
        for (int j = 0; j < RunParams.NPathTypes[i]; j++) {
            if (RunParams.ListPathTypes[i][j] == PATH_UDP) {
                udphosts++;
                break;
            }
        }
    }
    if (udphosts != 0) {
        rc = s->allgather((void *) NULL, (void *) NULL,
                          UDPGlobals::NPortsPerProc *
                          sizeof(unsigned short));
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: exchangeUDPPorts() - allgather failed with error %d\n", rc));
            returnValue = false;
            *errorCode = rc;
        }
    }

    return returnValue;
}


static bool exchangeTCPPorts(int *errorCode)
{
    adminMessage *s = RunParams.server;
    bool returnValue = true;
    int tcphosts = 0, rc;

    // we don't do any of this if there is only one host...
    if (RunParams.NHosts == 1)
        return returnValue;

    for (int i = 0; i < RunParams.NHosts; i++) {
        for (int j = 0; j < RunParams.NPathTypes[i]; j++) {
            if (RunParams.ListPathTypes[i][j] == PATH_TCP) {
                tcphosts++;
                break;
            }
        }
    }
    if (tcphosts != 0) {
        rc = s->allgather((void *) NULL, (void *) NULL,
                          sizeof(unsigned short));
        if (rc != ULM_SUCCESS) {
            ulm_err(("Error: exchangeTCPPorts() - allgather failed with error %d\n", rc));
            returnValue = false;
            *errorCode = rc;
        }
    }
    return returnValue;
}


static bool exchangeIBInfo(int *errorCode)
{
    adminMessage *s = RunParams.server;
    bool returnValue = true;
    int ibhosts = 0, i, j;
    int rc, active[3], tag;
    int alarm_time = (RunParams.TVDebug) ? -1 : ALARMTIME * 1000;

    // we don't do any of this if there is only one host...
    if (RunParams.NHosts == 1)
        return returnValue;

    for (i = 0; i < RunParams.NHosts; i++) {
        for (j = 0; j < RunParams.NPathTypes[i]; j++) {
            if (RunParams.ListPathTypes[i][j] == PATH_IB) {
                ibhosts++;
                break;
            }
        }
    }

    if (ibhosts != 0) {

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
            ulm_warn(("Warning: exchangeIBInfo() - No active InfiniBand HCAs or ports found (HCAs = %d, ports = %d)\n", active[0], active[1]));
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

static bool exchangeGMInfo(int *errorCode)
{
    adminMessage *s = RunParams.server;
    bool returnValue = true;
    int gmhosts = 0, i, j;
    int rc, maxDevs, tag;
    int alarm_time = (RunParams.TVDebug) ? -1 : ALARMTIME * 1000;

    // we don't do any of this if there is only one host...
    if (RunParams.NHosts == 1)
        return returnValue;

    for (i = 0; i < RunParams.NHosts; i++) {
        for (j = 0; j < RunParams.NPathTypes[i]; j++) {
            if (RunParams.ListPathTypes[i][j] == PATH_GM) {
                gmhosts++;
                break;
            }
        }
    }

    if (gmhosts != 0) {

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


static void getSocketsToClients(void)
{
    adminMessage *s = RunParams.server;
    int *fds = ulm_new(int, RunParams.NHosts);

    for (int i = 0; i < RunParams.NHosts; i++) {
        fds[i] = s->clientRank2FD(i);
    }

    RunParams.Networks.TCPAdminstrativeNetwork.SocketsToClients = fds;
}


static void *connectThread(void *arg)
{
    adminMessage *s = RunParams.server;
    int nprocs = *(int *) arg;
    int connectTimeOut =
        MIN_CONNECT_ALARMTIME + (PERPROC_CONNECT_ALARMTIME * nprocs);
    int rc = 0;
    sigset_t signals;

    /* enable SIGALRM for this thread */
    sigemptyset(&signals);
    sigaddset(&signals, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &signals, (sigset_t *) NULL);

    if (!s->serverConnect(RunParams.ProcessCount,
                          RunParams.HostList,
                          RunParams.NHosts, connectTimeOut)) {
        rc = -1;
        if (use_connect_thread) {
            pthread_exit((void *) rc);
        }

    }
    if (use_connect_thread) {
        pthread_exit((void *) rc);
    }

    return (void *) rc;
}


int mpirun(int argc, char **argv)
{
    int FirstAppArg;
    int *ListHostsStarted;
    int ReceivingSocket;
    int nprocs;
    int rc;
    pthread_t sc_thread;
    sigset_t newsignals;
    sigset_t oldsignals;
    unsigned int AuthData[3];

    /* setup process characteristics */
    InitProc();

    /* read in environment */
    lampi_environ_init();

    RunParams.isatty = isatty(fileno(stdin));

    /* read in mpirun arguments */
    rc = ProcessInput(argc, argv, &FirstAppArg);
    if (rc < 0) {
        ulm_err(("Error: Parsing command line\n"));
        Abort();
    }

    /* print banner message */
    if (RunParams.Quiet == 0) {
        if (ENABLE_DEBUG) {
            fprintf(stderr,
                    "LA-MPI: *** mpirun (" PACKAGE_VERSION "-debug)\n");
        } else {
            fprintf(stderr, "LA-MPI: *** mpirun (" PACKAGE_VERSION ")\n");
        }
    }

    /* get authorization data for admin communication */
    getAuthData(AuthData);

    /* calculate the total number of processes */
    nprocs = 0;
    for (int i = 0; i < RunParams.NHosts; i++) {
        nprocs += RunParams.ProcessCount[i];
    }

    /* create a new server admin connection/message object */
    RunParams.server = new adminMessage;

    /* intialize the object as a server */
    if (!RunParams.server ||
        !RunParams.server->serverInitialize((int *) AuthData, nprocs,
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
    RunParams.server->cancelConnect_m = false;
    if (use_connect_thread) {
        if (pthread_create
            (&sc_thread, (pthread_attr_t *) NULL, connectThread,
             (void *) &nprocs) != 0) {
            ulm_err(("Error: can't create serverConnect() thread!\n"));
            Abort();
        }
    }

    /* spawn user application */
    rc = Spawn(AuthData, ReceivingSocket, &ListHostsStarted,
               argc - FirstAppArg, argv + FirstAppArg);
    if (rc != ULM_SUCCESS) {
        char **p;

        ulm_err(("Error: Can't spawn application\n"));
        ulm_warn(("Command:"));
        for (p = argv; *p; p++) {
            fprintf(stderr, " %s", *p);
        }
        fprintf(stderr, "\n");
        ulm_warn(("Are PATH and LD_LIBRARY_PATH correct?\n"));

        RunParams.server->cancelConnect_m = true;
        if (use_connect_thread) {
            pthread_join(sc_thread, (void **) NULL);
        }
        Abort();
    }

    /* 2/4/04 RTA:
     * It appears that creating more than one thread before a bproc call
     * causes the bproc_vexecmove_* to only spawn one remote process.
     * So until bproc is fixed, move the connectThread() call to after
     * spawning user app.
     */
    if (0 == use_connect_thread) {
        if (connectThread((void *) &nprocs) == (void *) -1) {
            Abort();
        }
    }

    /*
     * at this stage all remote process have been spawned, but their
     * state is unknown
     */

    /*
     * Finish setting up administrative connections - time out
     * proportional to number of processes (not number of hosts, since
     * we don't always know the number of hosts)...with minimum
     * connect timeout
     */
    if (use_connect_thread) {
        /* join with serverConnect() processing thread */
        void *thread_return;
        if (pthread_join(sc_thread, &thread_return) == 0) {
            if (thread_return == (void *) -1) {
                Abort();
            }
        } else {
            ulm_err(("Error: pthread_join() with serverConnect() "
                     "thread failed!\n"));
            Abort();
        }
    }

    /* set signal mask to unblock SIGALRM */
    sigprocmask(SIG_SETMASK, &oldsignals, (sigset_t *) NULL);

    if (RunParams.server->nhosts() < 1) {
        ulm_err(("Error: nhosts (%d) less than one!\n",
                 RunParams.server->nhosts()));
        Abort();
    }

    /* now all daemon processes have connected back to mpirun */
    RunParams.ClientsSpawned = 1;

    /* update runtime information using returned host information */
    FixRunParams(RunParams.server->nhosts());
    getSocketsToClients();

    /* totalview debugging of client "daemons" */
    MPIrunTVSetUp();

    /* send initial input parameters */
    ulm_dbg(("\nmpirun: sending initial input to daemons...\n"));
    rc = SendInitialInputDataToClients();
    if (rc != ULM_SUCCESS) {
        ulm_err(("Error: While sending initial input data to clients (%d)\n", rc));
        Abort();
    }

    /*
     * at this stage all application processes have been
     * created on the remote hosts
     */

    /* collect PIDs of client applications */
    RunParams.AppPIDs = ulm_new(pid_t *, RunParams.NHosts);
    for (int i = 0; i < RunParams.NHosts; i++) {
        RunParams.AppPIDs[i] = ulm_new(pid_t, RunParams.ProcessCount[i]);
    }
    if (!getClientPids(RunParams.AppPIDs, &rc)) {
        ulm_err(("Error: Can't get client process IDs (%d)\n", rc));
        Abort();
    }

    /* totalview debugging of all client processes */
    if (RunParams.TVDebug && RunParams.TVDebugApp) {
        MPIrunTVSetUpApp(RunParams.AppPIDs);
    }

    /* IP address information exchange - postfork */
    if (!exchangeIPAddresses(&rc)) {
        ulm_err(("Error: While exchanging IP addresses (%d)\n", rc));
        Abort();
    }

    /* UDP port information exchange - postfork */
    if (!exchangeUDPPorts(&rc)) {
        ulm_err(("Error: While exchangind UDP ports (%d)\n", rc));
        Abort();
    }

    /* TCP port information exchange - postfork */
    if (!exchangeTCPPorts(&rc)) {
        ulm_err(("Error: While exchangind TCP ports (%d)\n", rc));
        Abort();
    }

    /* GM info information exchange - postfork */
    if (!exchangeGMInfo(&rc)) {
        ulm_err(("Error: While exchanging GM information (%d)\n", rc));
        Abort();
    }

    /* InfiniBand information exchange - postfork */
    if (!exchangeIBInfo(&rc)) {
        ulm_err(("Error: While exchanging InfiniBand (IB) information (%d)\n", rc));
        Abort();
    }

    /* more banner info: print out process distribution */
    if (RunParams.Quiet == 0) {
        fprintf(stderr,
                "LA-MPI: *** %d process(es) on %d host(s):",
                nprocs, RunParams.NHosts);
        for (int h = 0; h < RunParams.NHosts; h++) {
            struct hostent *hptr;

            /* try to get a name rather than dotted IP address */
            hptr = gethostbyname(RunParams.HostList[h]);
            if (!hptr) {
                perror("gethostbyname");
            }
            hptr = gethostbyaddr(hptr->h_addr, hptr->h_length, AF_INET);
            if (!hptr) {
                perror("gethostbyaddr");
            }
            fprintf(stderr, " %d*%s",
                    RunParams.ProcessCount[h], hptr->h_name);
        }
        fprintf(stderr, "\n");
    }

    /* release all processes explicitly with barrier admin. message */
    if (!releaseClients(&rc)) {
        ulm_err(("Error: Timed out while waiting to release clients (%d)\n", rc));
        Abort();
    }

    /* wait for child prun process to complete */
    if (RunParams.UseRMS) {
        int term_status;
        wait(&term_status);
    } else {
        Daemonize();
    }

    /* free memory */
    for (int i = 0; i < RunParams.NHosts; i++) {
        ulm_delete(RunParams.AppPIDs[i]);
    }
    ulm_delete(RunParams.AppPIDs);
    ulm_delete(RunParams.Networks.
               TCPAdminstrativeNetwork.SocketsToClients);

    return EXIT_SUCCESS;
}
