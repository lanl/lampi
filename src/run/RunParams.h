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

#ifndef _MPIRUNJOBPARAMS
#define _MPIRUNJOBPARAMS

#include <stdlib.h>

#include "client/adminMessage.h"
#include "client/SocketAdminNetwork.h"
#include "internal/types.h"

#if ENABLE_SHARED_MEMORY
#include "path/sharedmem/SharedMemJobParams.h"
#endif
#if ENABLE_UDP
# include "path/udp/setupInfo.h"
#endif
#if ENABLE_TCP
# include "path/tcp/tcpsetup.h"
#endif
#if ENABLE_GM
# include "path/gm/setupInfo.h"
#endif
#if ENABLE_INFINIBAND
#include "path/ib/setupInfo.h"
#endif


/* structure to hold input environment variables */
struct EnvSettings_t {
    char *var_m;
    bool setForAllHosts_m;
    bool *setForThisHost_m;
    char **envString_m;
};

/* mpirun runtime state */
struct RunParams_t {

    /* Have clients been spawned ? */
    int ClientsSpawned;

    /* number of host participating in run */
    int NHosts;

    /* has the number of hosts been specified on the command line */
    int NHostsSet;

    /* number of process on each host */
    int *ProcessCount;

    /* Number of hosts that terminated normally */
    int HostsNormalTerminated;

    /* Number of hosts that terminated abnormally */
    int HostsAbNormalTerminated;

    /* threads being used */
    int UseThreads;

    /* check MPI API arguments */
    int CheckArgs;

    /* prepend output prefix */
    int OutputPrefix;

    /* Is mpirun a terminal? */
    int isatty;

    /* suppress start-up message */
    int Quiet;

    /* hostname of mpirun host */
    HostName_t mpirunName;

    /* fully qualified network name of host in run */
    HostName_t *HostList;

    /* number of hosts in HostList */
    int HostListSize;

    /* Working dir list for each host */
    DirName_t *WorkingDirList;

    /* list of binaries to run - per host */
    ExeName_t *ExeList;

    /* directory of each binary - per host */
    DirName_t *UserAppDirList;

    /* list of Number of "compute" network interfaces per host */
    int *NPathTypes;

    /* list of "compute" network interface types used - per host list */
    int **ListPathTypes;

    /* list of network interfaces */
    InterfaceName_t *InterfaceList;
 
    /* number of elements in interface list */
    int NInterfaces;

    /* information about the available networks */
    struct {
        int UseSharedMemory;
        int UseUDP;
        int UseTCP;
        int UseQSW;
        int UseGM;
        int UseIB;
        SocketAdminNetwork_t TCPAdminstrativeNetwork;
#if ENABLE_SHARED_MEMORY
        SharedMemJobParams_t SharedMemSetup;
#endif
#if ENABLE_UDP
        UDPNetworkSetupInfo UDPSetup;
#endif
#if ENABLE_TCP
        TCPNetworkSetupInfo TCPSetup;
#endif
#if ENABLE_GM
        GMNetworkSetupInfo GMSetup;
#endif
#if ENABLE_INFINIBAND
        IBNetworkSetupInfo IBSetup;
#endif
    } Networks;

#if ENABLE_NUMA
    int *CpuList;
    int CpuListLen;
    // number of cpus to allocate per node (1 or 2)
    int *nCpusPerNode;
    // cpu affinity policy
    int *CpuPolicy;
    // memory affinity policy
    int *Affinity;
    // resource affinity specification - 1 apply affinity, 0 don't apply
    //   affinity
    int *useResourceAffinity;
    // resource affinity specification - 1 use os supplied defaults
    //                                   0 use user supplied affinity
    int *defaultAffinity;
    // resource affinity mandatory - 1 mandatory, fail if can't use affinity
    //                             - 0 continue if affinity can't be applied
    int *mandatoryAffinity;
#endif

    /* handle standard i/o traffic */
    int handleSTDio;

    /* STDIN file descriptor - input forward to process 0 */
    int STDINfd;

    // LSF being used
    int UseLSF;

    // RMS being used
    int UseRMS;

    // Bproc being used
    int UseBproc;

    /* indicate if being debugged by TotalView */
    int TVDebug;

    /* indicate if to debug the app
     *                     -  after the fork  == 1  (default)
     *                     -  before the fork right after startup == 0
     */
    int TVDebugApp;

    /* mpirun should spawn xterm/gdb sessions for each process */
    int GDBDebug;

    /* list of Debug hosts - in xxx.xxx.xxx.xxx format */
    HostName_t *TVHostList;

    /* list of client daemon process pid's */
    pid_t *DaemonPIDs;

    /* list of client application process pid's (per host) */
    pid_t **AppPIDs;

    /* server instance. */
    adminMessage *server;
	
    /* Heartbeat Time out period - in seconds */
    int HeartBeatTimeOut;

    // list of environment variables to set on each host
    int nEnvVarsToSet;
    EnvSettings_t *envVarsToSet;

    // irecv descriptors
    resourceSpecs_t irecvResources_m;

    //! maximum number of communicators per host
    int *maxCommunicators;

    //! number of bytes to per process for shared memory descriptor pool
    ssize_t *smDescPoolBytesPerProc;

    /* mask indicating which quadrics rails to use */
    int quadricsRailMask;

    /* whether to print Usage info in Abort() */
    int CmdLineOK;

    /* run on the local host using fork/exec to spawn processes */
    int Local;
    
    /* use SSH instead of RSH for spawning processes */
    int UseSSH;
    
    /* should we use CRCs instead of checksums -- where supported */
    int UseCRC;
    
    /* should we use local send completion notification or not on Quadrics */
    int quadricsDoAck;

    /* should we use quadrics hardware optimizations */
    int quadricsHW;

    /* should we do any checksumming/CRC on Quadrics */
    int quadricsDoChecksum;
};

#endif                          /* _MPIRUNJOBPARAMS */
