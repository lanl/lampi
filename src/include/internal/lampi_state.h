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

#ifndef LAMPI_INTERNAL_LAMPI_STATE_H
#define LAMPI_INTERNAL_LAMPI_STATE_H

#include <sys/types.h>
#include <unistd.h>

#include "internal/types.h"
#include "os/atomic.h"

struct bsendData_t;

#ifdef __cplusplus

class adminMessage;
class contextIDCtl_t;
class pathContainer_t;

#endif

/*
 * LA-MPI run-time state
 */

typedef struct {
    lockStructure_t lock[1];    /* lock since this will be in shared memory */
    volatile int flag;          /* has a child exited abnormally? */
    pid_t pid;                  /* pid of child */
    int signal;                 /* signal propagated from child */
    int status;                 /* exit status of child */
} abnormalExitInfo;

typedef struct {

    int *map_global_proc_to_on_host_proc_id; /* global rank to local (on-host) rank map */
    
    int *map_global_rank_to_host;   /* global rank to host index map */
    int *map_host_to_local_size;    /* number of procs on a given host */
    int debug_location;
    int global_rank;                /* rank of this process */
    int global_size;                /* total number of processes in COMM_WORLD */
    int global_to_local_offset;
    int hostid;                     /* host index of this host */
    int iAmDaemon;
    int initialized;
    int local_rank;                 /* on-host rank of this process */
    int local_size;                 /* number of processes on local host */
    int lock;
    int max_local_size;
    int nhosts;                     /* number of hosts in run */
    int useDaemon;
    unsigned int channelID;

    /*
     * list of interfaces to be used for TCP/UDP and their 
     * corresponding addresses
     */
    int                 if_count;
    InterfaceName_t    *if_names;
    struct sockaddr_in *if_addrs;

    /*
     * list of process host addresses index by:
     * h_addrs[globalProcessRank * if_index] 
     * where 0 <= if_index < if_count
     */
    struct sockaddr_in *h_addrs;

    /* 
     * boolean flags indicating run-time functionality
     */

    int debug;
    int error;

    int quadrics;
    int gm;
    int udp;
    int tcp;
    int ib;

    int usethreads;
    int usecrc;
    int checkargs;

    /*
     * memory locality parameters
     */

    int memLocalityIndex;
    int enforceAffinity;

    /*
     * daemon / child relations
     */

    pid_t *ChildPIDs;               /* array of app PID's */
    pid_t PIDofClientDaemon;        /* pid of the daemon process */
    int *IAmAlive;                  /* per-local-process shared memory flags */
    volatile pid_t *local_pids;

    /* 
     * stdio handling 
     */

    int interceptSTDio;             /* will library handle standard I/O? */
    int STDINfdToChild;
    int commonAlivePipe[2];         /* pipe used by daemon to determine when all procs have exited */
    int *STDERRfdsFromChildren;     /* stderr file descriptors to redirect */
    int *STDOUTfdsFromChildren;     /* stdout file descriptors to redirect */
    int output_prefix;              /* prepend informative prefix to stdout/stderr ? */
    int quiet;                      /* suppress start-up messages */
    int verbose;                    /* verbose start-up messages */
    int warn;                       /* enable warning messages */
    int isatty;                     /* is mpirun a tty? */
    size_t StderrBytesWritten;      /* number of bytes written to stderr */
    size_t StdoutBytesWritten;      /* number of bytes written to stdout */
    int *NewLineLast;               /* insert newline character? */
    int *LenIOPreFix;               /* length of string to prepend */
    PrefixName_t *IOPreFix;         /* string to prepend to output */

    /*
     * abnormal exit handling
     */

    abnormalExitInfo *AbnormalExit;

    /* 
     * MPI support
     */

    struct bsendData_t *bsendData;         /* for MPI buffered sends */

    /*
     * Synchronization flags (in shared memory)
     *
     * HostDoneWithSetup is an array used for a crude startup barrier.
     * If entry
     *     i == 1, host i is done with setup
     *     i == 0, host i is not done with setup
     * entry [NHosts] == 0 not all
     * hosts are done with setup == 1 all hosts are done with setup
     *
     * AllHostsDone is set to 1 when all hosts are done.  This is used
     * to keep the library processing messages.
     */

    struct {
        volatile int *HostDoneWithSetup;  
        volatile int *AllHostsDone;
    } sync;


    /*
     * All C++ variables below ...
     */

#ifdef __cplusplus
    adminMessage *client;           /* pointer to admin network manager */
    contextIDCtl_t *contextIDCtl;   /* controlling for generation of new context ID's */
    pathContainer_t *pathContainer; /* where to find our network paths */
#endif


} lampiState_t;

#endif /* LAMPI_INTERNAL_LAMPI_STATE_H */
