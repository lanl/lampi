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

static const int buf_size = 255;
static char file_line[buf_size + 1] = "";
static char string[buf_size + 1] = "";
static char log_file[buf_size + 1] = "lampi.log";
static FILE *log_stream = NULL;
static int initialized = 0;

int ulm_warn_enabled = 0;
int ulm_dbg_enabled = ENABLE_DBG;

static void log(const char *fmt, va_list ap)
{
    /* write to log file */

    if (!initialized) {
        initialized = 1;
        if (getenv("LAMPI_LOG")) {
            snprintf(log_file, buf_size, getenv("LAMPI_LOG"));
        }
    }

    log_stream = fopen(log_file, "a");
    if (log_stream) {
        int n = snprintf(string, buf_size, file_line);
        vsnprintf(string + n, buf_size - n, fmt, ap);
        fprintf(log_stream, string);
    }
    fclose(log_stream);
}

static void print(FILE *stream, const char *fmt, va_list ap)
{
    /* write to a file descriptor (usually stdout or stderr) */

    if (stream != NULL) {
        int n = snprintf(string, buf_size, file_line);
        vsnprintf(string + n, buf_size - n, fmt, ap);
        fprintf(stream, string);
        fflush(stream);
    }
}

extern "C" void _ulm_set_file_line(const char *name, int line)
{
    char *p;

    p = strstr(name, "src/");
    if (p) {
        p += strlen("src/");
        snprintf(file_line, buf_size, "LA-MPI:%s:%d: ", p, line);
    } else {
        snprintf(file_line, buf_size, "LA-MPI:%s:%d: ", name, line);
    }
}

extern "C" void _ulm_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(stdout, fmt, ap);
    log(fmt, ap);
    va_end(ap);
    file_line[0] = '\0';
}

extern "C" void _ulm_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(stdout, fmt, ap);
    log(fmt, ap);
    va_end(ap);
    file_line[0] = '\0';
}

extern "C" void _ulm_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(stdout, fmt, ap);
    va_end(ap);
    file_line[0] = '\0';
}

extern "C" void _ulm_dbg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(stdout, fmt, ap);
    log(fmt, ap);
    va_end(ap);
    file_line[0] = '\0';
}

extern "C" void _ulm_exit(int status, const char *fmt, ...)
{
    va_list ap;

    if (fmt) {
        va_start(ap, fmt);
        print(stdout, fmt, ap);
        if (status != 0) {
            log(fmt, ap);
        }
        va_end(ap);
    }

    exit(status);
}
