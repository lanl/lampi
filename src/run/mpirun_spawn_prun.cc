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
#include <sys/stat.h>
#include <sys/wait.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "client/adminMessage.h"
#include "run/JobParams.h"
#include "internal/log.h"

extern int ULMRunSpawnedClients;

/* indices into the exec args */
enum {
    PRUN,
    NODES,
    PROCS,
    RMS_TAG,
    RMS_SPAWN,
    PROG_NAME,
    PROG_ARGS
};

/* indices into the admin. environment variables */
enum {
    IP_ADDR,
    PORT,
    AUTH0,
    AUTH1,
    AUTH2,
    ADMIN_END
};


//void fix_RunParameters(ULMRunParams_t *RunParameters, int nhosts)
//{
//    int errorCode,tag;
//    int contacted = 0;
//
//    // free and reallocate memory for HostList if needed
//    if (!RunParameters->HostList || (nhosts != RunParameters->HostListSize)) {
//        if (RunParameters->HostList) {
//            ulm_delete(RunParameters->HostList);
//        }
//        RunParameters->HostList = ulm_new(HostName_t,  nhosts);
//    }
//    // get the new HostList info from the server adminMessage object
//    for (int i = 0; i < nhosts; i++) {
//        if (!server->
//            peerName(i, RunParameters->HostList[i],
//                     ULM_MAX_HOSTNAME_LEN)) {
//            ulm_err(("server could not get remote IP hostname/address for hostrank %d\n", i));
//            Abort();
//        }
//    }
//
//    // resize ProcessCount and DaemonPIDs arrays and get info from other hosts
//    if (nhosts != RunParameters->NHosts) {
//        ulm_delete(RunParameters->ProcessCount);
//        ulm_delete(RunParameters->DaemonPIDs);
//
//        RunParameters->ProcessCount = ulm_new(int, nhosts);
//        RunParameters->DaemonPIDs = ulm_new(pid_t,  nhosts);
//    }
//
//    server->reset(adminMessage::SEND);
//    tag=adminMessage::NHOSTS;
//    server->pack(&tag, (adminMessage::packType)sizeof(int), 1);
//    server->pack(&nhosts, (adminMessage::packType)sizeof(int), 1);
//    tag=adminMessage::THREADUSAGE;
//    server->pack(&tag, (adminMessage::packType)sizeof(int), 1);
//    server->pack(&(RunParameters->UseThreads), (adminMessage::packType)sizeof(bool), 1);
//    tag=adminMessage::TVDEBUG;
//    server->pack(&tag, (adminMessage::packType)sizeof(int), 1);
//    server->pack(&(RunParameters->TVDebug), (adminMessage::packType)sizeof(int), 1);
//    server->pack(&(RunParameters->TVDebugApp), (adminMessage::packType)sizeof(int), 1);
//    tag=adminMessage::CRC;
//    server->pack(&tag, (adminMessage::packType)sizeof(int), 1);
//    server->pack(&(RunParameters->UseCRC), (adminMessage::packType)sizeof(bool), 1);
//    if (!server->broadcast(adminMessage::RUNPARAMS, &errorCode)) {
//        ulm_err(("Error: Can't broadcast RUNPARAMS message (error %d)\n",
//                 errorCode));
//        Abort();
//    }
//    // now receive the information from the other hosts
//    while (contacted < nhosts) {
//        int rank, tag;
//        int recvd = server->receiveFromAny(&rank, &tag, &errorCode,
//                                           ALARMTIME * 1000);
//        switch (recvd) {
//        case (adminMessage::OK):
//            if ((tag != adminMessage::RUNPARAMS) || (rank >= nhosts)) {
//                ulm_err(("RUNPARAMS receiveFromAny(%d, %d) invalid rank or tag\n", rank, tag));
//                Abort();
//            }
//            server->unpack(&(RunParameters->ProcessCount[rank]),
//                           (adminMessage::packType) sizeof(int), 1);
//            server->unpack(&(RunParameters->DaemonPIDs[rank]),
//                           (adminMessage::packType) sizeof(pid_t), 1);
//            contacted++;
//            break;
//        default:
//            ulm_err(("RUNPARAMS receiveFromAny() returned code %d error %d\n", recvd, errorCode));
//            Abort();
//            break;
//        }
//    }
//
//    // resize and copy the first element from the following lists: DirName_t WorkingDirList[], ExeName_t ExeList[],
//    // DirName_t UserAppDirList[], int NPathTypes[], int *ListPathTypes[] if necessary
//    if (RunParameters->NHosts < nhosts) {
//        DirName_t *wdl = RunParameters->WorkingDirList;
//        ExeName_t *el = RunParameters->ExeList;
//        DirName_t *uadl = RunParameters->UserAppDirList;
//        int *nndt = RunParameters->NPathTypes;
//        int **lndt = RunParameters->ListPathTypes;
//
//        RunParameters->WorkingDirList = ulm_new(DirName_t,  nhosts);
//        RunParameters->ExeList = ulm_new(ExeName_t,  nhosts);
//        RunParameters->NPathTypes = ulm_new(int, nhosts);
//        RunParameters->ListPathTypes = ulm_new( int *, nhosts);
//        if (uadl) {
//            RunParameters->UserAppDirList = ulm_new(DirName_t,  nhosts);
//        }
//
//        for (int i = 0; i < nhosts; i++) {
//            strncpy(RunParameters->WorkingDirList[i], wdl[0],
//                    ULM_MAX_PATH_LEN);
//            strncpy(RunParameters->ExeList[i], el[0], ULM_MAX_PATH_LEN);
//            if (uadl) {
//                strncpy(RunParameters->UserAppDirList[i], uadl[0],
//                        ULM_MAX_PATH_LEN);
//            }
//            RunParameters->NPathTypes[i] = nndt[0];
//            RunParameters->ListPathTypes[i] =
//                ulm_new(int, nndt[0]);
//            for (int j = 0; j < nndt[0]; j++) {
//                RunParameters->ListPathTypes[i][j] = lndt[0][j];
//            }
//        }
//
//        ulm_delete(wdl);
//        ulm_delete(el);
//        if (uadl) {
//            ulm_delete(uadl);
//        }
//        for (int i = 0; i < RunParameters->NHosts; i++) {
//            ulm_delete(lndt[i]);
//        }
//        ulm_delete(lndt);
//        ulm_delete(nndt);
//    }
//    // disallow network setup if only one host
//    if (nhosts == 1) {
//        RunParameters->NPathTypes[0] = 0;
//    }
//    // send network device types to each host
//    else {
//        for (int i = 0; i < nhosts; i++) {
//            if (RunParameters->NPathTypes[i] < 1) {
//                ulm_err(("At least one network device type must be specified (host %d nhosts %d)!\n", i, nhosts));
//                Abort();
//            }
//            server->reset(adminMessage::SEND);
//            server->pack(&(RunParameters->NPathTypes[i]),
//                         (adminMessage::packType) sizeof(int), 1);
//            server->pack(RunParameters->ListPathTypes[i],
//                         (adminMessage::packType) sizeof(int),
//                         RunParameters->NPathTypes[i]);
//            if (!server->send(i, adminMessage::NETDEVS, &errorCode)) {
//                ulm_err(("Error: Can't send NETDEVS message (error %d)\n",
//                         errorCode));
//                Abort();
//            }
//        }
//    }
//
//    // set the number of hosts to the true known number...
//    RunParameters->NHosts = nhosts;
//            
//    // send end of input marker
//    server->reset(adminMessage::SEND);
//    if( !server->broadcast(adminMessage::ENDRUNPARAMS, &errorCode) ) {
//        ulm_err(("Error: Can't broadcast ENDRUNPARAMS message (error %d)\n",
//                 errorCode));
//        Abort();
//    }
//}

