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
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <limits.h>

#include "internal/log.h"
#include "internal/profiler.h"
#include "util/dclock.h"

#ifdef __sgi

#include <sys/syssgi.h>
#include <sys/immu.h>

//----------------------------------------------------------------------
// Initialize the hardware timer.
// This is stolen from aclmpl.
//----------------------------------------------------------------------

volatile dclock_hardware_t *dclock_hardware_ptr;
double dclock_secs_per_cycle;
dclock_hardware_t dclock_offset;

void init_dclock()
{
    unsigned int cycleval;

    int poffmask = getpagesize () - 1;
    __psunsigned_t phys_addr = syssgi (SGI_QUERY_CYCLECNTR, &cycleval);
    __psunsigned_t raddr = phys_addr & ~poffmask;
    int fd = open ("/dev/mmem", O_RDONLY);

    dclock_hardware_ptr =
	(dclock_hardware_t *) mmap (0, poffmask, PROT_READ,
				    MAP_PRIVATE, fd, (__psint_t) raddr);

    if (dclock_hardware_ptr == MAP_FAILED)
    {
	ulm_err(( "Error: initializing hardware timer!\n"));
	exit (1);
    }

    dclock_hardware_ptr =
	(dclock_hardware_t*) ((__psunsigned_t) dclock_hardware_ptr +
			      (phys_addr & poffmask));

    if (dclock_hardware_ptr == NULL)
    {
	ulm_err(("Error: aligning hardware timer!\n"));
	exit (1);
    }

    dclock_secs_per_cycle = cycleval / 1.0E12; /* pico sec to sec */
    dclock_offset = *dclock_hardware_ptr;

    close (fd);
}

dclock_hardware_t hw_dclock_overhead()
{
    dclock_hardware_t x0 = hw_dclock();
    dclock_hardware_t x1 = x0;
    for (int i=0; i<10000; ++i)
	x1 += *dclock_hardware_ptr - x1;
    return (x1-x0)/10000;
}

#else

static double time0;

void init_dclock()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    time0 = double(tv.tv_sec) + double(tv.tv_usec)/1000000;
}

dclock_hardware_t hw_dclock_overhead()
{
    struct timeval tv0, tv1;

    gettimeofday(&tv0, NULL);
    for (int i=0; i<10000; ++i) {
	gettimeofday(&tv1, NULL);
    }
    double t0 = double(tv0.tv_sec) + double(tv0.tv_usec)/1000000;
    double t1 = double(tv1.tv_sec) + double(tv1.tv_usec)/1000000;

    return dclock_hardware_t((t1 - t0)/10000);
}

double dclock()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double(tv.tv_sec) - time0) + double(tv.tv_usec)/1000000;
}

#endif // __sgi

double dclock_overhead()
{
    double x0 = dclock();
    for (int i=0; i<10000; ++i) {
	dclock();
    }
    double x1 = dclock();

    return (x1-x0)/10000;
}
