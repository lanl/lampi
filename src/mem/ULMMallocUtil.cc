/*
 * This file is part of LA-MPI
 *
 * Copyright 2002 Los Alamos National Laboratory
 *
 * This software and ancillary information (herein called "LA-MPI") is
 * made available under the terms described here.  LA-MPI has been
 * approved for release with associated LA-CC Number LA-CC-02-41.
 * 
 * Unless otherwise indicated, LA-MPI has been authored by an employee
 * or employees of the University of California, operator of the Los
 * Alamos National Laboratory under Contract No.W-7405-ENG-36 with the
 * U.S. Department of Energy.  The U.S. Government has rights to use,
 * reproduce, and distribute LA-MPI. The public may copy, distribute,
 * prepare derivative works and publicly display LA-MPI without
 * charge, provided that this Notice and any statement of authorship
 * are reproduced on all copies.  Neither the Government nor the
 * University makes any warranty, express or implied, or assumes any
 * liability or responsibility for the use of LA-MPI.
 * 
 * If LA-MPI is modified to produce derivative works, such modified
 * LA-MPI should be clearly marked, so as not to confuse it with the
 * version available from LANL.
 * 
 * LA-MPI is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * LA-MPI is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 */

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "mem/ULMMallocUtil.h"

/*
 * allocate a chunk of memory from /dev/zero
 */
void * ZeroAlloc(unsigned long long len, int MemoryProtection, int MemoryFlags)
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
	fprintf(stderr,
		" ZeroAlloc :: error in mmap(\"/dev/zero\") call.  Bytes Allocated :: %lld\n", len);
	fprintf(stderr, " errno :: %d\n", errno);
	perror(" mmap failed");
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

/*
 * free a /dev/zero mmaped segment of memory
 */
void ZeroFree(void *ptr, size_t len)
{
    int RetVal=munmap(ptr, len);
    if( RetVal == -1 ) {
	char *str1=" ZeroFree - Error :: Unable to free : ";
	char *str2=" length to free : ";
	ssize_t ptrSize=sizeof(ptr);
	ssize_t lenSize=sizeof(size_t);
	ssize_t totalLen=ptrSize+lenSize+strlen(str1)+strlen(str2)+1;
	char *errorString=(char *)ulm_malloc(totalLen);
	sprintf(errorString, "%s%p%s%lu", str1, ptr, str2, (unsigned long)len);
	perror(errorString);
	ulm_free(errorString);
    }
}
