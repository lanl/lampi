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
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include "internal/profiler.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/RunParams.h"


#if ENABLE_LSF
extern "C" {
#include <lsf/lsf.h>
}
extern char **environ;
#endif

#ifndef NGROUPS_MAX
#define NGROUPS_MAX 20
#endif


static char **ExecArgs, **EnvList;
static int nEnviron;          /* number of env variables from environ */
static int NumEnvs;           /* total number of ENV variables to exe */
static int hostNumEnvs;
static int hostNumExecArgs;
static int *lsfTasks = 0;
static char Temp[ULM_MAX_CONF_FILELINE_LEN];


static bool extraGidsMatch(int n, gid_t grp, gid_t *list)
{
    bool result = false;

    for (int i = 0; i < n; i++) {
        if (list[i] == grp)
            result = true;
    }

    return result;
}


static bool canExecute(struct stat *rstat)
{
    bool result = false;
    uid_t realuid, effuid;
    gid_t realgid, effgid, extragids[NGROUPS_MAX];
    int numExtraGids = 0;

    realuid = getuid();
    effuid = geteuid();
    realgid = getgid();
    effgid = getegid();

    if ((numExtraGids = getgroups(NGROUPS_MAX, extragids)) < 0)
        numExtraGids = 0;

    if ((realuid == rstat->st_uid) || (effuid == rstat->st_uid)) {
        if (rstat->st_mode & S_IXUSR)
            result = true;
    }
    else if ((realgid == rstat->st_gid) || (effgid == rstat->st_gid) || 
             extraGidsMatch(numExtraGids, rstat->st_gid, extragids)) {
        if (rstat->st_mode & S_IXGRP)
            result = true;
    } 
    else if (rstat->st_mode & S_IXOTH) {
        result = true;
    }
    
    return result;
}


static void handleLsfTasks(int signo)
{
#if ENABLE_LSF
    int i, taskID;
    LS_WAIT_T status;

    if (!lsfTasks || (signo != SIGUSR1))
        return;

    do {
        taskID = ls_rwait(&status, WNOHANG, (struct rusage *)NULL);

        if (taskID <= 0)
            return;

        for (i = 0; i < RunParams.NHosts ; i++) {
            if (taskID == lsfTasks[i]) {
                lsfTasks[i] = -1;
                break;
            }
        }
    } while (1);
#endif

    return;
}


static void AllocateAndFillEnv(int idx, char *data)
{
    int len;

    len = strlen(data) + 1;
    EnvList[idx] = ulm_new(char, len);
    bzero(EnvList[idx], len);
    strcpy(EnvList[idx], data);
}


static void HostBuildEnvList(int host, char *localHostName,
                             unsigned int *AuthData, int ReceivingSocket)
{
    int idx, envVar = 0;

    hostNumEnvs = NumEnvs;
    if (RunParams.nEnvVarsToSet > 0) {     /* Env Var from command line */
        for (envVar = 0; envVar < RunParams.nEnvVarsToSet; envVar++) {
            if (RunParams.envVarsToSet[envVar].setForAllHosts_m ||
                RunParams.envVarsToSet[envVar].
                setForThisHost_m[host]) {
                hostNumEnvs++;
            }
        }
    }

    /*
     * Allocate space for Env list.
     */
    EnvList = ulm_new( char *, hostNumEnvs);
    EnvList[hostNumEnvs - 1] = NULL;


    /*
     * Allocate and fill in all the individual environment variables.
     */

#if ENABLE_LSF
    for (envVar = 0; envVar < nEnviron; envVar++) {
        AllocateAndFillEnv(envVar, environ[envVar]);
    }
#endif
	
    sprintf(Temp, "LAMPI_ADMIN_AUTH0=%u", AuthData[0]);
    AllocateAndFillEnv(envVar++, Temp);
    sprintf(Temp, "LAMPI_ADMIN_AUTH1=%u", AuthData[1]);
    AllocateAndFillEnv(envVar++, Temp);
    sprintf(Temp, "LAMPI_ADMIN_AUTH2=%u", AuthData[2]);
    AllocateAndFillEnv(envVar++, Temp);
    sprintf(Temp, "LAMPI_ADMIN_PORT=%d", ReceivingSocket);
    AllocateAndFillEnv(envVar++, Temp);
    sprintf(Temp, "LAMPI_ADMIN_IP=%s", localHostName);
    AllocateAndFillEnv(envVar++, Temp);

    sprintf(Temp, "PWD=%s", RunParams.WorkingDirList[host]);
    AllocateAndFillEnv(envVar++, Temp);
    sprintf(Temp, "%s", "LAMPI_WITH_LSF=1");
    AllocateAndFillEnv(envVar++, Temp);

    if (hostNumEnvs > NumEnvs) {
        for (idx = 0; idx < RunParams.nEnvVarsToSet; idx++) {
            if (RunParams.envVarsToSet[idx].setForAllHosts_m ||
                RunParams.envVarsToSet[idx].setForThisHost_m[host]) {
                sprintf(Temp, "%s=%s",
                        RunParams.envVarsToSet[idx].var_m,
                        RunParams.envVarsToSet[idx].envString_m[0]);
                AllocateAndFillEnv(envVar++, Temp);
            }
        }

    }
}


