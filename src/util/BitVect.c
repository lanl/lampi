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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "BitVect.h"
#include "internal/malloc.h"

void bv_init(BitVect *bv, int size)
{
    bv->size = size;
    bv->vect = (unsigned char *)
        ulm_malloc(size * sizeof(unsigned char));
}

void bv_free(BitVect *bv)
{
    bv->size = 0;
    ulm_free(bv->vect);
    bv->vect = 0;
}

int bv_size(BitVect *bv)
{
    return bv->size * sizeof(unsigned char);
}

void bv_set(BitVect *bv, int x)
{
    unsigned char c = 1;
    int i = x / (sizeof(unsigned char) << 3);
    c <<= x % (sizeof(unsigned char) << 3);
    bv->vect[i] |= c;
}

void bv_setall(BitVect *bv)
{
    int i;
    for (i = 0; i < bv->size; i++) {
        bv->vect[i] = 255;
    }
}

int bv_isset(BitVect *bv, int x)
{
    unsigned char c = 1;
    int i = x / (sizeof(unsigned char) << 3);
    c <<= x % (sizeof(unsigned char) << 3);
    return (bv->vect[i] & c);
}

void bv_clear(BitVect *bv, int x)
{
    unsigned char c = 1;
    int i = x / (sizeof(unsigned char) << 3);
    c <<= x % (sizeof(unsigned char) << 3);
    bv->vect[i] &= ~c;
}

void bv_clearall(BitVect *bv)
{
    int i;
    for (i = 0; i < bv->size; i++) {
        bv->vect[i] = 0;
    }
}
