/*
 * Copyright 2002-2003. The Regents of the University of
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

#ifndef ADMINMESSAGE_H
#define ADMINMESSAGE_H

#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "ulm/errors.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/system.h"
#include "internal/types.h"
#include "queue/barrier.h"
#include "mem/FixedSharedMemPool.h"

#ifdef __APPLE__
#include <unistd.h>
#include <sys/uio.h>
#endif

#if ENABLE_BPROC
#include <sys/bproc.h>
#endif

#include "util/Lock.h"

class adminMessage;

/* returns: number of destination bytes used
 * arguments:
 *      void *source - of unpacked data
 *      void *destination - of packed data
 *      unsigned int count - data type count
 */
typedef unsigned int (*packFunction) (void *, void *, int);

/* returns: number of bytes needed to hold packed count data types
 * arguments:
 *      unsigned int count
 */
typedef unsigned int (*packSizeFunction) (int);

/* returns: number of destination bytes used
 * arguments:
 *      void *source - of packed data
 *      void *destination - of unpacked data
 *      unsigned int count - data type count
 */
typedef unsigned int (*unpackFunction) (void *, void *, int);

/* returns: number of bytes needed of packed data to constitute count data types
 * arguments:
 *      unsigned int count
 */
typedef unsigned int (*unpackSizeFunction) (int);

/* returns: true if successful, false if unsuccessful
 * arguments:
 *      adminMessage *caller
 *      int rank - global host rank of message source
 *      unsigned int tag - message tag
 */
typedef bool(*callbackFunction) (adminMessage *, int, int);

class adminMessage;
bool processNHosts(adminMessage * server, int rank, int tag);

typedef struct {
    int *groupProcIDOnHost;     // list of ranks in this context on a host
    int nGroupProcIDOnHost;     // number of ranks on host in this context
} admin_host_info_t;

typedef struct {
    enum {
        /* marker indicating start of shared memory input parameters */
        START_SHARED_MEM_INPUT = 1,
        /* marker indicating end of shared memory input parameters */
        END_SHARED_MEM_INPUT,

        /* marker indicating the start of GM input data */
        START_GM_INPUT,
        /* marker indicating the end of GM input data */
        END_GM_INPUT,
        /* marker indicating the start of Quadrics input data */
        START_QUADRICS_INPUT,
        /* marker indicating the end of Quadrics input data */
        END_QUADRICS_INPUT,
        /* marker indicating the start of InfiniBand input data */
        START_IB_INPUT,
        /* marker indicating the end of InfiniBand input data */
        END_IB_INPUT,
        /* end marker */
        START_TCP_INPUT,
        /* marker indicating the start of TCP input data */
        END_TCP_INPUT,
        /* marker indicating the end of TCP input data */
        COUNT
    };
} dev_type_params;

typedef struct {
    enum {
        /* number of bytes per process for the shared memory descriptor
         * pool */
        SMDESCPOOLBYTESPERPROC = dev_type_params::COUNT + 1,
        /* shared memory per proc page limit */
        SMPSMPAGESPERPROC,
        /* flag indicating to return error status when out of SMP
         * message fragment headers, rather than abort */
        SMPFRAGOUTOFRESRCABORT,
        /* flag indicating how many consective times to try and get
         * more SMP message fragment headers before deciding none are
         * available. */
        SMPFRAGRESOURCERETRY,
        /* minimum number of pages for SMP message fragment headers
         * per context */
        SMPFRAGMINPAGESPERCTX,
        /* maximum number of pages for SMP message fragment headers
         * per context */
        SMPFRAGMAXPAGESPERCTX,
        /* upper limiton total number of pages for SMP message fragment
         * headers */
        SMPFRAGMAXTOTPAGES,
        /* flag indicating to return error status when out of SMP
         * isend message headers, rather than abort */
        SMPISENDOUTOFRESRCABORT,
        /* flag indicating how many consective times to try and get
         * more SMP isend message headers before deciding none are
         * available. */
        SMPISENDRESOURCERETRY,
        /* minimum number of pages for SMP message isend descriptors
         * per context */
        SMPISENDMINPAGESPERCTX,
        /* maximum number of pages for SMP message isend descriptors
         * headers per context */
        SMPISENDMAXPAGESPERCTX,
        /* upper limiton total number of pages for SMP message isend
         * descriptors */
        SMPISENDMAXTOTPAGES,
        /* flag indicating to return error status when out of SMP
         * data buffers, rather than abort */
        SMPDATAPAGESOUTOFRESRCABORT,
        /* flag indicating how many consective times to try and get
         * more SMP data buffers before deciding none are available. */
        SMPDATAPAGESRESOURCERETRY,
        /* minimum number of pages for SMP data buffers per context */
        SMPDATAPAGESMINPAGESPERCTX,
        /* maximum number of pages for SMP data buffers headers per
         * context */
        SMPDATAPAGESMAXPAGESPERCTX,
        /* upper limiton total number of pages for SMP data buffers */
        SMPDATAPAGESMAXTOTPAGES
    };
} shared_mem_intput_params;

