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



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "init/environ.h"

#include "internal/constants.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "util/Utility.h"
#include "run/JobParams.h"
#include "run/Input.h"
#include "run/globals.h"
//#include "queue/globals.h"

extern int MPIR_being_debugged;

/*
 * parse the command line arguments, and setup the data to be sent to
 * the clients
 */
int MPIrunProcessInput(int argc, char **argv,
                       int *NULMArgs, int **IndexULMArgs,
                       ULMRunParams_t *RunParameters,
                       int *FirstAppArgument)
{
    int 	i,j;
    char	*val;
	
    RunParameters->UserAppDirList = NULL;
    RunParameters->HostList = NULL;
    RunParameters->HostListSize = 0;
    RunParameters->NHosts = 0;
    RunParameters->NHostsSet = 0;
    RunParameters->CmdLineOK = 0;
    RunParameters->mpirunName[0] = '\0';
    RunParameters->maxCommunicators = NULL;
#ifdef NUMA
    RunParameters->CpuList = NULL;
    RunParameters->CpuListLen = 0;
    RunParameters->nCpusPerNode = 0;
#endif                          // NUMA
    RunParameters->ULMVerbocity = 0;
    RunParameters->UseThreads = 0;
    RunParameters->CheckArgs = 1;
    RunParameters->OutputPrefix = 0;
    RunParameters->HeartBeatTimeOut = int (HEARTBEATTIMEOUT);
    RunParameters->Networks.SharedMemSetup.PagesPerProc = NULL;
    RunParameters->quadricsRailMask = 0;

    RunParameters->UseLSF = 0;
#ifdef BPROC
    RunParameters->UseBproc = 1;
#else
    RunParameters->UseBproc = 0;
#endif

#ifdef USE_RMS
    RunParameters->UseRMS = 1;
#else
    RunParameters->UseRMS = 0;
#endif
    
    lampi_environ_find_string("LSB_MCPU_HOSTS", &val);
    if ( strlen(val) ) {
        RunParameters->UseLSF = 1;
        GetLSFResource();
    }

    RunParameters->Networks.SharedMemSetup.recvFragResources_m.
        outOfResourceAbort = NULL;
    RunParameters->Networks.SharedMemSetup.recvFragResources_m.retries_m =
        NULL;
    RunParameters->Networks.SharedMemSetup.recvFragResources_m.
        minPagesPerContext_m = NULL;
    RunParameters->Networks.SharedMemSetup.recvFragResources_m.
        maxPagesPerContext_m = NULL;
    RunParameters->Networks.SharedMemSetup.recvFragResources_m.
        maxTotalPages_m = NULL;

    RunParameters->UseCRC = 0;
#ifdef RELIABILITY_ON
    RunParameters->quadricsDoAck = 1;
    RunParameters->quadricsDoChecksum = 1;
#else
    RunParameters->quadricsDoAck = 0;
    RunParameters->quadricsDoChecksum = 0;
#endif

    /* check to see if being debugged by TotalView */
    if (MPIR_being_debugged) {
        GetTVAll("BeingTotalViewDebugged");
    } else {
        RunParameters->TVDebug = 0;
        RunParameters->TVDebugApp = 1;
    }

    /* */
#ifdef USE_RMS
    RunParameters->handleSTDio=0;
#else
    RunParameters->handleSTDio=1;
#endif

    /* get input data - not parsed at this stage */
    ScanInput(argc, argv);

    /* process input */
    for (i = 0; i < SizeOfInputOptionsDB; i++) {
        ULMInputOptions[i].fpToExecute(ULMInputOptions[i].FileName);
    }
    RunParameters->CmdLineOK = 1;

    /* find first index of user's binary arguments, or set to argc */
    *FirstAppArgument = _ulm_AppArgsIndex;

    /*
     * Allocate space to hold list of Daemon processes
     */
    RunParameters->DaemonPIDs = ulm_new(pid_t,  RunParameters->NHosts);

#ifndef USE_RMS
    /* disallow network setup if only 1 host */
    if (RunParameters->NHosts == 1)
        RunParameters->NPathTypes[0] = 0;

#endif

    /* Verify the hosts and process counts are allowed in LSF */
    if (RunParameters->UseLSF) {
        VerifyLsfResources(RunParameters);
    }

    /* initialalize network parameters */
    RunParameters->Networks.UseUDP=0;
    RunParameters->Networks.UseGM=0;

    /* initialize network parameters */
    RunParameters->Networks.UseUDP = 0;
    RunParameters->Networks.UseGM = 0;
    RunParameters->Networks.UseQSW=0;
#ifdef UDP
    RunParameters->Networks.UDPSetup.NUDPHosts=0;
#endif                          // UDP
#ifdef GM
    RunParameters->Networks.GMSetup.NGMHosts=0;
#endif
    for (j = 0; j < RunParameters->NHosts; j++) {
        for (i = 0; i < RunParameters->NPathTypes[j]; i++) {
            switch (RunParameters->ListPathTypes[j][i]) {
#ifdef UDP
            case PATH_UDP:          // UDP interface
		RunParameters->Networks.UseUDP = 1;
                (RunParameters->Networks.UDPSetup.NUDPHosts)++;
                break;
#endif                          // UDP
#ifdef GM
	    case PATH_GM:
		RunParameters->Networks.UseGM = 1;
		RunParameters->Networks.GMSetup.NGMHosts++;
		break;
#endif
	    case PATH_QUADRICS:
		RunParameters->Networks.UseQSW=0;
		RunParameters->handleSTDio=0;
		break;
            }
        }
    }

    return 0;
}
