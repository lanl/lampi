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

/*
 * Functions for logging run information
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef RUSAGE_SELF
#define RUSAGE_SELF 0
#endif
#ifndef RUSAGE_CHILDREN
#define RUSAGE_CHILDREN -1
#endif

#include "internal/types.h"
#include "run/globals.h"
#include "run/RunParams.h"

//static int priority = LOG_DEBUG | LOG_USER;
static int priority = LOG_DEBUG | LOG_LOCAL6;
static rusage total_ru;

/*
 * add timevals, taking wrap around into acount
 */
#define	TV_ADD(tvp_result, tvp_a, tvp_b)                                \
    do {								\
        (tvp_result)->tv_sec  = (tvp_a)->tv_sec + (tvp_b)->tv_sec;      \
        (tvp_result)->tv_usec = (tvp_a)->tv_usec + (tvp_b)->tv_usec;    \
        if ((tvp_result)->tv_usec >= 1000000) {                         \
            (tvp_result)->tv_sec++;                                     \
            (tvp_result)->tv_usec -= 1000000;                           \
        }                                                               \
    } while (0)

static double seconds(struct timeval *tv)
{
    return (double) tv->tv_sec + 1.0e-6 * (double) tv->tv_usec;
}

void PrintRusage(const char *name, struct rusage *ru)
{
    fprintf(stderr,
            "LA-MPI: *** Resource usage (%s):\n"
            "LA-MPI: ***   utime:    %lf sec\n"
            "LA-MPI: ***   stime:    %lf sec\n"
            "LA-MPI: ***   maxrss:   %ld\n"
            "LA-MPI: ***   ixrss:    %ld\n"
            "LA-MPI: ***   idrss:    %ld\n"
            "LA-MPI: ***   isrss:    %ld\n"
            "LA-MPI: ***   minflt:   %ld\n"
            "LA-MPI: ***   majflt:   %ld\n"
            "LA-MPI: ***   nswap:    %ld\n"
            "LA-MPI: ***   inblock:  %ld\n"
            "LA-MPI: ***   oublock:  %ld\n"
            "LA-MPI: ***   msgsnd:   %ld\n"
            "LA-MPI: ***   msgrcv:   %ld\n"
            "LA-MPI: ***   nsignals: %ld\n"
            "LA-MPI: ***   nvcsw:    %ld\n"
            "LA-MPI: ***   nivcsw:   %ld\n",
            name,
            seconds(&(ru->ru_utime)),  /* user time used */
            seconds(&(ru->ru_stime)),  /* system time used */
            ru->ru_maxrss,             /* integral max resident set size */
            ru->ru_ixrss,              /* integral shared text memory size */
            ru->ru_idrss,              /* integral unshared data size */
            ru->ru_isrss,              /* integral unshared stack size */
            ru->ru_minflt,             /* page reclaims */
            ru->ru_majflt,             /* page faults */
            ru->ru_nswap,              /* swaps */
            ru->ru_inblock,            /* block input operations */
            ru->ru_oublock,            /* block output operations */
            ru->ru_msgsnd,             /* messages sent */
            ru->ru_msgrcv,             /* messages received */
            ru->ru_nsignals,           /* signals received */
            ru->ru_nvcsw,              /* voluntary context switches */
            ru->ru_nivcsw);            /* involuntary context switches */

    TV_ADD(&(total_ru.ru_utime), &(total_ru.ru_utime), &(ru->ru_utime));
    TV_ADD(&(total_ru.ru_stime), &(total_ru.ru_stime), &(ru->ru_stime));
    total_ru.ru_maxrss += ru->ru_maxrss;       
    total_ru.ru_ixrss += ru->ru_ixrss;        
    total_ru.ru_idrss += ru->ru_idrss;        
    total_ru.ru_isrss += ru->ru_isrss;        
    total_ru.ru_minflt += ru->ru_minflt;       
    total_ru.ru_majflt += ru->ru_majflt;       
    total_ru.ru_nswap += ru->ru_nswap;        
    total_ru.ru_inblock += ru->ru_inblock;      
    total_ru.ru_oublock += ru->ru_oublock;      
    total_ru.ru_msgsnd += ru->ru_msgsnd;       
    total_ru.ru_msgrcv += ru->ru_msgrcv;       
    total_ru.ru_nsignals += ru->ru_nsignals;     
    total_ru.ru_nvcsw += ru->ru_nvcsw;        
    total_ru.ru_nivcsw += ru->ru_nivcsw;      
}

void PrintTotalRusage(void)
{
    if (RunParams.PrintRusage) {
        PrintRusage("total of all application processes", &total_ru);
    }
}


void LogJobStart(void)
{
    if (ENABLE_SYSLOG) {

        if (RunParams.Verbose) {
            fprintf(stderr, "LA-MPI: *** writing syslog\n");
        }
        openlog("lampi", LOG_PID, priority);
        syslog(priority,
               "job starting: uid=%ld nhosts=%d nprocs=%d\n",
               (long) getuid(),
               RunParams.NHosts,
               RunParams.TotalProcessCount);
    }
}

void LogJobExit(void)
{
    if (ENABLE_SYSLOG) {
        if (RunParams.Verbose) {
            fprintf(stderr, "LA-MPI: *** writing syslog\n");
        }
        syslog(priority, "job exiting normally\n");
    }
}


void LogJobAbort(void)
{
    if (ENABLE_SYSLOG) {
        if (RunParams.Verbose) {
            fprintf(stderr, "LA-MPI: *** writing syslog\n");
        }
        syslog(priority, "job exiting abnormally\n");
    }
}
