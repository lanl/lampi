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
 * License as published by the Free Softaware Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <string.h>

#include "internal/linkage.h"
#include "ulm/types.h"

/*
 * type_copy - Copy a data type
 *
 * \param dest          output data type array
 * \param src           input data type array
 * \param count         size of array
 * \param type          type descriptor
 *
 */
CDECL_STATIC_INLINE
void type_copy(void *dest, const void *src, size_t count, ULMType_t *type)
{
    if (type == NULL) {
        memcpy(dest, src, count);
    } else if (type->layout == CONTIGUOUS) {
        memcpy(dest, src, count * type->extent);
    } else {
        unsigned char *p = ((unsigned char *) dest);
        unsigned char *q = ((unsigned char *) src);
        int map;

        while (count--) {
            for (map = 0; map < type->num_pairs; map++) {
                memcpy(p + type->type_map[map].offset,
                       q + type->type_map[map].offset,
                       type->type_map[map].size);
            }
            p += type->extent;
            q += type->extent;
        }
    }
}

/*
 * type_pack -- Incrementally copy data type arrays to/from a packed buffer
 *
 * \param pack          direction of copy: PACK or UNPACK
 * \param buf           buffer to pack into/unpack from
 * \param bufsize       size of buffer
 * \param offset        pointer to current byte offset into the buffer
 * \param typebuf       array of types
 * \param ntype         size of type array
 * \param type          type descriptor
 * \param type_index    pointer to index of current type
 * \param map_index     pointer to index of current type map
 * \param map_offset    pointer to byte offset into current type map
 * \return              0 if pack/unpack is complete, non-zero otherwise
 *
 * Incrementally copy data type arrays to/from a packed buffer.  by
 * iterating over the type and type_map until we finish or run out of
 * room.
 *
 * The contents of type_index, map_index and map_offset should be
 * initialized to 0 before the first call.
 */

enum {
    TYPE_PACK_PACK = 0,
    TYPE_PACK_UNPACK,
    TYPE_PACK_COMPLETE = 0,
    TYPE_PACK_INCOMPLETE_VECTOR,
    TYPE_PACK_INCOMPLETE_TYPE,
    TYPE_PACK_INCOMPLETE_TYPE_MAP
};

CDECL_STATIC_INLINE
int type_pack(int pack,
              void *buf, size_t bufsize, size_t * offset,
              void *typebuf, size_t ntype, ULMType_t * type,
              size_t * type_index, size_t * map_index,
              size_t * map_offset)
{
    char *ptr = (char *) typebuf + *type_index * type->extent;
    char *b = (char *) buf + *offset;
    int returnValue = TYPE_PACK_COMPLETE;

    bufsize -= (*offset);
    if (type->layout == CONTIGUOUS) {
        size_t copiedSoFar, bytesToCopy, leftToCopy;
        char *copyPtr = ptr + *map_offset;

        copiedSoFar = (*type_index) * type->extent + (*map_offset);
        bytesToCopy = bufsize;
        leftToCopy = ntype * type->extent - copiedSoFar;
        if (bytesToCopy > leftToCopy) {
            bytesToCopy = leftToCopy;
        }
        if (pack == TYPE_PACK_PACK) {
            memcpy(b, copyPtr, bytesToCopy);
        } else if (pack == TYPE_PACK_UNPACK) {
            memcpy(copyPtr, b, bytesToCopy);
        } else {
            return -1;
        }
        copiedSoFar += bytesToCopy;
        (*offset) += bytesToCopy;
        (*type_index) = copiedSoFar / type->extent;
        (*map_offset) = copiedSoFar - (*type_index) * type->extent;
        if (copiedSoFar != (ntype * type->extent)) {
            returnValue = TYPE_PACK_INCOMPLETE_VECTOR;
        }
    } else {
        while (*type_index < ntype) {
            while (*map_index < (size_t) type->num_pairs) {
                char *t = ptr + type->type_map[*map_index].offset;
                size_t size = type->type_map[*map_index].size;

                if (*map_offset > 0) {
                    t += *map_offset;
                    size -= *map_offset;
                    if (size <= bufsize) {
                        *map_offset = 0;
                    }
                }
                if (size > bufsize) {
                    size = bufsize;
                    *map_offset += size;
                }
                if (pack == TYPE_PACK_PACK) {
                    memcpy(b, t, size);
                } else if (pack == TYPE_PACK_UNPACK) {
                    memcpy(t, b, size);
                } else {
                    return -1;
                }
                *offset += size;
                if (*map_offset > 0) {
                    return TYPE_PACK_INCOMPLETE_TYPE_MAP;
                }
                bufsize -= size;
                b += size;
                *map_index += 1;
                if (bufsize == 0 && *map_index < (size_t) type->num_pairs) {
                    return TYPE_PACK_INCOMPLETE_TYPE;
                }
            }
            ptr += type->extent;
            *type_index += 1;
            *map_index = 0;
            if (bufsize == 0 && *type_index < ntype) {
                return TYPE_PACK_INCOMPLETE_VECTOR;
            }
        }
    }

    return returnValue;
}


/*
 * type_dump - Dump out a datatype to stderr (for debugging)
 *
 * \param type          pointer to datatype
 */
