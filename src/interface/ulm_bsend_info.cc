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

#include "internal/buffer.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/options.h"
#include "internal/state.h"
#include "path/common/BaseDesc.h"
#include "queue/globals.h"
#include "ulm/ulm.h"

extern "C" void ulm_bsend_info_set(ULMRequest_t request,
                                   void *originalBuffer, size_t bufferSize,
                                   int count, ULMType_t * datatype)
{
    SendDesc_t *sendDesc = (SendDesc_t *) request;

    if ((datatype != NULL) && (datatype->layout == CONTIGUOUS)
        && (datatype->num_pairs != 0)) {
        sendDesc->appBufferPointer =
            (void *) ((char *) originalBuffer +
                      datatype->type_map[0].offset);
    } else {
        sendDesc->appBufferPointer = originalBuffer;
    }
    sendDesc->bsendBufferSize = bufferSize;
    sendDesc->bsendDtypeCount = count;
    sendDesc->bsendDatatype = datatype;
    ulm_type_retain(datatype);
}

extern "C" void ulm_bsend_info_get(ULMRequest_t request,
                                   void **originalBuffer,
                                   size_t * bufferSize, int *count,
                                   ULMType_t ** datatype, int *comm)
{
    SendDesc_t *sendDesc = (SendDesc_t *) request;

    *originalBuffer = sendDesc->appBufferPointer;
    *bufferSize = sendDesc->bsendBufferSize;
    *count = sendDesc->bsendDtypeCount;
    *comm = sendDesc->ctx_m;
    *datatype = sendDesc->bsendDatatype;
}

extern "C" ULMBufferRange_t * ulm_bsend_alloc(ssize_t size, int freeAtZero)
{
    /*
       At any given time, only one buffer is in use as pointed to by
       lampiState.bsendData->buffer.  This is set up with the
       MPI_Buffer_attach.  lampiState.bsendData->allocations is a
       linked list that contains blocks of allocations from the
       buffer.  This routine attempts to grab another chunk of memory
       from the buffer and records the chunk with a new
       ULMBufferRange_t struct and adds it to the allocations linked
       list.
     */

    /* make sure there is at least a possibility of satisfying this allocation request */
    if ((lampiState.bsendData->bufferLength -
         lampiState.bsendData->bytesInUse) < size)
        return NULL;

    ULMBufferRange_t *result =
        (ULMBufferRange_t *) ulm_malloc(sizeof(ULMBufferRange_t));
    ULMBufferRange_t *next, *prev;
    int allocated = 0;

    /* return null if we can't allocate memory for a new allocation */
    if (result == NULL)
        return result;

    result->len = size;
    result->request = NULL;
    result->refCount = -1;
    result->freeAtZeroRefCount = freeAtZero;

    for (next = prev = lampiState.bsendData->allocations; next != NULL;
         prev = next, next = next->next) {
        /* is there room at the beginning of the buffer? */
        if (next == prev) {
            if (next->offset >= size) {
                result->next = next;
                result->offset = 0;
                lampiState.bsendData->allocations = result;
                allocated = 1;
                break;
            }
        }
        /* is there room between these two allocations? */
        else if ((next->offset - (prev->offset + prev->len)) >= size) {
            result->next = next;
            result->offset = prev->offset + prev->len;
            prev->next = result;
            allocated = 1;
            break;
        }
    }

    /* end cases */
    if (!allocated) {
        /* no allocations - so do first allocation! */
        if (!next && !prev) {
            result->next = NULL;
            result->offset = 0;
            lampiState.bsendData->allocations = result;
            allocated = 1;
        }
        /* is there room at the end of the buffer? */
        else if ((lampiState.bsendData->bufferLength - prev->offset -
                  prev->len) >= size) {
            result->next = NULL;
            result->offset = prev->offset + prev->len;
            prev->next = result;
            allocated = 1;
        }
    }

    /* allocation failed -- free memory and return null */
    if (!allocated) {
        ulm_free(result);
        result = NULL;
    }
    /* record allocation in total bytes used */
    else {
        lampiState.bsendData->bytesInUse += size;
    }

    return result;
}

extern "C" ULMBufferRange_t * ulm_bsend_find_alloc(ssize_t offset,
                                                   ULMRequest_t
                                                   request)
{
    ULMBufferRange_t *result = NULL;

    for (result = lampiState.bsendData->allocations; result != NULL;
         result = result->next) {
        if (offset >= 0) {
            if (result->offset == offset) {
                if ((result->request == request)
                    || ((result->request == NULL)
                        && (result->refCount == -1))) {
                    break;
                }
            }
        }
        /* offset == -1 in ulm_request_free() */
        else if ((result->request == request)
                 && (result->freeAtZeroRefCount != 1)) {
            break;
        }
    }

    return result;
}

extern "C" void ulm_bsend_clean_alloc(int wantToDetach)
{
    ULMBufferRange_t *next, *prev;

    prev = next = lampiState.bsendData->allocations;
    while (next != NULL) {
        if (wantToDetach) {
            /* all allocations that are inited only must be released now */
            if (next->refCount == -1) {
                next->refCount = 0;
            }
            next->freeAtZeroRefCount = 1;
        }
        if (next->freeAtZeroRefCount && (next->refCount == 0)) {
            lampiState.bsendData->bytesInUse -= next->len;
            /* new first allocation element in list */
            if (next == prev) {
                lampiState.bsendData->allocations = next->next;
                ulm_free(next);
                prev = next = lampiState.bsendData->allocations;
            }
            /* free this element and move on */
            else {
                prev->next = next->next;
                ulm_free(next);
                next = prev->next;
            }
        } else {
            prev = next;
            next = next->next;
        }
    }
}

extern "C" int ulm_bsend_increment_refcount(ULMRequest_t request,
                                            ssize_t offset)
{
    ULMBufferRange_t *allocation;

    if (usethreads())
        lock(&(lampiState.bsendData->Lock));

    allocation = ulm_bsend_find_alloc(offset, request);

    if (allocation == NULL) {
        if (usethreads())
            unlock(&(lampiState.bsendData->Lock));
        return 0;
    }

    allocation->request = request;
    allocation->refCount += ((allocation->refCount == -1) ? 2 : 1);

    if (usethreads())
        unlock(&(lampiState.bsendData->Lock));

    return 1;
}

extern "C" int ulm_bsend_decrement_refcount(ULMRequest_t request,
                                            ssize_t offset)
{
    ULMBufferRange_t *allocation;

    if (usethreads())
        lock(&(lampiState.bsendData->Lock));

    allocation = ulm_bsend_find_alloc(offset, request);

    if (allocation == NULL) {
        if (usethreads())
            unlock(&(lampiState.bsendData->Lock));
        return 0;
    }

    allocation->refCount--;
    if ((allocation->refCount == 0) && (allocation->freeAtZeroRefCount)) {
        ulm_bsend_clean_alloc(0);
    }

    if (usethreads())
        unlock(&(lampiState.bsendData->Lock));

    return 1;
}
