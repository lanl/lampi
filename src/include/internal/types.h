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

#ifndef INTERNAL_TYPES_H
#define INTERNAL_TYPES_H

#include <stdlib.h>

#ifdef __APPLE__
#include <sys/types.h>		/* for NFDBITS */
#endif

#include "internal/constants.h"

/**********************************************************************/

/*
 * Typedefs for basic integer types.  These definitions are to provide
 * a constant size for the basic types.  ulm ints are to be 32 bits
 * and ulm longs are to be 64 bits.  WARNING, these definitions are
 * architecture dependent.
 */
typedef int ulm_int32_t;
typedef unsigned int ulm_uint32_t;
typedef long long ulm_int64_t;
typedef unsigned long long ulm_uint64_t;

typedef union {
    void *ptr;
    char chr8[8];
    ulm_uint64_t ul;
} ulm_ptr_t;

typedef struct {
    int i1;
    int i2;
} IntPair_t;

typedef struct {
    unsigned int i1;
    unsigned int i2;
} UIntPair_t;

typedef char HostName_t[ULM_MAX_HOSTNAME_LEN];
typedef char DirName_t[ULM_MAX_PATH_LEN];
typedef char FileName_t[ULM_MAX_PATH_LEN];
typedef char ExeName_t[ULM_MAX_PATH_LEN];
typedef char PrefixName_t[ULM_MAX_PREFIX];

/*
 * Platform dependent time operations.
 */

#if defined (__linux__) || defined (__APPLE__)

typedef struct timeval ulm_timeval_t;
#define ulm_timeofday(t)	gettimeofday(&(t), NULL)
#define ulm_timesec(t)		(double)(t).tv_sec + ((double)(t).tv_usec)*1e-6

#else

typedef struct timespec ulm_timeval_t;
#define ulm_timeofday(t)	clock_gettime(CLOCK_REALTIME, &(t))
#define ulm_timesec(t)		(double)(t).tv_sec + ((double)(t).tv_nsec)*1e-9

#endif                          /* LINUX */

/*
 * For Apple/darwin, we have to explicitly define the iovec since the
 * base field is type char *.
 */
typedef struct {
    void *iov_base;
    size_t iov_len;
} ulm_iovec_t;

/* we need to allow a larger number of file descriptors. This is a (hopefully) temporary
fix for bproc. */

#ifndef ULM_FD_SETSIZE
#define ULM_FD_SETSIZE		4096
#endif

typedef struct {
    ulm_uint32_t fds_bits[ULM_FD_SETSIZE / NFDBITS];
} ulm_fd_set_t;


#define ulm_readv(fd, iov, iovc) \
		readv((fd), (const struct iovec *)(iov), (iovc))
#define ulm_writev(fd, iov, iovc) \
		writev((fd), (const struct iovec *)(iov), (iovc))


/*
 * Memory affinity is almost certainly and int everywhere, but let's
 * make it a typedef in case we need to make it OS dependenent
 * sometime...
 */

typedef int affinity_t;

/**********************************************************************/

/*
 * C++ only types
 */

#ifdef __cplusplus

#include "util/Links.h"
#include "util/Lock.h"

// link list for pointers

class ptrLink_t : public Links_t {

public:
    void *pointerToData;
    Locks Lock;

    // constructors
    ptrLink_t() { }

    ptrLink_t(int poolIndex)
        {
            Lock.init();
        }
};

// For context ID allocation

struct MaxCtxtId {
    volatile int max_id;
    Locks id_lock;
};

#endif /* __cplusplus */


/**********************************************************************/

#endif  /* INTERNAL_TYPES_H */