class adminMessage {
  public:
    // message and data tag values
    enum {
        INITMSG,                /* 3 integers of authData, 1 integer of global host rank, 1 integer of number of local processes */
        INITOK,                 /* 3 integers of authData, 1 integer of go-ahead status (0 = abort, 1 = go-ahead) */
        RUNPARAMS,              /* (client) integer local_nprocs() and pid_t daemon PID */
        ENDRUNPARAMS,           /* marker indicating end of initial data paramters - no data */
        THREADUSAGE,            /* indicates if threads are expected, or not - 1 interger */
        DEBUGGER,               /* debugger flags - 2 integers */
        CRC,                    /* 1 bool of use CRC */
        CLIENTPIDS,             /* (client) n pid_t client PIDs */
        IFNAMES,
        QUADRICSFLAGS,          /* 1 bool of DoAck, and 1 bool of DoChecksum */
        BARRIER,                /* 1 integer of go-ahead status (0 = abort, 1 = go-ahead) */
        NHOSTS,                 /* 1 integer from server with total number of hosts */
        HOSTID,                 /* host index */
        CHECKARGS,              /* function arguments need to be checked */
        OUTPUT_PREFIX,          /* controling if prefix will be added to std i/o */
        QUIET,                  /* supress start-up messages */
        VERBOSE,                /* verbose start-up messages */
        WARN,                   /* enable warning messages */
        ISATTY,                 /* is mpirun a tty? */
        DOHEARTBEAT,            /* do heartbeats? */
        HEARTBEATPERIOD,        /* interval between heartbeats */
        HEARTBEATTIMEOUT,       /* interval without seeing a heartbeat before application is shutdown */
        MAXCOMMUNICATORS,       /* limit on number of communicators */
        IRECVOUTOFRESRCABORT,   /* if set, return error when out of irecv headers - don't abort */
        IRECVRESOURCERETRY,     /* how many times to retry before deciding no more irecv headers are available */
        IRCEVMINPAGESPERCTX,    /* minimun number of pages for irecv descriptors - per context */
        IRECVMAXPAGESPERCTX,    /* maximum number of pages for irecv descriptors - per context */
        IRECVMAXTOTPAGES,       /* maximum total number of pages for  irecv descriptors */
        NPATHTYPES,             /* number of network device types */
        GMMAXDEVS,              /* maximum number of opened Myrinet/GM devices */
        IBMAXACTIVE,            /* 3 integers: max. active HCAs, max. active ports/HCA, sizeof(ib_ud_peer_info_t) */
#if ENABLE_NUMA
        CPULIST,                /* list of cpus for resource affinity */
        NCPUSPERNODE,           /* number of cpus per node */
        USERESOURCEAFFINITY,    /* user resource affinity */
        DEFAULTRESOURCEAFFINITY,        /* default resource affinity */
        MANDATORYRESOURCEAFFINITY,      /* mandatory resource affinity */
#endif                          /* ENABLE_NUMA */
        NUMMSGTYPES             /* must be last */
    };

