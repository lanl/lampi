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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "internal/profiler.h"
#include "init/environ.h"
#include "internal/constants.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/Input.h"
#include "run/Run.h"
#include "run/RunParams.h"
#include "util/Utility.h"

extern int MPIR_being_debugged;

/*
 * parse the command line arguments, and setup the data to be sent to
 * the clients
 */
int ProcessInput(int argc, char **argv, int *FirstAppArg)
{
    /* some default runtime parameters */
    RunParams.UserAppDirList = NULL;
    RunParams.HostList = NULL;
    RunParams.HostListSize = 0;
    RunParams.NHosts = 0;
    RunParams.NHostsSet = 0;
    RunParams.CmdLineOK = 0;
    RunParams.mpirunName[0] = '\0';
    RunParams.maxCommunicators = NULL;
    RunParams.UseThreads = 0;
    RunParams.CheckArgs = 1;
    RunParams.OutputPrefix = 0;
    RunParams.Quiet = 0;
    RunParams.Verbose = 0;
    RunParams.HeartbeatPeriod = 37;
    RunParams.quadricsRailMask = 0;
    RunParams.quadricsHW = 0;
    RunParams.dbg.GDB = 0;
    RunParams.UseSSH = 0;
    RunParams.Local = 0;

    /* stdio input handling */
    RunParams.STDINfd = STDIN_FILENO;

    /* schedulers to try (determined at compile time) */

    RunParams.UseLSF = ENABLE_LSF;
    RunParams.UseBproc = ENABLE_BPROC;
    RunParams.UseRMS = ENABLE_RMS;

    /* find an available scheduler, and make sure only one is enabled */

    if (RunParams.UseBproc) {
        char *val;
        if (lampi_environ_find_string("NODES", &val) ==
            LAMPI_ENV_ERR_NOT_FOUND) {
            /* disable Bproc */
            RunParams.UseBproc = 0;
        } else {
            /* disable other schedulers */
            RunParams.UseLSF = 0;
            RunParams.UseRMS = 0;
        }
    }

    if (RunParams.UseRMS) {
        int val;
        if (lampi_environ_find_integer("RMS_NNODES", &val) ==
            LAMPI_ENV_ERR_NOT_FOUND) {
            /* disable RMS */
            RunParams.UseRMS = 0;
        } else {
            /* disable other schedulers */
            RunParams.UseBproc = 0;
            RunParams.UseLSF = 0;
        }
    }

    if (RunParams.UseLSF) {
        char *val;
        if (lampi_environ_find_string("LSB_MCPU_HOSTS", &val) ==
            LAMPI_ENV_ERR_NOT_FOUND) {
            /* disable LSF */
            RunParams.UseLSF = 0;
        } else {
            /* disable other schedulers */
            RunParams.UseBproc = 0;
            RunParams.UseRMS = 0;

            /* get LSF resources */
            GetLSFResource();
        }
    }

    RunParams.UseCRC = 0;
    if (ENABLE_RELIABILITY) {
        RunParams.quadricsDoAck = 1;
        RunParams.quadricsDoChecksum = 1;
#if ENABLE_GM
        RunParams.Networks.GMSetup.doAck = 1;
        RunParams.Networks.GMSetup.doChecksum = 1;
#endif
    } else {
        RunParams.quadricsDoAck = 0;
        RunParams.quadricsDoChecksum = 0;
    }

    /* check to see if started under a debugger */
    if (MPIR_being_debugged) {
        GetDebugDefault(NULL);
    } else {
        RunParams.dbg.Spawned = 0;
        RunParams.dbg.WaitInDaemon = 0;
    }

#if ENABLE_SHARED_MEMORY
    RunParams.Networks.UseSharedMemory = 1;
    RunParams.Networks.SharedMemSetup.PagesPerProc = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        outOfResourceAbort = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.retries_m = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        minPagesPerContext_m = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        maxPagesPerContext_m = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.maxTotalPages_m =
        NULL;
#endif

#if ENABLE_INFINIBAND
    RunParams.Networks.IBSetup.ack = 1;
    RunParams.Networks.IBSetup.checksum = 0;
#endif

    /* are we in charge of standard I/O redirection? */
    RunParams.handleSTDio = RunParams.UseRMS ? 0 : 1;

    /* get input data - not parsed at this stage */
    ScanInput(argc, argv, FirstAppArg);

    /* process input */
    for (int i = 0; i < SizeOfOptions; i++) {
        Options[i].fpToExecute(Options[i].FileName);
    }
    RunParams.CmdLineOK = 1;

    /* Allocate space to hold list of Daemon processes */
    RunParams.DaemonPIDs = ulm_new(pid_t, RunParams.NHosts);

    /* disallow network setup if only 1 host */
    if (!(RunParams.UseRMS) && RunParams.NHosts == 1) {
        RunParams.NPathTypes[0] = 0;
    }

    /* Verify the hosts and process counts are allowed in LSF */
    if (RunParams.UseLSF) {
        VerifyLsfResources();
    }

    /* initialize network parameters */

    RunParams.Networks.UseSharedMemory = 0;
    RunParams.Networks.UseUDP = 0;
    RunParams.Networks.UseTCP = 0;
    RunParams.Networks.UseGM = 0;
    RunParams.Networks.UseQSW = 0;
    RunParams.Networks.UseIB = 0;

#if ENABLE_SHARED_MEMORY
    RunParams.Networks.UseSharedMemory = 1;
    RunParams.Networks.SharedMemSetup.PagesPerProc = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        outOfResourceAbort = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.retries_m = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        minPagesPerContext_m = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.
        maxPagesPerContext_m = NULL;
    RunParams.Networks.SharedMemSetup.recvFragResources_m.maxTotalPages_m =
        NULL;
#endif

#if ENABLE_GM
    RunParams.Networks.GMSetup.NGMHosts = 0;
#endif

#if ENABLE_INFINIBAND
    RunParams.Networks.IBSetup.NIBHosts = 0;
#endif

    for (int j = 0; j < RunParams.NHosts; j++) {
        for (int i = 0; i < RunParams.NPathTypes[j]; i++) {
            switch (RunParams.ListPathTypes[j][i]) {
#if ENABLE_UDP
            case PATH_UDP:     // UDP interface
                RunParams.Networks.UseUDP = 1;
                break;
#endif
#if ENABLE_TCP
            case PATH_TCP:     // TCP interface
                RunParams.Networks.UseTCP = 1;
                break;
#endif
#if ENABLE_GM
            case PATH_GM:
                RunParams.Networks.UseGM = 1;
                RunParams.Networks.GMSetup.NGMHosts++;
                break;
#endif
#if ENABLE_INFINIBAND
            case PATH_IB:
                RunParams.Networks.UseIB = 1;
                (RunParams.Networks.IBSetup.NIBHosts)++;
                break;
#endif
            case PATH_QUADRICS:
                RunParams.Networks.UseQSW = 0;
                RunParams.handleSTDio = 0;
                break;
            }
        }
    }

#if ENABLE_NUMA
    RunParams.CpuList = NULL;
    RunParams.CpuListLen = 0;
    RunParams.nCpusPerNode = 0;
#endif                          // ENABLE_NUMA

    return 0;
}
