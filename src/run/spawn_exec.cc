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

#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal/profiler.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/types.h"
#include "run/RunParams.h"
#include "run/coprocess.h"

/*
 * mpirun spawner for the case where server and clients are all on one
 * host, so we can create the jobs using fork/exec
 *
 * Mainly intended for debugging on a single system.
 */
int SpawnExec(unsigned int *auth_data,
              int socket,
              int **hosts_started,
              int argc, char **argv)
{
    enum {
        DEBUG = 0,
        BUFSIZE = 1024
    };
    char buf[BUFSIZE];
    coprocess_t cp[1];
    int rc;

    if (DEBUG) {
        fprintf(stderr, "SpawnExec\n");
        fflush(stderr);
    }

    /*
     * This spawn method is only valid for 1 host
     */

    if (RunParams.NHosts != 1) {
        perror("SpawnExec only valid for 1 host");
        return -1;
    }
    *hosts_started = (int *) ulm_malloc(sizeof(int));
    if (*hosts_started == NULL) {
        perror("malloc");
        return -1;
    }
	
    snprintf(buf, sizeof(buf), "LAMPI_ADMIN_AUTH0=%u", auth_data[0]);
    putenv(strdup(buf));
    snprintf(buf, sizeof(buf), "LAMPI_ADMIN_AUTH1=%u", auth_data[1]);
    putenv(strdup(buf));
    snprintf(buf, sizeof(buf), "LAMPI_ADMIN_AUTH2=%u", auth_data[2]);
    putenv(strdup(buf));
    snprintf(buf, sizeof(buf), "LAMPI_ADMIN_PORT=%d", socket);
    putenv(strdup(buf));
    snprintf(buf, sizeof(buf), "LAMPI_ADMIN_IP=%s",
             RunParams.mpirunName);
    putenv(strdup(buf));

    /*
     * fork/exec child
     */
    argv -= 1;
    if (DEBUG) {
        char **p = argv;
        fprintf(stderr, "exec args:");
        while (*p) {
            fprintf(stderr, " %s", *p++);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    rc = coprocess_open(argv, cp);
    if (rc < 0) {
        perror("coprocess_open");
        return EXIT_FAILURE;
    }
    *hosts_started[0] = 0;
    return 0;
}
