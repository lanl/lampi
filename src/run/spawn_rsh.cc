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

#include <sys/types.h> /* CYGWIN needs this first for netinet/in.h */
#include <sys/wait.h> /* CYGWIN needs this first for netinet/in.h */
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <setjmp.h>

#include "internal/profiler.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/RunParams.h"

/*
 * Use RSH to spawn master process on remote host.  This routine also
 * connects standard out and standard error to one end of a pipe - one
 * per host - so that when a connection is established with the remote
 * host, stdout and stderr from the remote hosts can be read by
 * ULMRun, and processed.  ULMRun in turn will redirect this output to
 * its own stdout and stderr files - in this case the controlling tty.
 */
int SpawnRsh(unsigned int *AuthData,
             int ReceivingSocket,
             int **ListHostsStarted,
             int argc, char **argv)
{
    char TMP[ULM_MAX_CONF_FILELINE_LEN];
    int i, offset, LenList;
    int AlarmReturn;
    size_t len, MaxSize;
    pid_t Child;
    char **ExecArgs;
    jmp_buf JumpBufferCheckClients; // longjmp buf for VerifyClientStartup
    int NHostsStarted = 0;

    /* compute size of execvp argv[] , and max space needed to store the strings */
    MaxSize = 0;
    /* app args */
    LenList = argc;
    /* remote host rsh  - rsh and remote host */
    LenList += 3;
    len = strlen("rsh");
    if (len > MaxSize)
        MaxSize = len;
    len = strlen("-n");
    if (len > MaxSize)
        MaxSize = len;
    for (i = 0; i < RunParams.NHosts; i++) {
        len = strlen(RunParams.HostList[i]);
        if (len > MaxSize)
            MaxSize = len;
    }
    /* app name */
    LenList++;

    len = strlen("export");
    if (len > MaxSize)
        MaxSize = len;

    /* csh/tcsh */
    LenList += (6 * 3);  /* 6 env vars and 3 items per var. */
    /* auth data */
    for (i = 0; i<3; i++)
    {
        len = strlen("LAMPI_ADMIN_AUTH0");
        sprintf(TMP, "%u", AuthData[i]);
        len += strlen(TMP);
        len += 1;
        if (len > MaxSize)
            MaxSize = len;
    }

    /* socket number */
    len = strlen("LAMPI_ADMIN_PORT");
    sprintf(TMP, "%d", ReceivingSocket);
    len += strlen(TMP);
    len += 1;
    if (len > MaxSize)
        MaxSize = len;

    len = strlen("LAMPI_ADMIN_IP");
    len += strlen(RunParams.mpirunName);
    len += 2;
    if (len > MaxSize)
        MaxSize = len;

    len = strlen("NO_IB_PREMAIN_INIT=1");
    len += 1;
    if (len > MaxSize)
        MaxSize = len;

    /* run directory */
    LenList += 3;
    len = strlen("cd");
    if (len > MaxSize)
        MaxSize = len;
    for (i = 0; i < RunParams.NHosts; i++) {
        len = strlen(RunParams.WorkingDirList[i]);
        if (len > MaxSize)
            MaxSize = len;
    }
    /* executable */
    LenList++;
    for (i = 0; i < RunParams.NHosts; i++) {
        len = strlen(RunParams.ExeList[i]);
        if (len > MaxSize)
            MaxSize = len;
    }
    /*  to background app  >/dev/null 2>&1 1>/dev/null & */
    LenList += 4;

    /* null terminator */
    LenList++;
    /* user args */
    for (i = 0; i < argc; i++) {
        len = strlen(argv[i]);
        if (len > MaxSize)
            MaxSize = len;
    }

    /* add the /bin/sh -c " " lines */
    LenList += 4;

    /****************************
     *      spawn jobs
     ****************************/
    /* setup for stdin fowarding */
    RunParams.STDINfd = STDIN_FILENO;

    /*
     * list of hosts for which the ULMRun fork() succeeded - needed for
     * cleanup after abnormal termination
     */
    *ListHostsStarted = ulm_new(int, RunParams.NHosts);

    /* spawn jobs */

    // each host could have a different number of execv arguments
    //    Could be a differnent number of environment variables to set
    int hostSpecificLenList;
    // each host could have a different Maximum string size
    size_t hostSpecificMaxSize;


#define MAX_CONCURRENT 128
    int rsh_pid[MAX_CONCURRENT];
    int rsh_index = 0;

    for (i = 0; i < RunParams.NHosts; i++) {
        /* create exec string */
        hostSpecificLenList = LenList;
        hostSpecificMaxSize = MaxSize;
        // check to see if additional environment variables need to be set,
        //  and if so, adjust hostSpecificLenList and hostSpecificMaxSize
        bool addEnvVar = false;
        int nAddedElements = 0;
        if (RunParams.nEnvVarsToSet > 0) {
            // check to see if any environment variables need to be set
            //  if so adjust paramenters
            for (int eVar = 0; eVar < RunParams.nEnvVarsToSet; eVar++) {
                size_t envLen =
                    strlen(RunParams.envVarsToSet[eVar].var_m);
                if (RunParams.envVarsToSet[eVar].setForAllHosts_m) {
                    addEnvVar = true;
                    // add elements for  ' export x=y ; '
                    hostSpecificLenList += 3;
                    nAddedElements += 3;
                    size_t tmp = strlen(RunParams.envVarsToSet[eVar].envString_m[0]);
                    if (hostSpecificMaxSize < envLen + tmp + 1)
                        hostSpecificMaxSize = envLen + tmp + 1;
                } else if (RunParams.envVarsToSet[eVar].
                           setForThisHost_m[i]) {
                    addEnvVar = true;
                    // add elements for  ' export x=y ; '
                    hostSpecificLenList += 3;
                    nAddedElements += 3;
                    size_t tmp = strlen(RunParams.envVarsToSet[eVar].envString_m[i]);
                    if (hostSpecificMaxSize < envLen + tmp + 1)
                        hostSpecificMaxSize = envLen + tmp + 1;
                }
            }                   // end eVar loop
        }                       // end if

        // add one byte padding to avoid garbage at end of string
        hostSpecificMaxSize++;
        ulm_dbg((" hostSpecificLenList %ld hostSpecificMaxSize %ld\n",
                 hostSpecificLenList, hostSpecificMaxSize));

        ExecArgs = ulm_new( char *, hostSpecificLenList);
        for (int ii = 0; ii < (hostSpecificLenList - 1); ii++) {
            ExecArgs[ii] = ulm_new(char, hostSpecificMaxSize);
            bzero(ExecArgs[ii], hostSpecificMaxSize);
        }
        ExecArgs[hostSpecificLenList - 1] = NULL;

        /* fill in string */

        // set offsets into ExecArgs
        int HostEntry = 2;
		
        /* IMPORTANT: Update this value if you add anything
           to ExecArgs below where the indices are explicit,
           e.g. ExecArgs[12] = "foo"
        */
        int EndLibEnvVars = 23;
        int CDEntry = EndLibEnvVars + 1 + nAddedElements;
        int WorkingDirEntry = CDEntry + 1;
        int AppEntry = CDEntry + 3;
        int AppArgs = AppEntry + 1;

        if ( RunParams.UseSSH )
            sprintf(ExecArgs[0], "ssh");
        else
            sprintf(ExecArgs[0], "rsh");
        sprintf(ExecArgs[1], "-n");
        /* entry 2 is the host name - will be filled in loop */
        sprintf(ExecArgs[3], "/bin/sh");
        sprintf(ExecArgs[4], "-lc");
        sprintf(ExecArgs[5], "\"");

        sprintf(ExecArgs[6], "export");
        sprintf(ExecArgs[7], "LAMPI_ADMIN_AUTH0=%u", AuthData[0]);
        sprintf(ExecArgs[8], ";");
        sprintf(ExecArgs[9], "export");
        sprintf(ExecArgs[10], "LAMPI_ADMIN_AUTH1=%u", AuthData[1]);
        sprintf(ExecArgs[11], ";");
        sprintf(ExecArgs[12], "export");
        sprintf(ExecArgs[13], "LAMPI_ADMIN_AUTH2=%u", AuthData[2]);
        sprintf(ExecArgs[14], ";");
		
        sprintf(ExecArgs[15], "export");
        sprintf(ExecArgs[16], "LAMPI_ADMIN_PORT=%d", ReceivingSocket);
        sprintf(ExecArgs[17], ";");
        sprintf(ExecArgs[18], "export");
        sprintf(ExecArgs[19], "LAMPI_ADMIN_IP=%s", RunParams.mpirunName);
        sprintf(ExecArgs[20], ";");
        sprintf(ExecArgs[21], "export");
        sprintf(ExecArgs[22], "NO_IB_PREMAIN_INIT=1");
        sprintf(ExecArgs[23], ";");

        if (addEnvVar) {
            // check to see if any environment variables need to be set
            //  if so adjust paramenters
            int nAdded = 0;
            for (int eVar = 0; eVar < RunParams.nEnvVarsToSet; eVar++) {
                if (RunParams.envVarsToSet[eVar].setForAllHosts_m) {
                    // add elements for  ' export x = y ; '
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 1], "export");
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 2], "%s=%s",
                            RunParams.envVarsToSet[eVar].var_m,
                            RunParams.envVarsToSet[eVar].envString_m[0]);
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 3], ";");
                    nAdded += 3;
                } else if (RunParams.envVarsToSet[eVar].
                           setForThisHost_m[i]) {
                    // add elements for  ' export x = y ; '
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 1],
                            "export");
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 2], "%s=%s",
                            RunParams.envVarsToSet[eVar].var_m,
                            RunParams.envVarsToSet[eVar].envString_m[i]);
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 3], ";");
                    nAdded += 3;
                }
            }                   // end eVar loop
        }                       // end if

        /* cd to execute directory */
        sprintf(ExecArgs[CDEntry], "cd");

        /* entry CDEntry+2 is working dir - will be filled in loop */
        sprintf(ExecArgs[CDEntry + 2], ";");

        /* entry CDEntry+4 is app name - will be filled in loop */
        for (int ii = 0; ii < argc; ii++) {
            sprintf(ExecArgs[AppArgs + ii], "%s",
                    argv[ii]);
        }

        /*
         * offset is the number of user args 17 are the number of args
         * for rsh up to the user args, e.g. setting of env vars,
         * cd'ing to the appropriate directory, executable
         */
        offset = argc;
        sprintf(ExecArgs[(CDEntry + 3) + offset + 1], "</dev/null");
        sprintf(ExecArgs[(CDEntry + 3) + offset + 2], "1>/dev/null");
        sprintf(ExecArgs[(CDEntry + 3) + offset + 3], "2>&1");
        sprintf(ExecArgs[(CDEntry + 3) + offset + 4], "&");
        sprintf(ExecArgs[(CDEntry + 3) + offset + 5], "\"");

        /* fork() failed */
        if ((Child = fork()) == -1) {
            printf(" Error forking child\n");
            perror(" fork ");
            exit(EXIT_FAILURE);
        } else if (Child == 0) {        /* child process */
            sprintf(ExecArgs[HostEntry], "%s", RunParams.HostList[i]);
            sprintf(ExecArgs[WorkingDirEntry], "%s",
                    RunParams.WorkingDirList[i]);
            sprintf(ExecArgs[AppEntry], "%s", RunParams.ExeList[i]);

            execvp(ExecArgs[0], ExecArgs);
            printf(" after execv\n");

        } else {                /* parent process */
            rsh_pid[rsh_index++] = Child;
            if(rsh_index == MAX_CONCURRENT) {
                // reap children (rsh)
                for(int index=0; index<rsh_index; index++) {
                    int status;
                    waitpid(rsh_pid[index], &status, 0);
                }
                rsh_index = 0;
            }
            (*ListHostsStarted)[NHostsStarted] = i;
            NHostsStarted++;
        }

        // free user app argument list
        for (int ii = 0; ii < (hostSpecificLenList - 1); ii++)
            ulm_delete(ExecArgs[ii]);
        ulm_delete(ExecArgs);
    }

    // reap children (rsh)
    for(int index=0; index<rsh_index; index++) {
        int status;
        waitpid(rsh_pid[index], &status, 0);
    }
        
    /*
     * Check that clients have started up normally
     */
    alarm(ALARMTIME);

    /*
     * Check for alarm expirations
     */
    AlarmReturn = setjmp(JumpBufferCheckClients);
    if (AlarmReturn != 0) {
        goto VerifyClientStartupTag;
    }

VerifyClientStartupTag:
    alarm(0);

    return 0;
}