static void HostBuildExecArg(int host, int argc, char **argv)
{
    int len, idx, aIdx;

    /*
     * Number of exec args contains binary execuable, number of arguments,
     * Null entry at end of list.
     */

    hostNumExecArgs = argc + 2;

    ExecArgs = ulm_new( char *, hostNumExecArgs);
    ExecArgs[hostNumExecArgs - 1] = NULL;

    len = strlen(RunParams.ExeList[host]) + 1;
    ExecArgs[0] = ulm_new(char, len);
    bzero(ExecArgs[0], len);
    sprintf(ExecArgs[0], "%s", RunParams.ExeList[host]);

    for (idx = 0, aIdx = 1; idx < argc; idx++, aIdx++) {
        len = strlen(argv[idx]) + 1;
        ExecArgs[aIdx] = ulm_new(char, len);
        bzero(ExecArgs[aIdx], len);
        sprintf(ExecArgs[aIdx], "%s", argv[idx]);
    }
}


/*
 * Use LSF to spawn master process on remote host.  This routine also
 * connects standard out and standard error to one end of a pipe - one
 * per host - so that when a connection is established with the remote
 * host, stdout and stderr from the remote hosts can be read by
 * ULMRun, and processed.  ULMRun in turn will redirect this output to
 * its own stdout and stderr files - in this case the controlling tty.
 */
int SpawnLsf(unsigned int *AuthData, int ReceivingSocket, 
             int **ListHostsStarted,
             int argc, char **argv)
{
    char LocalHostName[ULM_MAX_HOSTNAME_LEN + 1];
    int idx, host;
    struct sigaction action;
    int NHostsStarted=0;
#if ENABLE_LSF
    int RetVal;
    struct stat rstat;

    if (ls_initrex(RunParams.NHosts, 0) < 0) {
        ls_perror("ls_initrex");
        Abort();
    }
#else
    printf("Trying to use LSF without LSF compiled in.\n");
    Abort();
#endif

    lsfTasks = ulm_new(int, RunParams.NHosts);
    for (host = 0; host < RunParams.NHosts; host++) {
        lsfTasks[host] = -1;
    }

    action.sa_handler = handleLsfTasks;
    sigfillset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGUSR1, &action, (struct sigaction *)NULL) < 0) {
        ulm_err(("Error: Can't set SIGUSR1 handler to handleLsfTasks!\n"));
        Abort();
    }

    nEnviron = 0;
#if ENABLE_LSF
    /*
     * Pre-calculate the number of entires and maxi space needed for
     * the environment variable list that global to all hosts.
     */
    for (nEnviron = 0; environ[nEnviron] != NULL; nEnviron++);
#endif /* ENABLE_LSF */

    NumEnvs = nEnviron;
    NumEnvs += 7;               /* 3 for auth, 1 for working dir, 1 for ENABLE_LSF */
    NumEnvs++;                  /* 1 for NULL at end of *EnvList */

    /* copy the local hostname into LocalHostName as the official local name...
     */
    strncpy(LocalHostName, RunParams.mpirunName,
            ULM_MAX_HOSTNAME_LEN);

    /* list of hosts for which the ULMRun fork() succeeded - needed for
     *  cleanup after abnormal termination.
     */
    *ListHostsStarted = ulm_new(int, RunParams.NHosts);

    /* save stdin for forwarding to proc rank 0 */
    RunParams.STDINfd = dup(STDIN_FILENO);

    /* connect stdin to /dev/null */
    int fd = open("/dev/null", 0, 0);
    if(fd >= 0) {
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    /*
     * Loop through the NHosts and spawn remote job by using ls_rtask().
     */

    for (host = 0; host < RunParams.NHosts; host++) {
        HostBuildEnvList(host, LocalHostName, AuthData, ReceivingSocket);
        HostBuildExecArg(host, argc, argv);

#if ENABLE_LSF
        if ((RetVal = ls_chdir(RunParams.HostList[host],
                               RunParams.WorkingDirList[host])) < 0) {
            ulm_err(("Error: can't LSF remote change directory, ls_chdir(\"%s\",\"%s\") returned %d lserrno %d (error: %s)!\n", 
                     RunParams.HostList[host], RunParams.WorkingDirList[host], RetVal, lserrno, ls_sysmsg()));
            Abort();
        }

        if (ls_rstat(RunParams.HostList[host],
                     RunParams.ExeList[host], &rstat) < 0) {
            ulm_err(("Error: LSF remote stat of executable %s on host %s (error: %s)\n",
                     RunParams.ExeList[host], RunParams.HostList[host], strerror(errno)));
            Abort();
        }
        else if (!S_ISREG(rstat.st_mode)) {
            ulm_err(("Error: LSF remote stat of executable %s on host %s shows that the file is not a regular file!\n",
                     RunParams.ExeList[host], RunParams.HostList[host]));
            Abort();
        }
        else if (!canExecute(&rstat)) {
            ulm_err(("Error: LSF remote stat of executable %s on host %s -- no execute permission!\n",
                     RunParams.ExeList[host], RunParams.HostList[host]));
            Abort();
        }

        if ((lsfTasks[host] = ls_rtaske(RunParams.HostList[host], ExecArgs,
                                        REXF_CLNTDIR, EnvList)) < 0) {
            ulm_err(("Error: can't start job on host %s\n",
                     RunParams.HostList[host]));
            Abort();
        }
#endif

        (*ListHostsStarted)[NHostsStarted] = host;
        NHostsStarted++;

        // free user app argument list
        for (idx = 0; idx < (hostNumExecArgs - 1); idx++) {
            ulm_delete(ExecArgs[idx]);
        }
        for (idx = 0; idx < (hostNumEnvs - 1); idx++) {
            ulm_delete(EnvList[idx]);
        }
        ulm_delete(ExecArgs);
        ulm_delete(EnvList);
    }                           /* end of for loop for each remote host */
    return 0;
}
