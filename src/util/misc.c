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

/*
 *  misc.c
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Fri Jan 24 2003.
 */

#include <signal.h>

#include "misc.h"
#include "internal/malloc.h"

void free_double_carray(char **arr, int len)
{
    int i;

    if (!arr)
        return;

    for (i = 0; i < len; i++)
        ulm_free2(arr[i]);
    ulm_free2(arr);
}


void free_double_iarray(int **arr, int len)
{
    int i;

    if (!arr)
        return;

    for (i = 0; i < len; i++)
        ulm_free2(arr[i]);
    ulm_free2(arr);
}


unsigned int ulm_log2(unsigned int n)
{
    int overflow;
    unsigned int cnt, nn;

    cnt = 0;
    nn = n;
    while (nn >>= 1) {
        cnt++;
    }
    overflow = (~(1 << cnt) & n) > 0;

    return cnt + overflow;

}

unsigned int bit_string_to_uint(const char *bstr)
{
    unsigned int num, i;
    const char *ptr;

    num = i = 0;
    ptr = bstr + strlen(bstr) - 1;
    while (ptr >= bstr) {
        num += ((*ptr - '0') << i);
        ptr--;
        i++;
    }
    return num;
}


char *uint_to_bit_string(unsigned int num, int pad)
{
    char *bstr;
    unsigned int idx, svd, digits;
    int i;

    bstr = malloc(sizeof(char) * 8 * sizeof(unsigned int) + 1);
    idx = 1 << (8 * sizeof(unsigned int) - 1);
    //   skip leading 0s
    while (!(idx & num) && idx)
        idx = idx >> 1;

    if (!idx) {
        if (!pad)
            strcpy(bstr, "0");
        else {
            for (i = 0; i < pad; i++)
                bstr[i] = '0';
            bstr[pad] = '\0';
        }
        return bstr;
    }

    svd = idx;
    digits = 1;
    while ((svd = svd >> 1))
        digits++;

    for (i = 0; i < (int) pad - (int) digits; i++)
        bstr[i] = '0';

    while (idx) {
        bstr[i++] = (idx & num) ? '1' : '0';
        idx = idx >> 1;
    }
    bstr[i] = '\0';

    return bstr;
}


void set_sa_restart(void) {
    /*
     *  set all signals to SA_RESTART.  Otherwise, handled or unhandled signals
     *  will cause wait(), read() & write() to return a -1 and errno==EINTR.
     */
    struct sigaction action;
    int signal=0;
    do {
        ++signal;
        if (0>sigaction(signal,NULL,&action)) break;   // invalid signal
        if (0==(SA_RESTART & action.sa_flags) ){       // set SA_RESTART
            action.sa_flags |= SA_RESTART;
            sigaction(signal,&action,NULL);
        }
        //fprintf(stderr,"sig=%i SA_RESTART = %i \n",signal,action.sa_flags & SA_RESTART);
        //fflush(stderr);
    } while (signal<1024);
}

