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



#ifndef _MPIRUNJOBPARAMS
#define _MPIRUNJOBPARAMS

#include <unistd.h>

#include "internal/types.h"
#include "client/adminMessage.h"
#ifdef UDP
# include "path/udp/setupInfo.h"
#endif
#ifdef GM
# include "path/gm/setupInfo.h"
#endif
#include "client/SocketAdminNetwork.h"
#include "path/sharedmem/SharedMemJobParams.h"
#include "run/ResourceSpecs.h"

// structure to hold input environment variables
typedef struct ulmEnvSet {
    char *var_m;
    bool setForAllHosts_m;
    bool *setForThisHost_m;
    char **envString_m;
} ulmENVSettings;

/*
 *  information characterizing the inter-host network (both
 *  "administartive" and "compute" for a job.
 */
typedef struct ULMRunNetworkTypes {
    /* structure with the administrative network info */
    SocketAdminNetwork_t TCPAdminstrativeNetwork;

    /* shared memory setup */
    int UseSharedMemory;

    /* structure for UDP setup */
    int UseUDP;
#ifdef UDP
    UDPNetworkSetupInfo UDPSetup;
#endif

    /* structure for GM setup */
    int UseGM;
#ifdef GM
    GMNetworkSetupInfo GMSetup;
#endif

    /* structure for Quadrics setup */
    int UseQSW;

    /* structure with the Shared Memory requested setup */
    SharedMemJobParams_t SharedMemSetup;
} ULMRunNetworkTypes_t;

/*
 *  structure characterizing a job
 */
typedef struct ULMRunParams {
    /* number of host participating in run */
    int NHosts;

    /* has the number of hosts been specified on the command line */
    int NHostsSet;

    /* number of process on each host */
    int *ProcessCount;

    /* threads being used */
    int UseThreads;

    /* check MPI API arguments */
    int CheckArgs;

    /* prepend output prefix */
    int OutputPrefix;

    /* level of debug output */
    int ULMVerbocity;

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

    /* pointer to struct with Network Data */
    ULMRunNetworkTypes_t Networks;

#ifdef NUMA
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

    /* list of socket that send standad input data to host i */
    int *STDINfds;

    /* list of socket that receives standad out data from host i */
    int *STDOUTfds;

    /* list of socket that receives standad error data from host i */
    int *STDERRfds;

    // LSF being used
    int UseLSF;

    // Bproc being used
    int UseBproc;

    /* indicate if being debugged by TotalView */
    int TVDebug;

    /* indicate if to debug the app
     *                     -  after the fork  == 1  (default)
     *                     -  before the fork right after startup == 0
     */
    int TVDebugApp;

    /* list of Debug hosts - in xxx.xxx.xxx.xxx format */
    HostName_t *TVHostList;

    /* list of Client daemon process pid's */
    pid_t *DaemonPIDs;

	/* server instance. */
	adminMessage *server;
	
    /* Heartbeat Time out period - in seconds */
    int HeartBeatTimeOut;

    // list of environment variables to set on each host
    int nEnvVarsToSet;
    ulmENVSettings *envVarsToSet;

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

    /* should we use CRCs instead of checksums -- where supported */
    int UseCRC;

    /* should we use local send completion notification or not on Quadrics */
    int quadricsDoAck;

    /* should we do any checksumming/CRC on Quadrics */
    int quadricsDoChecksum;
    
} ULMRunParams_t;

#endif                          /* _MPIRUNJOBPARAMS */
