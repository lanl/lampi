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

#ifndef _ULM_INTERNAL_LOG_H_
#define _ULM_INTERNAL_LOG_H_

#include <stdio.h>
#include <stdarg.h>

#include "internal/linkage.h"

/* Runtime control of warning and debug messages */

extern int ulm_dbg_enabled;
extern int ulm_warn_enabled;

/* error, warning, debug and exit macros */

#define ulm_dbg(x)                                      \
    do {                                                \
        if (ulm_dbg_enabled) {                          \
            _ulm_set_file_line(__FILE__, __LINE__) ;    \
            _ulm_log x ;                                \
        }                                               \
    } while (0)

#define ulm_err(x)                                      \
    do {                                                \
        _ulm_set_file_line(__FILE__, __LINE__) ;        \
        _ulm_log x ;                                    \
    } while (0)

#define ulm_warn(x)                                     \
    do {                                                \
        if (ulm_warn_enabled) {                         \
            _ulm_set_file_line(__FILE__, __LINE__) ;    \
            _ulm_log x ;                                \
        }                                               \
    } while (0)

#define ulm_exit(x)                                     \
    do {                                                \
        _ulm_set_file_line(__FILE__, __LINE__) ;        \
        _ulm_log x ;                                    \
        exit(EXIT_FAILURE);                             \
    } while (0)

CDECL_BEGIN

/* print message to stderr and log file */
void _ulm_log(const char* fmt, ...);

/* set file and line information in message buffer */
void _ulm_set_file_line(const char *file, int lineno);

CDECL_END

#endif /* !_ULM_INTERNAL_LOG_H_ */
