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

#if ENABLE_BPROC == 0

int SpawnBproc(unsigned int *AuthData, int ReceivingSocket,
               int **ListHostsStarted, int argc, char **argv)
{
    return 0;
}

#else

#include <sys/bproc.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/profiler.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "run/RunParams.h"

extern char **environ;

/* a variable setenv -- takes strings or integer values */
static void vsetenv(const char *fmt, const char *var, /* val */ ...)
{
    char *val = NULL;
    va_list ap;

    va_start(ap, var);
    if (!fmt) {
        ulm_exit(("Error: vsetenv: Null format\n"));
    } else if (0 == strcmp(fmt, "%s")) {
        asprintf(&val, "%s", va_arg(ap, char *));
    } else if (0 == strcmp(fmt, "%c")) {
        asprintf(&val, "%c", va_arg(ap, int));
    } else if (0 == strcmp(fmt, "%d")) {
        asprintf(&val, "%d", va_arg(ap, int));
    } else if (0 == strcmp(fmt, "%f")) {
        asprintf(&val, "%f", va_arg(ap, double));
    } else if (0 == strcmp(fmt, "%lf")) {
        asprintf(&val, "%lf", va_arg(ap, double));
    } else {
        ulm_exit(("Error: vsetenv: Bad format: %s\n", fmt));
    }
    va_end(ap);

    setenv(var, val, 1);
    free(val);
}


int SpawnBproc(unsigned int *AuthData, int ReceivingSocket,
               int **ListHostsStarted, int argc, char **argv)
{
    char *progname = RunParams.ExeList[0];
    int *nodelist = NULL;
    int *pidlist = NULL;
    int nHosts = RunParams.NHosts;
    int rc = 0;
    struct bproc_io_t io[3];

    if (RunParams.Verbose) {
        fprintf(stderr, "Executing command: ");
        fprintf(stderr, "%s ", progname);
        for (char **v = argv; *v; v++) {
            fprintf(stderr, "%s ", *v);
        }
        fprintf(stderr, "\n");
    }

    /* set up the environment */
    vsetenv("%s", "LAMPI_ADMIN_IP", RunParams.mpirunName);
    vsetenv("%d", "LAMPI_ADMIN_PORT", ReceivingSocket);
    vsetenv("%d", "LAMPI_ADMIN_AUTH0", AuthData[0]);
    vsetenv("%d", "LAMPI_ADMIN_AUTH1", AuthData[1]);
    vsetenv("%d", "LAMPI_ADMIN_AUTH2", AuthData[2]);
    vsetenv("%s", "BPROC_RANK", "XXXXXXX");
    vsetenv("%s", "BPROC_PROGNAME", progname);

    /* initialize arrays of nodes and pids */
    nodelist = (int *) ulm_malloc(sizeof(int) * nHosts);
    if (!nodelist) {
        ulm_exit(("Error: Out of memory\n"));
    }
    pidlist = (int *) ulm_malloc(sizeof(int) * nHosts);
    if (!pidlist) {
        ulm_exit(("Error: Out of memory\n"));
    }
    for (int i = 0; i < nHosts; i++) {
        nodelist[i] = atoi(RunParams.HostList[i]);
        pidlist[i] = -1;
    }

    /* set up file descriptors */
    for (int i = 0; i < 3; i++) {
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
     * Running mpirun under Totalview, a SIGSTOP can be generated,
     * which causes bproc_vexecmove_io to exit early and not launch
     * all procs.  So ugly fix is to add sleep() to catch the SIGSTOP.
     */
    sleep(1);

    /* start the processes */
    if (bproc_vexecmove_io(nHosts, nodelist, pidlist, io, 3,
                           progname, argv - 1, environ) < 0) {
        ulm_err(("Error: bproc_vexecmove_io: %s\n", bproc_strerror(errno)));
        rc = -1;
    }

    /* check the returned pids */
    for (int i = 0; i < nHosts; i++) {
        if (pidlist[i] <= 0) {
            ulm_err(("Error: bproc_vexecmove_io (node %d): %s\n",
                     nodelist[i], bproc_strerror(pidlist[i])));
            rc = -1;
        }
        RunParams.DaemonPIDs[i] = pidlist[i];
    }

    /* clean up */
    ulm_free(nodelist);
    ulm_free(pidlist);
    return rc;
}

#endif                          /* ENABLE_BPROC */
