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



#include <sys/types.h> /* CYGWIN needs this first for netinet/in.h */
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <setjmp.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "run/JobParams.h"

/*
 * Use RSH to spawn master process on remote host.  This routine also
 * connects standard out and standard error to one end of a pipe - one
 * per host - so that when a connection is established with the remote
 * host, stdout and stderr from the remote hosts can be read by
 * ULMRun, and processed.  ULMRun in turn will redirect this output to
 * its own stdout and stderr files - in this case the controlling tty.
 */
int SpawnUserAppRSH(unsigned int *AuthData,
                    int ReceivingSocket,
                    int **ListHostsStarted,
                    ULMRunParams_t *RunParameters,
                    int FirstAppArgument, int argc, char **argv)
{
    char TMP[ULM_MAX_CONF_FILELINE_LEN];
    int i, RetVal, offset, LenList, dupSTDERRfd, dupSTDOUTfd;
#ifndef ENABLE_CT
    int STDERRpipe[2], STDOUTpipe[2];
#endif
    int AlarmReturn;
    size_t len, MaxSize;
    pid_t Child;
    char **ExecArgs;
    jmp_buf JumpBufferCheckClients; // longjmp buf for VerifyClientStartup
    int NHostsStarted = 0;

    /* compute size of execvp argv[] , and max space needed to store the strings */
    MaxSize = 0;
    /* app args */
    LenList = argc - FirstAppArgument;
    /* remote host rsh  - rsh and remote host */
    LenList += 3;
    len = strlen("rsh");
    if (len > MaxSize)
        MaxSize = len;
    len = strlen("-n");
    if (len > MaxSize)
        MaxSize = len;
    for (i = 0; i < RunParameters->NHosts; i++) {
        len = strlen(RunParameters->HostList[i]);
        if (len > MaxSize)
            MaxSize = len;
    }
    /* app name */
    LenList++;
    /* env. vars. needed for setup  - 3 of them, format:
       csh/tcsh: setenv X Y ;
       sh : export X ; X=Y ;

    */
    /* !!!!!!!!!!!! add code for sh */
    len = strlen("setenv");
    if (len > MaxSize)
        MaxSize = len;

    /* csh/tcsh */
    LenList += (5 * 4);  /* 5 env vars and 4 items per var. */
    /* auth data */
    len = strlen("LAMPI_ADMIN_AUTH0");
    if (len > MaxSize)
        MaxSize = len;
    for (i = 0; i<3; i++)
    {
        sprintf(TMP, "%u", AuthData[i]);
        len = strlen(TMP);
        if (len > MaxSize)
            MaxSize = len;
    }

    /* socket number */
    len = strlen("LAMPI_ADMIN_PORT");
    if (len > MaxSize)
        MaxSize = len;
    sprintf(TMP, "%d", ReceivingSocket);
    len = strlen(TMP);
    if (len > MaxSize)
        MaxSize = len;

    if ((len = strlen(RunParameters->mpirunName)) > MaxSize)
        MaxSize = len;
    len = strlen("LAMPI_ADMIN_IP");
    if (len > MaxSize)
        MaxSize = len;

    /* run directory */
    LenList += 3;
    len = strlen("cd");
    if (len > MaxSize)
        MaxSize = len;
    for (i = 0; i < RunParameters->NHosts; i++) {
        len = strlen(RunParameters->WorkingDirList[i]);
        if (len > MaxSize)
            MaxSize = len;
    }
    /* executable */
    LenList++;
    for (i = 0; i < RunParameters->NHosts; i++) {
        len = strlen(RunParameters->ExeList[i]);
        if (len > MaxSize)
            MaxSize = len;
    }
    /* & to background app */
    LenList++;
    len = strlen("&");
    if (len > MaxSize)
        MaxSize = len;

    /* null terminator */
    LenList++;
    /* user args */
    for (i = FirstAppArgument; i < argc; i++) {
        len = strlen(argv[i]);
        if (len > MaxSize)
            MaxSize = len;
    }

    /* add the csh -c " " lines */
    LenList += 4;

    /****************************
     *      spawn jobs
     ****************************/
    /* first setup stderr intercept so that error conditions can be intercepeted */

    RunParameters->STDERRfds = ulm_new(int, RunParameters->NHosts);
    RunParameters->STDOUTfds = ulm_new(int, RunParameters->NHosts);
    for (i = 0; i < RunParameters->NHosts; i++) {
        RunParameters->STDERRfds[i] = 0;
        RunParameters->STDOUTfds[i] = 0;
    }

    /*
     * list of hosts for which the ULMRun fork() succeeded - needed for
     * cleanup after abnormal termination
     */
    *ListHostsStarted = ulm_new(int, RunParameters->NHosts);

    /* dup current stderr and stdout so that ULMRun's stderr and
     * stdout can be resored to those before exiting this routines.
     */

    dupSTDERRfd = dup(STDERR_FILENO);
    if (dupSTDERRfd <= 0) {
        printf("Error: duping STDERR_FILENO.\n");
        Abort();
    }
    dupSTDOUTfd = dup(STDOUT_FILENO);
    if (dupSTDOUTfd <= 0) {
        printf("Error: duping STDOUT_FILENO.\n");
        Abort();
    }

    fflush(stdout);
    fflush(stderr);

    /* spawn jobs */

    // each host could have a different number of execv arguments
    //    Could be a differnent number of environment variables to set
    int hostSpecificLenList;
    // each host could have a different Maximum string size
    size_t hostSpecificMaxSize;

    for (i = 0; i < RunParameters->NHosts; i++) {
        /* create exec string */
        hostSpecificLenList = LenList;
        hostSpecificMaxSize = MaxSize;
        // check to see if additional environment variables need to be set,
        //  and if so, adjust hostSpecificLenList and hostSpecificMaxSize
        bool addEnvVar = false;
        int nAddedElements = 0;
        if (RunParameters->nEnvVarsToSet > 0) {
            // check to see if any environment variables need to be set
            //  if so adjust paramenters
            for (int eVar = 0; eVar < RunParameters->nEnvVarsToSet; eVar++) {
                size_t envLen =
                    strlen(RunParameters->envVarsToSet[eVar].var_m);
                if (RunParameters->envVarsToSet[eVar].setForAllHosts_m) {
                    addEnvVar = true;
                    // add elements for  ' setenv x y ; '
                    hostSpecificLenList += 4;
                    nAddedElements += 4;
                    size_t tmp =
                        strlen(RunParameters->envVarsToSet[eVar].
                               envString_m[0]);
                    if (hostSpecificMaxSize < envLen)
                        hostSpecificMaxSize = envLen;
                    if (hostSpecificMaxSize < tmp)
                        hostSpecificMaxSize = tmp;
                } else if (RunParameters->envVarsToSet[eVar].
                           setForThisHost_m[i]) {
                    addEnvVar = true;
                    // add elements for  ' setenv x y ; '
                    hostSpecificLenList += 4;
                    nAddedElements += 4;
                    size_t tmp =
                        strlen(RunParameters->envVarsToSet[eVar].
                               envString_m[i]);
                    if (hostSpecificMaxSize < envLen)
                        hostSpecificMaxSize = envLen;
                    if (hostSpecificMaxSize < tmp)
                        hostSpecificMaxSize = tmp;
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
        int EndLibEnvVars = 25;
        int CDEntry = EndLibEnvVars + 1 + nAddedElements;
        int WorkingDirEntry = CDEntry + 1;
        int AppEntry = CDEntry + 3;
        int AppArgs = AppEntry + 1;

        sprintf(ExecArgs[0], "rsh");
        sprintf(ExecArgs[1], "-n");
        /* entry 2 is the host name - will be filled in loop */
        sprintf(ExecArgs[3], "csh");
        sprintf(ExecArgs[4], "-c");
        sprintf(ExecArgs[5], "\"");

        sprintf(ExecArgs[6], "setenv");
        sprintf(ExecArgs[7], "LAMPI_ADMIN_AUTH0");
        sprintf(ExecArgs[8], "%u", AuthData[0]);
        sprintf(ExecArgs[9], ";");
        sprintf(ExecArgs[10], "setenv");
        sprintf(ExecArgs[11], "LAMPI_ADMIN_AUTH1");
        sprintf(ExecArgs[12], "%u", AuthData[1]);
        sprintf(ExecArgs[13], ";");
        sprintf(ExecArgs[14], "setenv");
        sprintf(ExecArgs[15], "LAMPI_ADMIN_AUTH2");
        sprintf(ExecArgs[16], "%u", AuthData[2]);
        sprintf(ExecArgs[17], ";");
		
        sprintf(ExecArgs[18], "setenv");
        sprintf(ExecArgs[19], "LAMPI_ADMIN_PORT");
        sprintf(ExecArgs[20], "%d", ReceivingSocket);
        sprintf(ExecArgs[21], ";");
        sprintf(ExecArgs[22], "setenv");
        sprintf(ExecArgs[23], "LAMPI_ADMIN_IP");
        sprintf(ExecArgs[24], "%s", RunParameters->mpirunName);
        sprintf(ExecArgs[25], ";");
        if (addEnvVar) {
            // check to see if any environment variables need to be set
            //  if so adjust paramenters
            int nAdded = 0;
            for (int eVar = 0; eVar < RunParameters->nEnvVarsToSet; eVar++) {
                if (RunParameters->envVarsToSet[eVar].setForAllHosts_m) {
                    // add elements for  ' setenv x y ; '
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 1],
                            "setenv");
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 2],
                            RunParameters->envVarsToSet[eVar].var_m);
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 3], "%s",
                            RunParameters->envVarsToSet[eVar].
                            envString_m[0]);
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 4], ";");
                    nAdded += 4;
                } else if (RunParameters->envVarsToSet[eVar].
                           setForThisHost_m[i]) {
                    // add elements for  ' setenv x y ; '
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 1],
                            "setenv");
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 2],
                            RunParameters->envVarsToSet[eVar].var_m);
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 3], "%s",
                            RunParameters->envVarsToSet[eVar].
                            envString_m[i]);
                    sprintf(ExecArgs[EndLibEnvVars + nAdded + 4], ";");
                    nAdded += 4;
                }
            }                   // end eVar loop
        }                       // end if

        /* cd to execute directory */
        sprintf(ExecArgs[CDEntry], "cd");

        /* entry CDEntry+2 is working dir - will be filled in loop */
        sprintf(ExecArgs[CDEntry + 2], ";");

        /* entry CDEntry+4 is app name - will be filled in loop */
        for (int ii = FirstAppArgument; ii < argc; ii++) {
            sprintf(ExecArgs[AppArgs + ii - FirstAppArgument], "%s",
                    argv[ii]);
        }

        /*
         * offset is the number of user args 17 are the number of args
         * for rsh up to the user args, e.g. setting of env vars,
         * cd'ing to the appropriate directory, executable
         */
        offset = argc - FirstAppArgument;
        sprintf(ExecArgs[(CDEntry + 3) + offset + 1], "&");
        sprintf(ExecArgs[(CDEntry + 3) + offset + 2], "\"");

        /* redirect stderr */