    enum packType {
        BYTE = 1,
        SHORT = 2,
        INTEGER = 4,
        LONGLONG = 8,
        USERDEFINED = 0
    };

    static dev_type_params devTags;

    enum direction {
        SEND,
        RECEIVE,
        BOTH
    };

    enum recvResult {
        ERROR = 1,
        TIMEOUT = 2,
        HANDLED = 4,
        OK = 8,
        NOERROR = 14
    };

    enum {
        MAXHOSTNAMESIZE = 512,
        DEFAULTBUFFERSIZE = ULM_MAX_IO_BUFFER + 512,
        MAXSOCKETS = 8192
    };


    /* collective operation type */
    enum {
        ALLGATHER = 1
    };

    enum collective_size { MINCOLLMEMPERPROC = 512 };

    /* the rank of the daemon process - if it exists */
    enum { DAEMON_PROC = -1 };

    /* tag for unknown number of app procs on Client side */
    enum { UNKNOWN_N_PROCS = -1 };

    /* tag for unknown host rank - sent by the client */
    enum { UNKNOWN_HOST_ID = -1 };

    int socketToServer_m;
    int serverSocket_m;
    bool cancelConnect_m;
    bool socketsToProcess_m[MAXSOCKETS];

  private:

    bool client_m, server_m;

    char **connectInfo_m;
    int curHost_m;
    int hostRank_m;
    int totalNProcesses_m;
    int nhosts_m;
    pid_t daemonPIDs_m[MAXSOCKETS];


    static struct sigaction oldSignals, newSignals;
    static jmp_buf savedEnv;

    bool clientSocketActive_m[MAXSOCKETS];
    int ranks_m[MAXSOCKETS];
    int processCount[MAXSOCKETS];
    int largestClientSocket_m;
    int lastRecvSocket_m;
    int hint_m;

    unsigned char *sendBuffer_m;
    unsigned char *recvBuffer_m;
    int sendOffset_m;
    int recvOffset_m;
    int recvBufferBytes_m;
    int recvMessageBytes_m;
    int sendBufferSize_m;
    int recvBufferSize_m;

    int authData_m[3];
    int port_m;

    /* Information for use with some of the collective operations.
     *   These collectives are designed to work correctlfy both
     *   pre and post fork */
    admin_host_info_t *groupHostData_m;
    swBarrierData *barrierData_m;
    void *RESTRICT_MACRO sharedBuffer_m;
    ssize_t lenSharedMemoryBuffer_m;
    volatile int *syncFlag_m;
    int localProcessRank_m;
    int hostCommRoot_m;         // local proc that participates in interhost
    //   data exchange
    long long collectiveTag_m;
    char hostname_m[MAXHOSTNAMESIZE];

    callbackFunction callbacks_m[NUMMSGTYPES];

  public:
    int clientRank2FD(int rank) {
        int returnValue = -1;

        if (ranks_m[hint_m] == rank) {
            if (clientSocketActive_m[hint_m]) {
                returnValue = hint_m;
                return returnValue;
            }
        }

        for (int i = 0; i <= largestClientSocket_m; i++) {
            if (ranks_m[i] == rank) {
                if (clientSocketActive_m[i]) {
                    returnValue = i;
                    hint_m = i;
                    break;
                }
            }
        }

        return returnValue;
    }

  private:

    int clientFD2Rank(int sockfd) {
        hint_m = sockfd;
        return ranks_m[sockfd];
    }

    int clientSocketCount(void) {
        int returnValue = 0;

        if (server_m) {
            for (int i = 0; i <= largestClientSocket_m; i++) {
                if (clientSocketActive_m[i]) {
                    returnValue++;
                }
            }
        }

        return returnValue;
    }

    int clientProcessCount(void) {
        int returnValue = 0;

        if (server_m) {
            for (int i = 0; i <= largestClientSocket_m; i++) {
                if (clientSocketActive_m[i]) {
                    returnValue += processCount[i];
                }
            }
        }

        return returnValue;
    }


