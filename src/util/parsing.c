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

/*
 *  parsing.c
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Sat Jan 18 2003.
 */

#include "parsing.h"
#include "linked_list.h"
#include "internal/malloc.h"

#ifndef __mips
inline
#endif
static void _ulm_parse_add(char **substr, int cnt, const char *cptr,
                           int len)
{
    substr[cnt] = (char *) ulm_malloc(sizeof(char) * (len + 1));
    if (NULL == substr[cnt]) {
        printf("Error: can't allocate memory in parser. Len %ld\n",
               (long) len);
        perror(" Memory allocation failure ");
        exit(-1);
    }
    /* copy substring. */
    strncpy(substr[cnt], cptr, len);
    substr[cnt][len] = '\0';
}


int _ulm_parse_string(char ***ArgList, const char *InputBuffer,
                      const int NSeparators, const char *SeparatorList)
{
    /*
       This function will parse the input string in InputBuffer and
       return an array of substrings that are separated by the characters
       identified by SeparatorList.
       PRE:     SeparatorList is an array of length NSeparators where each
       character is a delimiter for InputBuffer.
       ArgList is a reference parameter for an array of strings.
       POST:    function will create an array of substrings that are
       delimited by the chars in SeparatorList.  Caller is responsible
       for freeing the memory.
     */
    const char *cptr, *ptr;
    int substrCnt, cnt, totalCnt, len, blocks, i;
    char **substr;

    /* Make an estimate of how many substrings there could be.
       This is a conservative estimate of the upper bound.
       Guess that the worst case is all substrings are of length 3.
     */
    substrCnt = strlen(InputBuffer) / 3;
    substrCnt = (!substrCnt) ? 1 : substrCnt;   // alloc at least 1 string
    *ArgList = (char **) ulm_malloc(sizeof(char *) * substrCnt);
    if (NULL == *ArgList) {
        printf("Error: can't allocate memory in parser. Len %ld\n",
               (long) (sizeof(char *) * substrCnt));
        perror(" Memory allocation failure ");
        exit(-1);
    }
    bzero(*ArgList, substrCnt);
    blocks = 1;

    /* parse InputBuffer. */
    ptr = cptr = InputBuffer;
    totalCnt = cnt = 0;
    substr = *ArgList;
    while (*ptr) {

        for (i = 0; i < NSeparators; i++)
            if (*ptr == SeparatorList[i])
                break;

        if (i < NSeparators) {
            /* encountered delimiter */
            len = ptr - cptr;
            if (len) {
                _ulm_parse_add(substr, cnt, cptr, len);
                totalCnt++;

                if (++cnt >= substrCnt) {
                    blocks++;
                    len = substrCnt * blocks;
                    *ArgList = realloc(*ArgList, sizeof(char *) * len);
                    substr = *ArgList + len - substrCnt;
                    if (NULL == *ArgList) {
                        printf
                            ("Error: can't allocate memory in parser. Len %ld\n",
                             (long) (sizeof(char *) * substrCnt * blocks));
                        perror(" Memory allocation failure ");
                        exit(-1);
                    }
                    cnt = 0;
                }
            }
            cptr = ptr + 1;
        }
        ptr++;
    }
    /* check for last substring; InputBuffer may not end with
       a delimiter. */
    if ((len = (ptr - cptr))) {
        _ulm_parse_add(substr, cnt, cptr, len);
        totalCnt++;
    }

    return totalCnt;
}