#ifndef ENABLE_CT
        RetVal = pipe(STDERRpipe);
        if (RetVal < 0) {
            printf("Error: creating STDERRpipe pipe.\n");
            Abort();
        }
        RetVal = dup2(STDERRpipe[1], STDERR_FILENO);
        if (RetVal <= 0) {
            printf("Error: in dup2 STDERRpipe[0], STDERR_FILENO.\n");
            Abort();
        }
        RunParameters->STDERRfds[i] = STDERRpipe[0];
        close(STDERRpipe[1]);

        /* redirect stdout */
        RetVal = pipe(STDOUTpipe);
        if (RetVal < 0) {
            printf("Error: creating STDOUTpipe pipe.\n");
            Abort();
        }
        RetVal = dup2(STDOUTpipe[1], STDOUT_FILENO);
        if (RetVal <= 0) {
            printf("Error: in dup2 STDOUTpipe[0], STDOUT_FILENO.\n");
            Abort();
        }
        RunParameters->STDOUTfds[i] = STDOUTpipe[0];
        close(STDOUTpipe[1]);
#endif
        
        /* fork() failed */
        if ((Child = fork()) == -1) {
            printf(" Error forking child\n");
            perror(" fork ");
            exit(-1);
        } else if (Child == 0) {        /* child process */
            sprintf(ExecArgs[HostEntry], "%s", RunParameters->HostList[i]);
            sprintf(ExecArgs[WorkingDirEntry], "%s",
                    RunParameters->WorkingDirList[i]);
            sprintf(ExecArgs[AppEntry], "%s", RunParameters->ExeList[i]);

            execvp(ExecArgs[0], ExecArgs);
            printf(" after execv\n");

        } else {                /* parent process */
            (*ListHostsStarted)[NHostsStarted] = i;
            NHostsStarted++;
        }
        // free user app argument list
        for (int ii = 0; ii < (hostSpecificLenList - 1); ii++)
            ulm_delete(ExecArgs[ii]);
        ulm_delete(ExecArgs);
    }
    /* restore STDERR_FILENO and STDOUT_FILENO to state when this routine was entered */
    RetVal = dup2(dupSTDERRfd, STDERR_FILENO);
    if (RetVal <= 0) {
        printf("Error: in dup2 dupSTDERRfd, STDERR_FILENO.\n");
        Abort();
    }
    RetVal = dup2(dupSTDOUTfd, STDOUT_FILENO);
    if (RetVal <= 0) {
        printf("Error: in dup2 dupSTDOUTfd, STDOUT_FILENO.\n");
        Abort();
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