    bool getRecvBytes(int bytes, int timeout) {
        int bytesReceived = 0;
        bool returnValue = true;

        if (timeout > 0) {
            if (sigaction(SIGALRM, &newSignals, &oldSignals) < 0) {
                ulm_err(("adminMessage::getRecvBytes unable to install SIGALRM handler!\n"));
                return false;
            }
            if (setjmp(savedEnv) != 0) {
                ulm_err(("adminMessage::getRecvBytes %d second timeout exceeded!\n", timeout));
                return false;
            }
            alarm(timeout);
        }

        while (bytesReceived < bytes) {
            int bytesThisRead = read(lastRecvSocket_m,
                                     (void *) (recvBuffer_m + recvBufferBytes_m),
                                     bytes - bytesReceived);
            if (bytesThisRead <= 0) {
                returnValue = false;
                break;
            } else {
                bytesReceived += bytesThisRead;
                recvBufferBytes_m += bytesThisRead;

            }
        }

        if (timeout > 0) {
            alarm(0);
            sigaction(SIGALRM, &oldSignals, (struct sigaction *) NULL);
        }

        return returnValue;
    }


  public:
    /* default constructor */
    adminMessage();

    /* default destructor */
    virtual ~adminMessage();

    static void handleAlarm(int signal);

    /* (client) initialize connection info and socket to server
     * auth (in): 3 integers of authorization information
     * hostname (in): string containing IP address or hostname of server
     * port (in): TCP port of server
     * returns: true if successful, false if unsuccessful (requiring program exit)
     */
    bool clientInitialize(int *authData, char *hostname, int port);


    /* (client) initialize connection info and socket to server
     * nprocesses (in): the number of local processes to report to server
     * hostrank (in): the global rank of our host
     * timeout (in): time in seconds to attempt connect to server
     * returns: true if successful, false if unsuccessful (requiring program exit)
     */
    bool clientConnect(int nprocesses, int hostrank, int timeout = -1);

    /* (server) initialize socket and wait for all connections from clients
     * authData (in): 3 integers of authorization information
     * nprocs (in): the total number of processes to account for
     * port (out): the port number of the listening socket
     * returns: true if successful, false if unsuccessful (requiring program exit)
     */
    bool serverInitialize(int *authData, int nprocs, int *port);

    /* (server) initialize socket and wait for all connections from clients
     * timeout (in): time in seconds to wait for all connections from clients
     * returns: true if successful, false if unsuccessful (requiring program exit)
     */

    bool serverConnect(int *procList, HostName_t * hostList, int numHosts, int timeout = -1);

    /* (client/server) close sockets and free memory resources */
    void terminate() {
        if (client_m) {
            if (socketToServer_m >= 0)
                close(socketToServer_m);
        }
        if (server_m) {
            if (serverSocket_m >= 0)
                close(serverSocket_m);
            for (int i = 0; i <= largestClientSocket_m; i++)
                if (clientSocketActive_m[i]) {
                    close(i);
                }
        }
        if (sendBuffer_m)
            ulm_free(sendBuffer_m);
        if (recvBuffer_m)
            ulm_free(recvBuffer_m);
    }


    /* enlarge buffer if necessary and reset buffer offset to zero
     * dir (in): which buffer(s) is involved
     * size (in): desired size of buffer (0 == don't change buffer size)
     * returns: true if successful, false if buffer could not be grown
     */
    bool reset(direction dir, int size = 0);


    /* pack data into the send buffer in network order
     * data (in): pointer to the data to be packed
     * type (in): the type of data element (could be USERDEFINED)
     * count (in): the number of contiguous data elements in data
     * pf (in): customized pack function (type == USERDEFINED)
     * psf (in): customized function to return the number of bytes needed for count data types (type == USERDEFINED)
     * returns: true if successful, false if unsuccessful for whatever reason
     */
    bool pack(void *data, packType type, int count, packFunction pf = 0, packSizeFunction psf = 0);