CDECL_STATIC_INLINE
void type_dump(ULMType_t * type)
{
    int i;

    fprintf(stderr,
            "type_dump: type at %p:\n"
            "\tlower_bound = %ld\n"
            "\textent = %ld\n"
            "\tpacked_size = %ld\n"
            "\tnum_pairs = %d\n"
            "\tlayout = %d\n"
            "\tisbasic = %d\n"
            "\tnum_primitives = %d\n"
            "\tsecond_primitive_offset = %d\n"
            "\top_index = %d\n"
            "\tfhandle = %d\n"
            "\ttype_map =",
            type,
            (long) type->lower_bound,
            (long) type->extent,
            (long) type->packed_size,
            type->num_pairs,
            type->layout,
            type->isbasic,
            type->num_primitives,
            type->second_primitive_offset,
            type->op_index,
            type->fhandle);
    for (i = 0; i < type->num_pairs; i++) {
        fprintf(stderr,
                " (%ld, %ld, %ld)",
                (long) type->type_map[i].size,
                (long) type->type_map[i].offset,
                (long) type->type_map[i].seq_offset);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}


#ifdef TEST

/*
 * A simple test program for type_pack()
 *
 * Sets up a simple non-contiguous datatype and then packs and unpacks
 * it using type_pack() with different increments for the pack and
 * unpack, and then checks that the data is recovered correctly.
 */

#include <stdio.h>
#include <stdlib.h>

enum {
    LOWER_BOUND = 8,            /* lower bound for test datatype */
    NTYPE = 73,                 /* number of types in the array */
    PACK_INCREMENT = 3,         /* increment for packing */
    UNPACK_INCREMENT = 17,      /* increment for unpacking */
    VERBOSE = 1                 /* print out return values */
};

void hexdump_buffer(const void *p, size_t n)
{
    unsigned char *c = (unsigned char *) p;

    printf("[%p]", p);
    while (n--) {
        printf("%02x:", *c++);
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    char *p;
    int i;
    int rc;
    size_t blocksize;
    size_t map_index;
    size_t map_offset;
    size_t offset;
    size_t type_index;
    void *buf;
    void *typebuf1;
    void *typebuf;
    ULMType_t type[1];
    ULMTypeMapElt_t tmap[3] = {
        {sizeof(int), LOWER_BOUND, 0},
        {sizeof(char), LOWER_BOUND + 12, sizeof(int)},
        {sizeof(long long), LOWER_BOUND + 16, sizeof(int) + sizeof(char)}
    };

    type->type_map = tmap;
    type->lower_bound = LOWER_BOUND;
    type->extent = 16 + sizeof(long long);
    type->packed_size = sizeof(int) + sizeof(char) + sizeof(long long);
    type->num_pairs = 3;
    type->layout = NON_CONTIGUOUS;
    type->isbasic = 0;
    type->op_index = -1;
    type->fhandle = -1;

    blocksize = type->extent;
    typebuf = alloca(NTYPE * blocksize);
    typebuf1 = alloca(NTYPE * blocksize);
    buf = alloca(NTYPE * type->packed_size);

    memset(typebuf, 0, NTYPE * blocksize);

    for (i = 0; i < NTYPE; i++) {
        unsigned char *p;
        p = (unsigned char *) typebuf + i * blocksize;
        *((int *) (p + type->type_map[0].offset)) = (int) i + 0xAABB0000;
        *((char *) (p + type->type_map[1].offset)) = (char) i + 64;
        *((long long *) (p + type->type_map[2].offset)) =
            (long long) i + 0xAABBCCDDEEFF0000;
    }

    if (VERBOSE) {
        hexdump_buffer(typebuf, NTYPE * blocksize);
    }

    memset(buf, 0, NTYPE * type->packed_size);
    offset = 0;
    type_index = 0;
    map_index = 0;
    map_offset = 0;
    p = buf;
    do {
        rc = type_pack(TYPE_PACK_PACK,
                       p, PACK_INCREMENT, &offset,
                       typebuf, NTYPE, type,
                       &type_index, &map_index, &map_offset);
        p += PACK_INCREMENT;

        if (VERBOSE) {
            printf
                ("rc = %d, type_index = %d, map_index = %d, map_offset = %d\n",
                 rc, type_index, map_index, map_offset);
            hexdump_buffer(buf, NTYPE * type->packed_size);
        }
    } while (rc != TYPE_PACK_COMPLETE);

    offset = 0;
    type_index = 0;
    map_index = 0;
    map_offset = 0;
    memset(typebuf1, 0, NTYPE * blocksize);
    p = buf;

    do {
        rc = type_pack(TYPE_PACK_UNPACK,
                       p, UNPACK_INCREMENT, &offset,
                       typebuf1, NTYPE, type,
                       &type_index, &map_index, &map_offset);
        p += UNPACK_INCREMENT;

        if (VERBOSE) {
            printf
                ("rc = %d, type_index = %d, map_index = %d, map_offset = %d\n",
                 rc, type_index, map_index, map_offset);
            hexdump_buffer(typebuf1, NTYPE * blocksize);
        }

    } while (rc != TYPE_PACK_COMPLETE);

    if (memcmp(typebuf, typebuf1, NTYPE * blocksize) == 0) {
        puts("SUCCESS");
    } else {
        puts("FAILURE");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#endif                          /* TEST */
