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

#ifndef _INLINE_COPY_FUNCTIONS
#define _INLINE_COPY_FUNCTIONS

#include "ulm/types.h"
#include "util/Utility.h"

/*
 * copy "packed" data to the specified format datatype - data
 * description dest - base destination pointer bytesCopied - number of
 * bytes copied in this call offsetIntoPackedData - starting offset
 * int the packed data bytesToCopy - number of bytes to copy in this
 * call sourcePointer - base contiguous memory pointer
 */
inline int contiguous_to_noncontiguous_copy(ULMType_t *datatype,
                                            void *dest,
                                            ssize_t *bytesCopied,
                                            size_t offsetIntoPackedData,
                                            size_t bytesToCopy,
                                            void *sourcePointer)
{
    size_t len_copied = 0;
    size_t bytesToRead = bytesToCopy;

    /* finish off unread part of last element being read */
    int prev_dtype_cnt = offsetIntoPackedData / datatype->packed_size;
    if (prev_dtype_cnt * datatype->packed_size != offsetIntoPackedData) {

        /* need to continue copying the partially copied data element */
        size_t bytesCopied =
            offsetIntoPackedData - prev_dtype_cnt * datatype->packed_size;

        /* find last partially copied field of data structure */
        int ti = 0;
        size_t partialSum = 0;
        while (datatype->type_map[ti].size + partialSum < bytesCopied) {
            partialSum += datatype->type_map[ti].size;
            ti++;
        }

        void *destinationBuffer =
            (void *) ((char *) dest + prev_dtype_cnt * datatype->extent);

        void *dst;
        /* copy the remaining part of element ti */
        size_t tiAlreayCopied = bytesCopied - partialSum;
        size_t leftInTmapToCopy =
            datatype->type_map[ti].size - tiAlreayCopied;
        if (leftInTmapToCopy > 0) {
            size_t offset = tiAlreayCopied;
            dst =
                (void *) ((char *) destinationBuffer +
                          datatype->type_map[ti].offset + offset);
            /* don't overflow destination buffer */
            if (leftInTmapToCopy > bytesToRead)
                leftInTmapToCopy = bytesToRead;
            MEMCOPY_FUNC(sourcePointer, dst, leftInTmapToCopy);
            len_copied += leftInTmapToCopy;
            sourcePointer =
                (void *) ((char *) sourcePointer + leftInTmapToCopy);
            bytesToRead -= leftInTmapToCopy;
        }

        /* continue to copy the first partial element into the shared
         * memory buffer
         */
        for (int tMapInd = ti + 1; tMapInd < datatype->num_pairs;
             tMapInd++) {

            if (datatype->type_map[tMapInd].size <= bytesToRead) {

                /* full data structure element fits in shared buffer */
                dst =
                    (void *) ((char *) destinationBuffer +
                              datatype->type_map[tMapInd].offset);
                MEMCOPY_FUNC(sourcePointer, dst,
                             datatype->type_map[tMapInd].size);
                len_copied += datatype->type_map[tMapInd].size;
                sourcePointer =
                    (void *) ((char *) sourcePointer +
                              datatype->type_map[tMapInd].size);
                bytesToRead -= datatype->type_map[tMapInd].size;
                if (bytesToRead == 0)
                    break;
            } else {

                /* partial data structure element fits in shared buffer */
                dst =
                    (void *) ((char *) destinationBuffer +
                              datatype->type_map[tMapInd].offset);
                MEMCOPY_FUNC(sourcePointer, dst, bytesToRead);
                len_copied += bytesToRead;
                sourcePointer =
                    (void *) ((char *) sourcePointer + bytesToRead);
                bytesToRead = 0;
                break;
            }
        }
        prev_dtype_cnt++;
    }

    /* end read of first element */
    /* read all possible full element */
    int nElementsToCopy = bytesToRead / datatype->packed_size;
    void *destination =
        (void *) ((char *) dest + prev_dtype_cnt * datatype->extent);

    for (int dtElement = 0; dtElement < nElementsToCopy; dtElement++) {
        for (int tMapInd = 0; tMapInd < datatype->num_pairs; tMapInd++) {
            void *dst =
                (void *) ((char *) destination +
                          datatype->type_map[tMapInd].offset);
            MEMCOPY_FUNC(sourcePointer, dst,
                         datatype->type_map[tMapInd].size);
            len_copied += datatype->type_map[tMapInd].size;
            sourcePointer =
                (void *) ((char *) sourcePointer +
                          datatype->type_map[tMapInd].size);
            bytesToRead -= datatype->type_map[tMapInd].size;
        }
        destination = (void *) ((char *) destination + datatype->extent);
    }

    /* read partial last element in buffer */
    if (bytesToRead > 0) {

        for (int tMapInd = 0; tMapInd < datatype->num_pairs; tMapInd++) {

            if (datatype->type_map[tMapInd].size <= bytesToRead) {

                /* full data structure element fits in shared buffer */
                void *dst =
                    (void *) ((char *) destination +
                              datatype->type_map[tMapInd].offset);
                MEMCOPY_FUNC(sourcePointer, dst,
                             datatype->type_map[tMapInd].size);
                len_copied += datatype->type_map[tMapInd].size;
                sourcePointer =
                    (void *) ((char *) sourcePointer +
                              datatype->type_map[tMapInd].size);
                bytesToRead -= datatype->type_map[tMapInd].size;
                if (bytesToRead == 0)
                    break;

            } else {

                /* partial data structure element fits in shared buffer */
                void *dst =
                    (void *) ((char *) destination +
                              datatype->type_map[tMapInd].offset);
                MEMCOPY_FUNC(sourcePointer, dst, bytesToRead);
                len_copied += bytesToRead;
                sourcePointer =
                    (void *) ((char *) sourcePointer + bytesToRead);
                bytesToRead = 0;
                break;
            }
        }
    }

    *bytesCopied = len_copied;
    return ULM_SUCCESS;
}

