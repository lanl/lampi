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

#ifndef _ULM_INTERNAL_LOG_H_
#define _ULM_INTERNAL_LOG_H_

#include <stdio.h>
#include <stdarg.h>

#include "internal/linkage.h"

#define ulm_dbg(x) \
do { \
    if (ENABLE_DBG) { \
        _ulm_set_file_line( __FILE__, __LINE__) ; \
        _ulm_dbg x ; \
    } \
} while (0)

#define ulm_fdbg(x) \
do { \
    if (ENABLE_DBG) { \
        _ulm_set_file_line( __FILE__, __LINE__) ; \
        _ulm_fdbg x ; \
    } \
} while (0)

#define ulm_dbg_fp(fp, x) \
do { \
    if (ENABLE_DBG) { \
        fprintf(fp, "%s:%d: (pid = %d): ", __FILE__, __LINE__, getpid()); \
        fprintf(fp, x); \
    } \
} while (0)

#define ulm_err(x) \
do { \
    _ulm_set_file_line(__FILE__, __LINE__) ; \
    _ulm_err x ; \
} while (0)

#define ulm_ferr(x) \
do { \
    _ulm_set_file_line(__FILE__, __LINE__) ; \
    _ulm_fdbg x ; \
} while (0)

#define ulm_warn(x) \
do { \
    _ulm_set_file_line(__FILE__, __LINE__) ; \
    _ulm_warn x ; \
} while (0)

#define ulm_exit(x) \
do { \
    _ulm_set_file_line(__FILE__, __LINE__) ; \
    _ulm_exit x ; \
} while (0)

#define ulm_info(x) \
do { \
    if (ENABLE_VERBOSE) { \
        _ulm_info x ; \
    } \
} while (0)

#define ulm_notice(x) \
do { \
    _ulm_notice x ; \
} while (0)

CDECL_BEGIN

void _ulm_log(FILE* fd, const char* fmt, va_list ap);
void _ulm_print(FILE* fd, const char* fmt, va_list ap);

/* Error condition */
void _ulm_err(const char* fmt, ...);

/* Warning condition */
void _ulm_warn(const char* fmt, ...);

/* Normal, but significant, condition */
void _ulm_notice(const char* fmt, ...);

/* Informational message */
void _ulm_info(const char* fmt, ...);

/* Debugging message */
void _ulm_dbg(const char* fmt, ...);

/* Debugging message (outputs to file lampi_<host name>_<pid>.log) */
void _ulm_fdbg(const char* fmt, ...);

/* Exit with error message */
void _ulm_exit(int status, const char* fmt, ...);

/* Set file and line info */
void _ulm_set_file_line(const char *file, int lineno);

CDECL_END

#endif /* !_ULM_INTERNAL_LOG_H_ */
