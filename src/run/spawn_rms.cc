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
#include <sys/stat.h>
#include <sys/wait.h>

#include "internal/profiler.h"
#include "client/adminMessage.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/RunParams.h"

/* indices into the exec args */
enum {
    PRUN = 0,
    NODES = 1,
    PROCS = 2,
    RMS_TAG = 3,
    RMS_SPAWN = 4,
    PROG_NAME = 5,
    PROG_ARGS = 6
};

/* indices into the admin. environment variables */
enum {
    IP_ADDR = 0,
    PORT = 1,
    AUTH0 = 2,
    AUTH1 = 3,
    AUTH2 = 4,
    ADMIN_END = 5
};


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


int SpawnRms(unsigned int *AuthData, int port, int argc, char **argv)
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
    adminMessage *server;

    server = RunParams.server;
    /* set up environment variables */
    if (RunParams.nEnvVarsToSet > 0) {
        env_vars = (char **)
            ulm_malloc((RunParams.nEnvVarsToSet + 5) * sizeof(char *));
    }

    /* environment variables from RunParams */
    for (i = 0; i < RunParams.nEnvVarsToSet; i++) {
        env_vars[i] = (char *)
            ulm_malloc(strlen(RunParams.envVarsToSet[i].var_m)
                       + strlen(RunParams.envVarsToSet[i].envString_m[0])
                       + 2);
        sprintf(env_vars[i], "%s=%s",
                RunParams.envVarsToSet[i].var_m,
                RunParams.envVarsToSet[i].envString_m[0]);
    }

    /* put the variables into the environment */
    for (i = 0; i < RunParams.nEnvVarsToSet; i++) {
        if (putenv(env_vars[i]) != 0) {
            perror("Error trying to set env. vars");
            return -1;
        }
    }

    /* set up environment variables for admin. message */
    admin_vars = (char **) ulm_malloc(ADMIN_END * sizeof(char *));
    admin_vars[IP_ADDR] = (char *) ulm_malloc(strlen("LAMPI_ADMIN_IP=")
                                              +
                                              strlen(RunParams.
                                                     mpirunName) + 1);
    sprintf(admin_vars[IP_ADDR], "LAMPI_ADMIN_IP=%s",
            RunParams.mpirunName);

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
#if defined(__osf__) && defined(UGLY_FIX_FOR_BROKEN_RMSLOADER)
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
    exec_args = (char **) ulm_malloc((PROG_ARGS + argc)
                                     * sizeof(char *));
    for (i = 0; i < PROG_ARGS + argc; i++) {
        exec_args[i] = 0;
    }

    /*
     * set up exec args
     */

    /* Prun */
    exec_args[PRUN] = (char *) ulm_malloc(5);
    sprintf(exec_args[PRUN], "prun");

    /* number of hosts */
    sprintf(buf, "%d", RunParams.NHosts);
    if (RunParams.NHostsSet) {
        exec_args[NODES] = (char *) ulm_malloc(sizeof(buf) + 4);
        sprintf(exec_args[NODES], "-N %s", buf);
    } else {
        skip_arg_count++;
    }

    /* number of processors */
    nprocs = 0;
    for (i = 0; i < RunParams.NHosts; i++) {
        nprocs += RunParams.ProcessCount[i];
    }
    sprintf(buf, "%d", nprocs);
    exec_args[PROCS - skip_arg_count] =
        (char *) ulm_malloc(sizeof(buf) + 4);
    sprintf(exec_args[PROCS - skip_arg_count], "-n %s", buf);

    if (0) {                    // DEBUG
        printf(">>>>> NHosts = %d, NHostsSet = %d, nprocs = %d\n",
               RunParams.NHosts, RunParams.NHostsSet, nprocs);
        printf(">>>>> ProcessCount =");
        for (i = 0; i < RunParams.NHosts; i++) {
            printf(" %d", RunParams.ProcessCount[i]);
        }
        printf("\n");
        fflush(stdout);
    }

    /* rms flag to tag stdout/stderr with process id */
    if (RunParams.OutputPrefix) {
        exec_args[RMS_TAG - skip_arg_count] =
            (char *) ulm_malloc(strlen("-t") + 1);
        sprintf(exec_args[RMS_TAG - skip_arg_count], "-t");
    } else {
        skip_arg_count++;
    }

    /* rms flag to spawn one proc per node */
    exec_args[RMS_SPAWN - skip_arg_count] = (char *)
        ulm_malloc(strlen("-Rone-process-per-node,rails=XX,railmask=XX") +
                   1);
    if (RunParams.quadricsRailMask) {
        sprintf(exec_args[RMS_SPAWN - skip_arg_count],
                "-Rone-process-per-node,rails=%d,railmask=%d",
                quadricsNRails(RunParams.quadricsRailMask),
                RunParams.quadricsRailMask);
    } else {
        sprintf(exec_args[RMS_SPAWN - skip_arg_count],
                "-Rone-process-per-node");
    }

    /* program name */
    exec_args[PROG_NAME - skip_arg_count] = (char *)
        ulm_malloc(strlen(RunParams.ExeList[0]) + 1);
    sprintf(exec_args[PROG_NAME - skip_arg_count], "%s",
            RunParams.ExeList[0]);

    /* program arguments */
    p = &exec_args[PROG_ARGS - skip_arg_count];
    q = &argv[0];

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
            for (i = 0; i < PROG_ARGS + argc; i++) {
                if (exec_args[i]) {
                    ulm_free(exec_args[i]);
                }
            }
            ulm_free(exec_args);
            exec_args = 0;
        }

        if (env_vars) {
            for (i = 0; i < RunParams.nEnvVarsToSet; i++) {
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

    /* check if prun already returned with error */

    sleep(1);
    if (waitpid(-1, NULL, WNOHANG) == child) {
        return -1;
    }

    return 0;
}