    /*
       ASSERT: msg has been received in recv buffer and current buffer ptr is pointing
       to tag of next block of data.
     */
    recvResult nextTag(int *tag);

    /* unpack data from a receive buffer
     * data (in): pointer for the unpacked data to be stored at
     * type (in): the type of data element (could be USERDEFINED)
     * count (in): the number of contiguous data elements in data
     * timeout (in): time in seconds to wait for all the data requested (-1 == unlimited)
     * upf (in): customized unpack function (type == USERDEFINED)
     * upsf (in): customized function to return the number of bytes needed for count data types (type == USERDEFINED)
     * returns: true if successful, false if unsuccessful due to timeout/connection failure for needed data
     */
    bool unpack(void *data, packType type, int count, int timeout = -1, unpackFunction upf = 0,
                unpackSizeFunction upsf = 0);


    bool unpackMessage(void *data, packType type, int count, int timeout = -1, unpackFunction upf =
                       0, unpackSizeFunction upsf = 0);


    /* send data from send buffer with message tag to a single destination
     * rank (in): global host rank of destination, -1 for server
     * tag (in): message tag value
     * errorCode (out): errorCode if false is returned
     * returns: true if successful, false if unsuccessful (errorCode is then set)
     */
    bool send(int rank, int tag, int *errorCode);

    /* send data from send buffer (or receive buffer if useRecvBuffer is true) to all destinations
     * tag (in): message tag value
     * errorCode (out): errorCode if false is returned
     * returns: true if successful, false if unsuccessful (errorCode is then set)
     */
    bool broadcast(int tag, int *errorCode);

    /*
     * this primitive implementation of allgather handles only contiguous data.
     *     sendbuf - source buffer
     *     recvbug - destination buffer
     *     bytesPerProc - number of bytes each process contributes to the
     *                    collective operation
     */
    int allgather(void *sendbuf, void *recvbuf, ssize_t bytesPerProc);

    /* 
     * this routine sets up this collective data for use by this
     * object's collective routines
     */
    int setupCollectives(int myLocalRank, int myHostRank,
                         int *map_global_rank_to_host, int daemonIsHostCommRoot,
                         ssize_t lenSMBuffer, void *sharedBufferPtr,
                         void *lockPtr, void *CounterPtr, int *sPtr);

    /* this is a primtive implementation of a barrier - it can
     *   optionally include the deamon process, if such exists
     */
    void localBarrier();

    /* get a tag - not thread safe.  This is assumed to be used by the
     *   library in a non-threaded region.
     */
    long long get_base_tag(int num_requested);

    /* receive data into receive buffer from a given destination
     * rank (in): global host rank of source, -1 for server
     * tag (in): message tag value
     * errorCode (out): errorCode if false is returned
     * timeout (in): -1 no timeout, 0 poll, > 0 milliseconds to wait in select
     * returns: OK if received message and needs to be handled, HANDLED if
     * received and automatically handled through a callback, TIMEOUT if
     * nothing received during the timeout, or ERROR if there is an error
     * (errrorCode is then set)
     */