/*
 * copy data with specifed format to "packed" buffer datatype - data
 * description dest - base destination pointer bytesCopied - number of
 * bytes copied in this call offsetIntoPackedData - starting offset
 * int the packed data bytesToCopy - number of bytes to copy in this
 * call sourcePointer - base contiguous memory pointer
 */
inline int noncontiguous_to_contiguous_copy(ULMType_t *datatype,
                                            void *dest,
                                            ssize_t *bytesCopied,
                                            size_t offsetIntoPackedData,
                                            size_t bytesToCopy,
                                            void *sourcePointer)
{
    size_t len_copied = 0;
    size_t bytesToRead = bytesToCopy;

    /* finish off unread part of last element being read */
    int prev_dtype_cnt = offsetIntoPackedData / datatype->packed_size;
    if (prev_dtype_cnt * datatype->packed_size != offsetIntoPackedData) {

        /* need to continue copying the partially copied data element */
        size_t bytesCopied =
            offsetIntoPackedData - prev_dtype_cnt * datatype->packed_size;

        /* find last partially copied field of data structure */
        int ti = 0;
        size_t partialSum = 0;
        while (datatype->type_map[ti].size + partialSum < bytesCopied) {
            partialSum += datatype->type_map[ti].size;
            ti++;
        }

        void *source =
            (void *) ((char *) sourcePointer + prev_dtype_cnt * datatype->extent);

        void *src;
        /* copy the remaining part of element ti */
        size_t tiAlreayCopied = bytesCopied - partialSum;
        size_t leftInTmapToCopy =
            datatype->type_map[ti].size - tiAlreayCopied;
        if (leftInTmapToCopy > 0) {
            size_t offset = tiAlreayCopied;
            src =
                (void *) ((char *) source + datatype->type_map[ti].offset +
                          offset);
            /* don't overflow destination buffer */
            if (leftInTmapToCopy > bytesToRead)
                leftInTmapToCopy = bytesToRead;
            MEMCOPY_FUNC(src, dest, leftInTmapToCopy);
            len_copied += leftInTmapToCopy;
            dest = (void *) ((char *) dest + leftInTmapToCopy);
            bytesToRead -= leftInTmapToCopy;
        }

        /* continue to copy the first partial element into the shared
         * memory buffer
         */
        for (int tMapInd = ti + 1; tMapInd < datatype->num_pairs;
             tMapInd++) {

            if (datatype->type_map[tMapInd].size <= bytesToRead) {

                /* full data structure element fits in shared buffer */
                src =
                    (void *) ((char *) source +
                              datatype->type_map[tMapInd].offset);
                MEMCOPY_FUNC(src, dest, datatype->type_map[tMapInd].size);
                len_copied += datatype->type_map[tMapInd].size;
                dest =
                    (void *) ((char *) dest +
                              datatype->type_map[tMapInd].size);
                bytesToRead -= datatype->type_map[tMapInd].size;
                if (bytesToRead == 0)
                    break;
            } else {

                /* partial data structure element fits in shared buffer */
                src =
                    (void *) ((char *) source +
                              datatype->type_map[tMapInd].offset);
                MEMCOPY_FUNC(src, dest, bytesToRead);
                len_copied += bytesToRead;
                dest = (void *) ((char *) dest + bytesToRead);
                bytesToRead = 0;
                break;
            }
        }
        prev_dtype_cnt++;
    }

    /* end read of first element */
    /* read all possible full element */
    int nElementsToCopy = bytesToRead / datatype->packed_size;
    void *source =
        (void *) ((char *) sourcePointer + prev_dtype_cnt * datatype->extent);

    for (int dtElement = 0; dtElement < nElementsToCopy; dtElement++) {
        for (int tMapInd = 0; tMapInd < datatype->num_pairs; tMapInd++) {
            void *src =
                (void *) ((char *) source +
                          datatype->type_map[tMapInd].offset);
            MEMCOPY_FUNC(src, dest, datatype->type_map[tMapInd].size);
            len_copied += datatype->type_map[tMapInd].size;
            dest =
                (void *) ((char *) dest +
                          datatype->type_map[tMapInd].size);
            bytesToRead -= datatype->type_map[tMapInd].size;
        }
        source = (void *) ((char *) source + datatype->extent);
    }

    /* read partial last element in buffer */
    if (bytesToRead > 0) {

        for (int tMapInd = 0; tMapInd < datatype->num_pairs; tMapInd++) {

            if (datatype->type_map[tMapInd].size <= bytesToRead) {

                /* full data structure element fits in shared buffer */
                void *src =
                    (void *) ((char *) source +
                              datatype->type_map[tMapInd].offset);
                MEMCOPY_FUNC(src, dest, datatype->type_map[tMapInd].size);
                len_copied += datatype->type_map[tMapInd].size;
                dest =
                    (void *) ((char *) dest +
                              datatype->type_map[tMapInd].size);
                bytesToRead -= datatype->type_map[tMapInd].size;
                if (bytesToRead == 0)
                    break;

            } else {

                /* partial data structure element fits in shared buffer */
                void *src =
                    (void *) ((char *) source +
                              datatype->type_map[tMapInd].offset);
                MEMCOPY_FUNC(src, dest, bytesToRead);
                len_copied += bytesToRead;
                dest = (void *) ((char *) dest + bytesToRead);
                bytesToRead = 0;
                break;
            }
        }
    }

    *bytesCopied = len_copied;
    return ULM_SUCCESS;
}

#endif                          /* !_INLINE_COPY_FUNCTIONS */