static int quadricsNRails(int railmask)
{
    int result = 0;

    while (railmask) {
        if (railmask & 1) {
            result++;
        }
        railmask >>= 1;
    }

    return result;
}

int mpirun_spawn_prun(unsigned int *AuthData, int port,
                      ULMRunParams_t *RunParameters,
                      int FirstAppArgument, int argc, char **argv)
{
    char **exec_args;
    char **env_vars = 0;
    char **admin_vars;
    char **p, **q;
    int nprocs;
    int i, n;
    int child;
    int skip_arg_count = 0;
    char buf[512];
	adminMessage	*server;
	
	server = RunParameters->server;
    /* set up environment variables */
    if (RunParameters->nEnvVarsToSet > 0) {
        env_vars = (char **)
            ulm_malloc((RunParameters->nEnvVarsToSet +
                        5) * sizeof(char *));
    }

    /* environment variables from RunParameters */
    for (i = 0; i < RunParameters->nEnvVarsToSet; i++) {
        env_vars[i] = (char *)
            ulm_malloc(strlen(RunParameters->envVarsToSet[i].var_m)
                       +
                       strlen(RunParameters->envVarsToSet[i].
                              envString_m[0])
                       + 2);
        sprintf(env_vars[i], "%s=%s",
                RunParameters->envVarsToSet[i].var_m,
                RunParameters->envVarsToSet[i].envString_m[0]);
    }

    /* put the variables into the environment */
    for (i = 0; i < RunParameters->nEnvVarsToSet; i++) {
        if (putenv(env_vars[i]) != 0) {
            perror("Error trying to set env. vars");
            return -1;
        }
    }

    /* set up environment variables for admin. message */
    admin_vars = (char **) ulm_malloc(ADMIN_END * sizeof(char *));
    admin_vars[IP_ADDR] = (char *) ulm_malloc(strlen("LAMPI_ADMIN_IP=")
                                              +
                                              strlen(RunParameters->
                                                     mpirunName) + 1);
    sprintf(admin_vars[IP_ADDR], "LAMPI_ADMIN_IP=%s",
            RunParameters->mpirunName);

    sprintf(buf, "%d", port);
    admin_vars[PORT] = (char *) ulm_malloc(strlen("LAMPI_ADMIN_PORT=")
                                           + strlen(buf) + 1);
    sprintf(admin_vars[PORT], "LAMPI_ADMIN_PORT=%s", buf);

    sprintf(buf, "%d", AuthData[0]);
    admin_vars[AUTH0] = (char *) ulm_malloc(strlen("LAMPI_ADMIN_AUTH0=")
                                            + strlen(buf) + 1);
    sprintf(admin_vars[AUTH0], "LAMPI_ADMIN_AUTH0=%s", buf);

    sprintf(buf, "%d", AuthData[1]);
    admin_vars[AUTH1] = (char *) malloc(strlen("LAMPI_ADMIN_AUTH1=")
                                        + strlen(buf) + 1);
    sprintf(admin_vars[AUTH1], "LAMPI_ADMIN_AUTH1=%s", buf);

    sprintf(buf, "%d", AuthData[2]);
    admin_vars[AUTH2] = (char *) ulm_malloc(strlen("LAMPI_ADMIN_AUTH2=")
                                            + strlen(buf) + 1);
    sprintf(admin_vars[AUTH2], "LAMPI_ADMIN_AUTH2=%s", buf);

    /* put the admin. variables in the environment */
    for (i = 0; i < ADMIN_END; i++) {
        if (putenv(admin_vars[i]) != 0) {
            perror("Error trying to set admin. env. vars");
            return -1;
        }
    }

/* BEGIN: ugly fix for broken rmsloader on certain Q systems */ 
#define UGLY_FIX_FOR_BROKEN_RMSLOADER
#if defined(__osf__) and defined(UGLY_FIX_FOR_BROKEN_RMSLOADER)
    struct stat statbuf;

    if (stat("/usr/local/compaq/test/rmsloader", &statbuf) == 0) {
        putenv("RMS_PATH=/usr/local/compaq/test");
        if (getenv("PRINT_RMS")) {
            system("env | grep RMS_ | sort");
        }
    }
#endif
/* END: ugly fix for broken rmsloader on certain Q systems */ 


    /* allocate array of exec arguments */
    exec_args = (char **) ulm_malloc((PROG_ARGS + argc - FirstAppArgument)
                                     * sizeof(char *));
    for (i = 0; i < PROG_ARGS + argc - FirstAppArgument; i++) {
        exec_args[i] = 0;
    }

    /*
     * set up exec args
     */

    /* Prun */
    exec_args[PRUN] = (char *) ulm_malloc(5);
    sprintf(exec_args[PRUN], "prun");

    /* number of hosts */
    sprintf(buf, "%d", RunParameters->NHosts);
    if (RunParameters->NHostsSet) {
        exec_args[NODES] = (char *) ulm_malloc(sizeof(buf) + 4);
        sprintf(exec_args[NODES], "-N %s", buf);
    } else {
        skip_arg_count++;
    }

    /* number of processors */
    nprocs = 0;
    for (i = 0; i < RunParameters->NHosts; i++) {
        nprocs += RunParameters->ProcessCount[i];
    }
    memset(buf, 0, 512);
    sprintf(buf, "%d", nprocs);
    exec_args[PROCS - skip_arg_count] =
        (char *) ulm_malloc(sizeof(buf) + 4);
    sprintf(exec_args[PROCS - skip_arg_count], "-n %s", buf);

    /* rms flag to tag stdout/stderr with process id */
    if (RunParameters->OutputPrefix) {
        exec_args[RMS_TAG - skip_arg_count] =
            (char *) ulm_malloc(strlen("-t") + 1);
        sprintf(exec_args[RMS_TAG - skip_arg_count], "-t");
    } else {
        skip_arg_count++;
    }
    
    /* rms flag to spawn one proc per node */
    exec_args[RMS_SPAWN - skip_arg_count] = (char *)
        ulm_malloc(strlen("-Rone-process-per-node,rails=XX,railmask=XX") + 1);
    if (RunParameters->quadricsRailMask) {
        sprintf(exec_args[RMS_SPAWN - skip_arg_count],
                "-Rone-process-per-node,rails=%d,railmask=%d", 
                quadricsNRails(RunParameters->quadricsRailMask),
                RunParameters->quadricsRailMask);
    }
    else {
        sprintf(exec_args[RMS_SPAWN - skip_arg_count],
                "-Rone-process-per-node");
    }

    /* program name */
    exec_args[PROG_NAME - skip_arg_count] = (char *)
        ulm_malloc(strlen(RunParameters->ExeList[0]) + 1);
    sprintf(exec_args[PROG_NAME - skip_arg_count], "%s",
            RunParameters->ExeList[0]);

    /* program arguments */
    p = &exec_args[PROG_ARGS - skip_arg_count];
    q = &argv[FirstAppArgument];

    while (*q) {
        *p = (char *) ulm_malloc(1 + strlen(*q));
        sprintf(*p++, "%s", *q++);
    }
    *p = NULL;

    /*
     * Launch prun in a child process
     */
    if ((child = fork()) == -1) {       /* error */
        perror("error forking child for prun");
        return -1;
    } else if (child == 0) {
        delete server;
        n = execvp(exec_args[0], exec_args);
        if (n < 0) {
            perror("Exec error");
        }
    } else {                    /* parent */
        /* clean up */
        if (exec_args) {
            for (i = 0; i < PROG_ARGS + argc - FirstAppArgument; i++) {
                if (exec_args[i]) {
                    ulm_free(exec_args[i]);
                }
            }
            ulm_free(exec_args);
            exec_args = 0;
        }

        if (env_vars) {
            for (i = 0; i < RunParameters->nEnvVarsToSet; i++) {
                if (env_vars[i]) {
                    ulm_free(env_vars[i]);
                }
            }
            ulm_free(env_vars);
            env_vars = 0;
        }

        if (admin_vars) {
            for (i = 0; i < ADMIN_END; i++) {
                if (admin_vars[i]) {
                    ulm_free(admin_vars[i]);
                }
            }
            ulm_free(admin_vars);
            admin_vars = 0;
        }
        
    }

    return 0;
}
