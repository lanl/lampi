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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>

#include "internal/constants.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "util/Utility.h"
#include "client/SocketGeneric.h"

int parseString(char ***ArgList,
                const char* InputBuffer,
                const int NSeparators,
                const char *SeparatorList)
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

    /*
      Example: InputBuffer = "foo:bar/moo;cow", SeparatorList = ":;/'", NSeparators = 4
      The function returns the array {"foo", "bar", "moo", cow"} in ArgList.
    */

    int i, j, cnt, InString;
    size_t len;
    char *StrtString, *EndString;
    int ThisIsSeprator;
    char Char;

    cnt = 0;
    InString = 0;
    StrtString = (char *) InputBuffer;
    EndString = StrtString + strlen(InputBuffer);

    for (i = 0; i < (long) strlen(InputBuffer); i++) {
        Char = *(InputBuffer + i);
        ThisIsSeprator = 0;
        for (j = 0; j < NSeparators; j++) {
            if (SeparatorList[j] == Char) {
                ThisIsSeprator = 1;
                break;
            }
        }
        if (ThisIsSeprator) {   /* process character */
            if (InString) {
                EndString = (char *) InputBuffer + i;
                cnt++;
                InString = 0;
            }
        } else {
            if (!InString) {
                InString = 1;
                StrtString = (char *) InputBuffer + i;
                if (i == (long) (strlen(InputBuffer) - 1)) {    /* end of string code */
                    EndString = (char *) InputBuffer + i;
                    cnt++;
                }
            } else {            /* end of string check */
                if (i == (long) (strlen(InputBuffer) - 1)) {
                    EndString = (char *) InputBuffer + i;
                    cnt++;
                }
            }
        }                       /* end processing character */
    }

    // return if cnt == 0
    if (cnt == 0)
        return cnt;

    *ArgList = (char **) ulm_malloc(sizeof(char *) * cnt);
    if ((*ArgList) == NULL) {
        printf("Error: can't allocate memory in parser. Len %ld\n",
               (long) (sizeof(char *) * cnt));
        perror(" Memory allocation failure ");
        exit(EXIT_FAILURE);
    }
    cnt = 0;
    InString = 0;
    StrtString = (char *) InputBuffer;
    EndString = StrtString + strlen(InputBuffer);

    for (i = 0; i < (long) strlen(InputBuffer); i++) {
        Char = *(InputBuffer + i);
        ThisIsSeprator = 0;
        for (j = 0; j < NSeparators; j++) {
            if (SeparatorList[j] == Char) {
                ThisIsSeprator = 1;
                break;
            }
        }
        if (ThisIsSeprator) {   /* process character */
            if (InString) {
                EndString = (char *) InputBuffer + i;
                len = (EndString - StrtString) / sizeof(char);
                (*ArgList)[cnt] =
                    (char *) ulm_malloc(sizeof(char) * (len + 1));
                memset((*ArgList)[cnt], 0, sizeof(char) * (len + 1));
                strncpy((*ArgList)[cnt++], StrtString, len);
                InString = 0;
            }
        } else {
            if (!InString) {
                InString = 1;
                StrtString = (char *) InputBuffer + i;
                if (i == (long) (strlen(InputBuffer) - 1)) {    /* end of string code */
                    EndString = (char *) InputBuffer + i;
                    len = (EndString - StrtString) / sizeof(char) + 1;
                    (*ArgList)[cnt] =
                        (char *) ulm_malloc(sizeof(char) * (len + 1));
                    memset((*ArgList)[cnt], 0, sizeof(char) * (len + 1));
                    strncpy((*ArgList)[cnt++], StrtString, len);
                }
            } else {            /* end of string check */
                if (i == (long) (strlen(InputBuffer) - 1)) {
                    EndString = (char *) InputBuffer + i;
                    len = (EndString - StrtString) / sizeof(char) + 1;
                    (*ArgList)[cnt] =
                        (char *) ulm_malloc(sizeof(char) * (len + 1));
                    memset((*ArgList)[cnt], 0, sizeof(char) * (len + 1));
                    strncpy((*ArgList)[cnt++], StrtString, len);
                }
            }
        }                       /* end processing character */
    }

    return cnt;
}


int NStringArgs(char *InputBuffer, int NSeparators,
                char *SeparatorList)
{
    int i, j, cnt;
    int InString;
    int ThisIsSeprator;
    char Char;

    cnt = 0;
    InString = 0;

    for (i = 0; i < (long) strlen(InputBuffer); i++) {
        Char = *(InputBuffer + i);
        ThisIsSeprator = 0;
        for (j = 0; j < NSeparators; j++) {
            if (SeparatorList[j] == Char) {
                ThisIsSeprator = 1;
                break;
            }
        }
        if (ThisIsSeprator) {   /* process character */
            if (InString) {
                cnt++;
                InString = 0;
            }
        } else {
            if (!InString) {
                InString = 1;
                if (i == (long) (strlen(InputBuffer) - 1)) {    /* end of string code */
                    cnt++;
                }
            } else {            /* end of string check */
                if (i == (long) (strlen(InputBuffer) - 1)) {
                    cnt++;
                }
            }
        }                       /* end processing character */
    }

    return cnt;
}

void FreeStringMem(char ***ArgList, int NArgs)
{
    int i;
    for (i = 0; i < NArgs; i++)
        ulm_free((*ArgList)[i]);
    ulm_free((*ArgList));
}


/*
 * This routine is used to send a heartbeat message on each of n
 * sockets.  All that is sent is a tag.  To avoid clock
 * synchronization problems, the receiving end supplies the time
 * stamp.
 */
int SendHeartBeat(int *fd, int n)
{
    int maxfd;
    int rc;
    struct timeval tv;
    ulm_fd_set_t fdset;
    ulm_iovec_t iov[2];
    unsigned int tag;

    tag = HEARTBEAT;
    iov[0].iov_base = &tag;
    iov[0].iov_len = (ssize_t) (sizeof(unsigned int));
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    bzero(&fdset, sizeof(fdset));

    maxfd = 0;
    for (int i = 0; i < n; i++) {
        if (fd[i] >= 0) {
            FD_SET(fd[i], (fd_set *) & fdset);
            maxfd = ULM_MAX(maxfd, fd[i]);
        }
    }

    rc = select(maxfd + 1, NULL, (fd_set *) &fdset, NULL, &tv);
    if (rc < 0) {
        return rc;
    }

    /* send heart beat on each socket */
    if (rc > 0) {
        for (int i = 0; i < n; i++) {
            if ((fd[i] >= 0) && (FD_ISSET(fd[i], (fd_set *) &fdset))) {
                if (SendSocket(fd[i], 1, iov) < 0)
                    return -1;
            }
        }
    }

    return 0;
}
