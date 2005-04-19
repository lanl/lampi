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
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* Runtime control of warning and debug messages */

int ulm_warn_enabled = 0;
int ulm_dbg_enabled = ENABLE_DBG;

static const int buf_size = 4096;
static char buf[buf_size];
static char *log_filename = "lampi.log";
static int initialized = 0;

extern "C" void _ulm_log(const char *fmt, ...)
{
    /* write to stderr and a log file */

    FILE *stream;
    va_list ap;

    if (!initialized) {
        initialized = 1;
        if (getenv("LAMPI_LOG")) {
            snprintf(log_filename, buf_size, getenv("LAMPI_LOG"));
        }
    }

    va_start(ap, fmt);
    vsnprintf(buf + strlen(buf), buf_size - strlen(buf), fmt, ap);
    va_end(ap);

    fputs(buf, stderr);
    fflush(stderr);

    stream = fopen(log_filename, "a");
    if (stream) {
        time_t t = time(NULL);
        fputs(ctime(&t), stream);
        fputs(buf, stream);
        fflush(stream);
        fclose(stream);
    }
    buf[0] = '\0';
}

extern "C" void _ulm_set_file_line(const char *name, int line)
{
    char *p = strstr(name, "src/");
    if (p) {
        p += strlen("src/");
        snprintf(buf, buf_size, "LA-MPI:%s:%d: ", p, line);
    } else {
        snprintf(buf, buf_size, "LA-MPI:%s:%d: ", name, line);
    }
}
