/*
 * Copyright 2002-2003. The Regents of the University of
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



#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ctnetwork/CTNetwork.h"

static const int file_line_max = 255;
static char file_line[file_line_max + 1] = "";

double timing_cur, timing_stmp;
char timing_out[100][100];
int timing_scnt = 0;

extern "C" double second(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double) tv.tv_sec + (double) tv.tv_usec * 1.0e-6;
}

extern "C" void _ulm_log(FILE *fd, const char *fmt, va_list ap)
{
    /* Write to a file descriptor and the log file */

    FILE *log_fd;
    
    if (fd != NULL) {
	fprintf(fd, file_line);
	vfprintf(fd, fmt, ap);
	fflush(fd);
    }

    log_fd = fopen("lampi.log", "a");
    if (log_fd != NULL) {
        fprintf(log_fd, file_line);
        vfprintf(log_fd, fmt, ap);
        fflush(log_fd);
        fclose(log_fd);
    } else {
        fprintf(stderr,
                "Unable to append to lampi.log in file %s\n",
                __FILE__);
    }
    
    file_line[0] = '\0';
}

extern "C" void _ulm_print(FILE * fd, const char *fmt, va_list ap)
{
    /* Write to a file descriptor (usually stdout or stderr) */

    if (fd != NULL) {
	fprintf(fd, file_line);
	vfprintf(fd, fmt, ap);
	fflush(fd);
    }
    file_line[0] = '\0';
}

extern "C" void _ulm_set_file_line(const char *name, int line)
{
    sprintf(file_line, "LA-MPI:%s:%d: ", name, line);
}

extern "C" void _ulm_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _ulm_log(stderr, fmt, ap);
    va_end(ap);
}

extern "C" void _ulm_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _ulm_log(stderr, fmt, ap);
    va_end(ap);
}

extern "C" void _ulm_notice(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    fflush(stdout);
    va_end(ap);
}

extern "C" void _ulm_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _ulm_print(stdout, fmt, ap);
    va_end(ap);
}

extern "C" void _ulm_dbg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _ulm_log(stdout, fmt, ap);
    va_end(ap);
}

extern "C" void _ulm_fdbg(const char *fmt, ...)
{
    FILE *log_fd;
    char	buf[200];

    sprintf(buf, "lampi_%s_%d.log", _ulm_host(), getpid());
    log_fd = fopen(buf, "a");
    if (log_fd != NULL) {
        va_list ap;
        va_start(ap, fmt);
        _ulm_log(log_fd, fmt, ap);
        va_end(ap);
        fclose(log_fd);
    } else {
        fprintf(stderr,
                "Unable to append to lampi.log in file %s\n",
                __FILE__);
    }

}

extern "C" void _ulm_exit(int status, const char* fmt, ...)
{
    va_list ap;

    if (fmt) {
        va_start(ap, fmt);
        if (status != 0) {
            _ulm_log(stderr, fmt, ap);
        }
        else {
            _ulm_print(stdout, fmt, ap);
        }
        va_end(ap);
    }

    exit(status);
}
