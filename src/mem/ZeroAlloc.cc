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

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal/log.h"
#include "internal/malloc.h"

#if 0

// A portable (?) wrapper for anonymous mmap-ing, using either
// MAP_ANON(YMOUS) or /dev/zero

static void *mmap_anon(void *addr, size_t size, int prot, int flags)
{
    void *p;

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#ifdef MAP_ANONYMOUS
    p = mmap(addr, size, prot, MAP_ANONYMOUS | flags, -1, 0);
#else
    int fd;

    fd = open("/dev/zero", O_RDWR);
    if (fd == -1) {
        ulm_err(("Error: Can't open /dev/zero (%s)\n",
                 strerror(errno)));
    }
    p = mmap(addr, size, prot, flags, fd, 0);
    (void)close(fd);
#endif

    return p;
}


// allocate anonymous memory

void *ZeroAlloc(size_t size, int prot, int flags)
{
    void *p;

    p = mmap_anon(NULL, size, prot, flags);
    if (p == MAP_FAILED) {
        ulm_err(("Error: Out of memory from mmap (%s)\n",
                 strerror(errno)));
        return NULL;
    }

    return p;
}

#endif


void *ZeroAlloc(size_t len, int MemoryProtection, int MemoryFlags)
{
    void *ptr;
    int fd, flags = MemoryFlags;
#ifndef __osf__
    fd = -1;
		
#ifndef __APPLE__
    fd = open("/dev/zero", O_RDWR);
    if (fd < 0)
    {
	perror("/dev/zero");
	close(fd);
	return 0;
    }
#else
	flags = flags | MAP_ANON;
#endif		/* __APPLE__ */

    ptr = mmap(NULL, len, MemoryProtection, flags, fd, 0);
    if ( ptr == MAP_FAILED )
    {
	ulm_err(("Error: mmap failed (%s)\n", strerror(errno)));
	close(fd);
	return (void *)0;
    }
    close(fd);
#else
    if( MemoryFlags & MAP_PRIVATE ) {
	//
	// private memory allocation
	//
	fd = open("/dev/zero", O_RDWR);
	if (fd < 0)
	{
	    perror("/dev/zero");
	    close(fd);
	    return 0;
	}
	ptr = mmap(NULL, len, MemoryProtection, MemoryFlags, fd, 0);
	if ( ptr == MAP_FAILED )
	{
	    fprintf(stderr,
		    " ZeroAlloc :: error in mmap(\"/dev/zero\") call.  Bytes Allocated :: %ld\n", len);
	    fprintf(stderr, " errno :: %d\n", errno);
	    perror(" mmap failed");
	    close(fd);
	    return (void *)0;
	}
	close(fd);
    } else {
	long pageSize = sysconf(_SC_PAGESIZE);
	long long paddedLen = len + (2 * pageSize);
	ptr = ulm_malloc(paddedLen * sizeof(char));
        if (!ptr) {
            ulm_warn(("ZeroAlloc :: padded ulm_malloc() failed for "
                        "%lld bytes\n", paddedLen));
            return (void *)0;
        }
        memset(ptr, 0, paddedLen * sizeof(char));
	ptr = (char *)ptr + (pageSize - 1);
	ptr = (void *)((long)ptr & ~(pageSize - 1));
	//
	// shared memory allocation
	//
	fd = -1;
	ptr = mmap(ptr, len, MemoryProtection, MAP_FIXED | MemoryFlags, fd, 0);

	if ( ptr == MAP_FAILED )
	{
	    ulm_warn(("ZeroAlloc shared mmap error :: %d fd %d\n", errno, fd));
	    perror(" mmap failed");
	    return (void *)0;
	}
    }  // end memory allocation
#endif

    return ptr;
}

