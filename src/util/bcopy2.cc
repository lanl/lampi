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

#include <string.h>
#include <stdio.h>

#include "internal/system.h"

void bcopy2(const void *source, void *destination, unsigned long copylen)
{
    unsigned int *RESTRICT_MACRO src = (unsigned int *) source;
    unsigned int *RESTRICT_MACRO dest = (unsigned int *) destination;
    unsigned int temp;
    unsigned long long i;

    int sourceAligned = intwordaligned(source);
    int destAligned = intwordaligned(destination);
    if (sourceAligned && destAligned) {
#ifdef _COUNT_ME_
        _AA++;
#endif
        unsigned long long numInts = copylen >> 2;

        for (i = 0; i < numInts; i++) {
            *dest++ = *src++;
        }
        copylen -= numInts * sizeof(unsigned int);
    } else if (sourceAligned) {
#ifdef _COUNT_ME_
        _AU++;
#endif
        for (; copylen >= sizeof(*src); copylen -= sizeof(*src)) {
            temp = *src++;
            memcpy(dest, &temp, sizeof(temp));
            dest++;
        }
    } else if (destAligned) {
#ifdef _COUNT_ME_
        _UA++;
#endif
        for (; copylen >= sizeof(*src); copylen -= sizeof(*src)) {
            memcpy(&temp, src, sizeof(temp));
            src++;
            *dest++ = temp;
        }
    } else {
#ifdef _COUNT_ME_
        _UU++;
#endif
        for (; copylen >= sizeof(*src); copylen -= sizeof(*src)) {
            memcpy(&temp, src, sizeof(temp));
            src++;
            memcpy(dest, &temp, sizeof(temp));
            dest++;
        }
    }

    /* if copylen is non-zero there was a bit left, less than an unsigned long's worth */

    if (copylen != 0) {
        memcpy(&temp, src, copylen);
        memcpy(dest, &temp, copylen);
    }
}
