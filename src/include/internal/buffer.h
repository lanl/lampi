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



#ifndef _ULM_BUFFER_H_
#define _ULM_BUFFER_H_

#include <sys/types.h>

#include "internal/cLock.h"
#include "internal/linkage.h"
#include "ulm/types.h"

CDECL_BEGIN

/*
 * buffered pt-2-pt user supplied buffer
 */
struct ULMBufferRange_t {
        
    /* pointer to the next allocated range structure */
    struct ULMBufferRange_t *next;

    /* offset into buffer */
    ssize_t offset;

    /* length of allocation */
    ssize_t len;

    /* pointer to request object that "owns" allocation */
    ULMRequest_t request;

    /* send descriptor reference count; decremented in ReturnDesc */
    int refCount;

    /* flag on when to free allocation */
    int freeAtZeroRefCount;
};

typedef struct ULMBufferRange_t ULMBufferRange_t;

struct bsendData_t {

    /* is pool in use */
    volatile int poolInUse;

    /* buffer length in bytes */
    ssize_t bufferLength;

    /* number of bytes in use */
    volatile ssize_t bytesInUse;

    /* buffer */
    void *buffer;

    /* single linked list of allocations */
    ULMBufferRange_t *allocations;

    /* management lock */
    lockStructure_t Lock;
};

typedef struct bsendData_t  bsendData_t;

ULMBufferRange_t *ulm_bsend_alloc(ssize_t size, int freeAtZero);
ULMBufferRange_t *ulm_bsend_find_alloc(ssize_t offset, ULMRequest_t request);
void ulm_bsend_clean_alloc(int wantToDetach);
int ulm_bsend_increment_refcount(ULMRequest_t request, ssize_t offset);
int ulm_bsend_decrement_refcount(ULMRequest_t request, ssize_t offset);

CDECL_END

#endif /* _ULM_BUFFER_H_ */
