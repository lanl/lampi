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
#include <netinet/in.h>
#include <netdb.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "run/JobParams.h"

/*
 * Use RSH to spawn master process on remote host.
 *   This routine also connects standard
 *   out and standard error to one end of a pipe - one per host - so that
 *   when a connection is established with the remote host, stdout and stderr
 *   from the remote hosts can be read by ULMRun, and processed.  ULMRun
 *   in turn will redirect this output to its own stdout and stderr files -
 *   in this case the controlling tty.
 */

/* indices into the exec args */
enum {
    RSH,
    HOSTNAME,
    CD,
    LAMPID,
    NPROCS,
    SOCK_IP,
    CNTL_PORT,
    STDERR_PORT,
    STDOUT_PORT,
    PROG_NAME,
    PROG_ARGS,
    ENV_VARS,
    END
};


int mpirun_spawn_rsh(unsigned int *AuthData, char *sock_ip,
                     int *NHostsStarted, int **ListHostsStarted,
                     ULMRunParams_t *RunParameters, int FirstAppArgument,
                     int argc, char **argv)
{
    int i;
    int j;
    int ret_status = 0;
    size_t len;
    pid_t child;
    char **exec_args;
    char buf[512];
    link_t *l;
    host_sock_info *hi;

    /* begin setup of exec args */
    exec_args = (char **) ulm_malloc((END + 1) * sizeof(char *));
    exec_args[END] = NULL;

    /*
     * fill in 'constants' in the exec_args
     */

    /* rsh */
    exec_args[RSH] = (char *) ulm_malloc(4);
    sprintf(exec_args[RSH], "rsh");

    /* lampid */
    exec_args[LAMPID] = (char *) ulm_malloc(1 + strlen("./lampid"));
    sprintf(exec_args[LAMPID], "./lampid");

    /* socket ip_address */
    exec_args[SOCK_IP] = (char *) ulm_malloc(4 + strlen(sock_ip));
    sprintf(exec_args[SOCK_IP], "-i %s", sock_ip);

    /* program arguments */
    if (argc - FirstAppArgument <= 0) {
        exec_args[PROG_ARGS] = (char *) ulm_malloc(2);
        sprintf(exec_args[PROG_ARGS], " ");
    } else {
        len = 0;
        for (i = FirstAppArgument; i < argc; i++) {
            len += strlen(argv[i]);
            len += 2;
        }
        exec_args[PROG_ARGS] = (char *) ulm_malloc(6 + len);
        strcat(exec_args[PROG_ARGS], "-a \"");
        for (i = FirstAppArgument; i < argc; i++) {
            strcat(exec_args[PROG_ARGS], " ");
            strcat(exec_args[PROG_ARGS], argv[i]);
            strcat(exec_args[PROG_ARGS], " ");
        }
        strcat(exec_args[PROG_ARGS], "\"");
    }

    /* loop over hosts and spawn jobs */
    *NHostsStarted = 0;
    l = host_sock_l;
    for (i = 0; i < RunParameters->NHosts; i++) {
        hi = (host_sock_info *) l->data;

        /*
         * finish filling in exec_args
         */

        /* working directory */
        exec_args[CD] = (char *)
            ulm_malloc(strlen(RunParameters->WorkingDirList[i])
                       + 5);
        sprintf(exec_args[CD], "cd %s;", RunParameters->WorkingDirList[i]);

        /* program name */
        exec_args[PROG_NAME] = (char *)
            ulm_malloc(strlen(RunParameters->ExeList[i])
                       + 4);
        sprintf(exec_args[PROG_NAME], "-p ");
        sprintf(exec_args[PROG_NAME] + 3, "%s", RunParameters->ExeList[i]);

        /* number of processes */
        sprintf(buf, "%d", RunParameters->ProcessCount[i]);
        exec_args[NPROCS] = (char *)
            ulm_malloc(strlen(buf) + 4);
        sprintf(exec_args[NPROCS], "-n %s", buf);

        /* control port */
        sprintf(buf, "%d", hi->cntl_port);
        exec_args[CNTL_PORT] = (char *) ulm_malloc(strlen(buf) + 4);
        sprintf(exec_args[CNTL_PORT], "-c %s", buf);

        /* stderr port */
        sprintf(buf, "%d", hi->stderr_port);
        exec_args[STDERR_PORT] = (char *) ulm_malloc(strlen(buf) + 4);
        sprintf(exec_args[STDERR_PORT], "-t %s", buf);

        /* stdout port */
        sprintf(buf, "%d", hi->stdout_port);
        exec_args[STDOUT_PORT] = (char *) ulm_malloc(strlen(buf) + 4);
        sprintf(exec_args[STDOUT_PORT], "-o %s", buf);

        /* environment variables */
        len = 0;
        for (j = 0; j < RunParameters->nEnvVarsToSet; j++) {
            len += 1;           /* ' ' */
            len += strlen(RunParameters->envVarsToSet[j].var_m);
            len += 1;           /* '=' */
            if (RunParameters->envVarsToSet[j].setForAllHosts_m) {
                len +=
                    strlen(RunParameters->envVarsToSet[j].envString_m[0]);
            } else {
                len +=
                    strlen(RunParameters->envVarsToSet[j].envString_m[j]);
            }
            len += 1;           /* ' ' */
        }

        exec_args[ENV_VARS] = (char *) ulm_malloc(len + 6);
        sprintf(exec_args[ENV_VARS], "-e \"");
        len = 4;
        for (j = 0; j < RunParameters->nEnvVarsToSet; j++) {
            strcat(exec_args[ENV_VARS], " ");
            strcat(exec_args[ENV_VARS],
                   RunParameters->envVarsToSet[j].var_m);
            strcat(exec_args[ENV_VARS], "=");
            if (RunParameters->envVarsToSet[j].setForAllHosts_m) {
                strcat(exec_args[ENV_VARS],
                       RunParameters->envVarsToSet[j].envString_m[0]);
                strcat(exec_args[ENV_VARS], " ");
            } else {
                strcat(exec_args[ENV_VARS],
                       RunParameters->envVarsToSet[j].envString_m[j]);
                strcat(exec_args[ENV_VARS], " ");
            }
        }
        strcat(exec_args[ENV_VARS], "\"");

        /* host name */
        exec_args[HOSTNAME] = (char *)
            ulm_malloc(strlen(RunParameters->HostList[i]) + 1);
        sprintf(exec_args[HOSTNAME], "%s", RunParameters->HostList[i]);

        /* fork off children to perform rsh */
        if ((child = fork()) == -1) {   /* error */
            perror("Error forking child for rsh");
            Abort();
        } else if (child == 0) {        /* child */
            printf("exec_args before forking\n");
            fflush(stdout);
            for (i = 0; i < END; i++) {
                printf("exec_args[%i] = %s\n", i, exec_args[i]);
                fflush(stdout);
            }

            execvp(exec_args[0], exec_args);
        } else {                /* parent */
            (*NHostsStarted)++;

            /* free child-specific arguments */
            free(exec_args[HOSTNAME]);
            free(exec_args[NPROCS]);
            free(exec_args[CNTL_PORT]);
            free(exec_args[STDERR_PORT]);
            free(exec_args[STDOUT_PORT]);
            free(exec_args[ENV_VARS]);
            free(exec_args[PROG_NAME]);
            free(exec_args[CD]);

            l = l->next;
        }
    }

    /* clean up */
    free(exec_args[RSH]);
    free(exec_args[LAMPID]);
    free(exec_args[SOCK_IP]);
    free(exec_args[PROG_ARGS]);

    free(exec_args);

    return ret_status;
}
