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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "internal/profiler.h"
#include "init/environ.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/RunParams.h"

#if ENABLE_BPROC == 0

int SpawnBproc(unsigned int *AuthData, int ReceivingSocket,
               int **ListHostsStarted, int argc, char **argv)
{
    return 0;
}

#else

#include <sys/un.h>
#include <sys/bproc.h>

#define MAXHOSTNAMELEN 256
#define MAXBUFFERLEN 256
#define OVERWRITE 2

#if defined (__linux__) || defined (__CYGWIN__)
#define SETENV(NAME,VALUE)  setenv(NAME,VALUE,OVERWRITE);
#endif

#define CHECK_FOR_ERROR(RC)                     \
    if ((RC) == NULL) {                         \
        ERROR_FILE = __FILE__;                  \
        ERROR_LINE = __LINE__;                  \
        goto CLEANUP_ABNORMAL;                  \
    }

extern char **environ;
static int ERROR_LINE = 0;
static char *ERROR_FILE = NULL;

enum {
    EXEC_NAME,
    EXEC_ARGS,
    END
};


static int *pids = 0;


int SpawnBproc(unsigned int *AuthData, int ReceivingSocket,
               int **ListHostsStarted, int argc, char **argv)
{
    char *execName = NULL;
    char *exec_args[END + 1];
    char *tmp_args = NULL;
    char hostList[MAXHOSTNAMELEN];
    int *nodes = 0;
    int argsUsed = 0;
    int i = 0;
    int nHosts = 0;
    int ret_status = 0;
    size_t len = 0;
    char *auth0_str = "LAMPI_ADMIN_AUTH0";
    char *auth1_str = "LAMPI_ADMIN_AUTH1";
    char *auth2_str = "LAMPI_ADMIN_AUTH2";
    char *server_str = "LAMPI_ADMIN_IP";
    char *socket_str = "LAMPI_ADMIN_PORT";
    char *space_str = " ";
    char LAMPI_SOCKET[MAXBUFFERLEN];
    char LAMPI_AUTH[MAXBUFFERLEN];
    char LAMPI_SERVER[MAXBUFFERLEN];
    struct bproc_io_t io[3];

    /* initialize some values */
    memset(LAMPI_SOCKET, 0, MAXBUFFERLEN);
    memset(LAMPI_SERVER, 0, MAXBUFFERLEN);
    memset(LAMPI_AUTH, 0, MAXBUFFERLEN);
    memset(hostList, 0, MAXBUFFERLEN);

    exec_args[EXEC_NAME] = NULL;
    exec_args[EXEC_ARGS] = NULL;
    exec_args[END] = NULL;

    nHosts = RunParams.NHosts;

    CHECK_FOR_ERROR(nodes = (int *) ulm_malloc(sizeof(int) * nHosts));
    CHECK_FOR_ERROR(pids = (int *) ulm_malloc(sizeof(int) * nHosts));

    for (i = 0; i < nHosts; i++) {
        nodes[i] = atoi(RunParams.HostList[i]);
        pids[i] = -1;
    }

    /* executable  for each of the hosts */
    execName = RunParams.ExeList[0];

    sprintf(LAMPI_SOCKET, "%d", ReceivingSocket);
    sprintf(LAMPI_SERVER, "%s", RunParams.mpirunName);

    /* SERVER ENV */
    SETENV(server_str, LAMPI_SERVER);

    /* AUTH ENV */
    sprintf(LAMPI_AUTH, "%u", AuthData[0]);
    SETENV(auth0_str, LAMPI_AUTH);
    sprintf(LAMPI_AUTH, "%u", AuthData[1]);
    SETENV(auth1_str, LAMPI_AUTH);
    sprintf(LAMPI_AUTH, "%u", AuthData[2]);
    SETENV(auth2_str, LAMPI_AUTH);

    /* SOCKET ENV */
    SETENV(socket_str, LAMPI_SOCKET);

    /* adding executable name */
    CHECK_FOR_ERROR(exec_args[EXEC_NAME] =
                    (char *) ulm_malloc(sizeof(char) *
                                        (1 + strlen(execName))));
    sprintf(exec_args[EXEC_NAME], execName);

    /* program arguments */

    if (argc > 0) {
        len = 0;
        argsUsed = 1;
        for (i = 0; i < argc; i++) {
            len += strlen(argv[i]);
            len += 1;           /* for the space after the arguments */

        }
        CHECK_FOR_ERROR(tmp_args =
                        (char *) ulm_malloc(sizeof(char) * (1 + len)));
        tmp_args[0] = 0;
        for (i = 0; i < argc; i++) {
            strcat(tmp_args, argv[i]);
            strcat(tmp_args, space_str);
        }
        CHECK_FOR_ERROR(exec_args[EXEC_ARGS] =
                        (char *) ulm_malloc(sizeof(char) *
                                            (1 + strlen(tmp_args))));
        sprintf(exec_args[EXEC_ARGS], tmp_args);
    }
    /*
     * finish filling in exec_args
     */
    exec_args[END] = NULL;

    for (i = 0; i < 3; i++) {
        io[i].fd = i;
        io[i].type = BPROC_IO_FILE;
#if BPROC_MAJOR_VERSION < 4
        io[i].send_info = 0;
#else
        io[i].flags = 0;
#endif
        io[i].d.file.offset = 0;
        strcpy(io[i].d.file.name, "/dev/null");
    }
    io[0].d.file.flags = O_RDONLY;      /* in */
    io[1].d.file.flags = O_WRONLY;      /* out */
    io[2].d.file.flags = O_WRONLY;      /* err */

    /*
     * Running mpirun within a debug environment, e.g. Totalview, can
     * cause a SIGSTOP to be generated, which causes
     * bproc_vexecmove_io to exit early and not launch all procs.  So
     * ugly fix is to add sleep() to catch the SIGSTOP.
     */
    sleep(1);
    /*
     * Put a template for BPROC_RANK into the environment.
     * For bproc version >= 3.2.0 this gets filled in with the node rank
     * 0000000, 0000001, 0000002, etc.
     */
    putenv("BPROC_RANK=XXXXXXX");
    if (bproc_vexecmove_io(nHosts, nodes, pids, io, 3,
                           exec_args[EXEC_NAME], argv - 1, environ) < 0) {
        ulm_err(("SpawnBproc: bproc_vexecmove: error %s\n",
                 bproc_strerror(errno)));
        ret_status = errno;
        goto CLEANUP_ABNORMAL;
    }

    /* Check the returned pids are OK */
    for (i = 0; i < nHosts; i++) {
        if (pids[i] <= 0) {
            ulm_err(("SpawnBproc: "
                     "failed to start process on node %d: "
                     "error %s\n", nodes[i], bproc_strerror(pids[i])));
            ret_status = pids[i];
            goto CLEANUP_ABNORMAL;
        }
        RunParams.DaemonPIDs[i] = pids[i];
    }

    /* clean up the memory allocations */
    ulm_free(exec_args[EXEC_NAME]);
    if (argsUsed == 1) {
        free(tmp_args);
        free(exec_args[EXEC_ARGS]);
        free(nodes);
        free(pids);
    }
    return 0;

  CLEANUP_ABNORMAL:
    free(exec_args[EXEC_NAME]);
    if (argsUsed == 1) {
        free(tmp_args);
        free(exec_args[EXEC_ARGS]);
        free(nodes);
        free(pids);
    }
    ulm_err(("SpawnBproc: Abnormal termination"));
    return -1;
}

#endif                          /* ENABLE_BPROC */