    recvResult receive(int rank, int *tag, int *errorCode, int timeout = -1) {
        recvResult returnValue = OK;
        int sockfd = (rank == -1) ? socketToServer_m : clientRank2FD(rank);
        int s;
        struct timeval t;
        ulm_fd_set_t fds;
        ulm_iovec_t iovecs;

        reset(RECEIVE);
        *tag = -1;


        if (sockfd < 0) {
            *errorCode = ULM_ERR_RANK;
            returnValue = ERROR;
            return returnValue;
        }

        if (timeout >= 0) {
            int seconds = timeout / (int) 1000;
            int microseconds = timeout * 1000;
            if (seconds) {
                microseconds = (timeout - (seconds * 1000)) * 1000;
            }
            t.tv_sec = seconds;
            t.tv_usec = microseconds;
        }

        bzero(&fds, sizeof(fds));
        FD_SET(sockfd, (fd_set *) & fds);

        if ((s = select(sockfd + 1, (fd_set *) & fds, (fd_set *) NULL, (fd_set *) NULL,
                        (timeout < 0) ? (struct timeval *) NULL : &t)) == 1) {
            iovecs.iov_base = tag;
            iovecs.iov_len = sizeof(int);
            if (ulm_readv(sockfd, &iovecs, 1) != sizeof(int)) {
                *errorCode = errno;
                returnValue = ERROR;
                return returnValue;
            }
            lastRecvSocket_m = sockfd;
        } else {
            returnValue = (s == 0) ? TIMEOUT : ERROR;
            if (returnValue == ERROR)
                *errorCode = errno;
            return returnValue;
        }

        // check for any registered callback for this tag value
        if (callbacks_m[*tag]) {
            bool callReturn = (*callbacks_m[*tag]) (this, rank, *tag);
            if (callReturn) {
                returnValue = HANDLED;
            } else {
                *errorCode = ULM_ERROR;
                returnValue = ERROR;
            }
        }

        return returnValue;
    }

    /* received data into receive buffer from any destination
     * rank (out): global host rank of source, -1 for server
     * tag (out): message tag value
     * errorCode (out): errorCode if false is returned
     * timeout (in): -1 no timeout, 0 poll, > 0 milliseconds to wait in select
     * returns: OK if received message and needs to be handled, HANDLED if
     * received and automatically handled through a callback, TIMEOUT if
     * nothing received during the timeout, or ERROR if there is an error
     * (errrorCode is then set)
     */
    recvResult receiveFromAny(int *rank, int *tag, int *errorCode, int timeout = -1);


    /* register a callback to be called from receive or receiveFromAny for a given tag value */
    bool registerCallback(int tag, callbackFunction cf) {
        if ((tag >= NUMMSGTYPES) || (tag < 0)) {
            return false;
        }
        callbacks_m[tag] = cf;
        return true;
    }

    /* unregister a callback to be called from receive or receiveFromAny for a given tag value */
    void unregisterCallback(int tag) {
        if ((tag >= NUMMSGTYPES) || (tag < 0)) {
            return;
        }
        callbacks_m[tag] = (callbackFunction) 0;
    }

    /* number of hosts
     * timeout (in): -1 no timeout, 0 poll, > 0 milliseconds to wait in select
     * returns: number of hosts, or -1 if an error has occurred
     */
    int nhosts(int timeout = -1) {
        int returnValue = -1;
        int tag, errorCode;

        if (server_m)
            return nhosts_m;
        else if (client_m) {
            if (reset(BOTH) && send(-1, NHOSTS, &errorCode)) {
                int recvd;
                do {
                    recvd = receive(-1, &tag, &errorCode, timeout);
                    if ((recvd == OK) && (tag == NHOSTS)) {
                        unpack(&returnValue, INTEGER, 1);
                    } else {
                        ulm_warn(("adminMessage::nhosts client received "
                                  "msg. with tag %d (recvd %d)\n", tag, recvd));
                    }
                } while (recvd == HANDLED);
            }
        }

        return returnValue;
    }

    /* return true if the name or IP address in dot notation of the
     * peer to be stored at dst, or false if there is an error */
    bool peerName(int hostrank, char *dst, int bytes, bool useDottedIP);

    /*
     *  Accessor methods.
     */

    pid_t daemonPIDForHostRank(int rank) {
        return daemonPIDs_m[rank];
    }

    int processCountForHostRank(int rank) {
        return processCount[rank];
    }

    void setHostRank(int rank);

    admin_host_info_t *groupHostData();
    void setGroupHostData(admin_host_info_t * groupHostData);

    ssize_t lenSharedMemoryBuffer();
    void setLenSharedMemoryBuffer(ssize_t len);

    int localProcessRank();
    void setLocalProcessRank(int rank);

    void setNumberOfHosts(int nhosts);

    int totalNumberOfProcesses();
    void setTotalNumberOfProcesses(int nprocs);

    int *getAuthData(void) {
        return authData_m;
    }
};

#endif
