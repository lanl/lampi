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



#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/JobParams.h"

#include "run/coprocess.h"

/*
 * mpirun spawner for the case where server and clients are all on one host,
 * so we can create the jobs using fork/exec
 *
 * Mainly intended for debugging on a single system.
 */
int mpirun_spawn_exec(unsigned int *auth_data,
                      int socket,
                      int **hosts_started,
                      ULMRunParams_t *RunParameters,
                      int FirstAppArgument, int argc, char **argv)
{
    enum {
        DEBUG = 1,
        BUFSIZE = 1024
    };
    char buf[BUFSIZE];
    coprocess_t cp[1];
    int rc;

    if (DEBUG) {
        fprintf(stderr, "mpirun_spawn_exec\n");
        fflush(stderr);
    }

    /*
     * This spawn method is only valid for 1 host
     */

    if (RunParameters->NHosts != 1) {
        perror("mpirun_spawn_exec only valid for 1 host");
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
             RunParameters->mpirunName);
    putenv(strdup(buf));
    /* turn app started echoing off */
    snprintf(buf, sizeof(buf), "LAMPI_NOECHOAPPSTARTED=");
    putenv(strdup(buf));

    /*
     * fork/exec child
     */
    argv += FirstAppArgument - 1;
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
