/*
 * Copyright 2002-2004. The Regents of the University of
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
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"

#include "client/daemon.h"
#include "client/SocketGeneric.h"
#include "queue/globals.h"

// data used to set startWithNewLine boolean of ClientWriteToServer
static int lastClientStderrFD = -1;
static int lastClientStdoutFD = -1;
static int lastNewLineLastToServerStderr = 0;
static int lastNewLineLastToServerStdout = 0;

static int readFromDescriptor(int ClientFD, int *ServerFD, int StdioFD,
                              int *NewLineLast, int lastInputFD,
                              int lastNewLine, char *IOPreFix,
                              int LenIOPreFix, ssize_t *lenWritten,
                              lampiState_t * state)
{
    ssize_t lenR;
    char ReadBuffer[ULM_MAX_TMP_BUFFER];
    bool startWithNewLine;

    do {
        lenR = read(ClientFD, ReadBuffer, ULM_MAX_TMP_BUFFER - 1);
    } while ((lenR < 0) && (errno == EINTR));


    if (lenR > 0) {
        startWithNewLine = false;
        ReadBuffer[lenR] = '\0';
        if ((lastInputFD != ClientFD) && (lastInputFD >= 0)) {
            if (!lastNewLine)
                startWithNewLine = true;
            else
                *NewLineLast = 1;
        }
        *lenWritten =
            ClientWriteToServer(ServerFD,
                                ReadBuffer,
                                (char *) IOPreFix,
                                LenIOPreFix,
                                NewLineLast,
                                StdioFD, lenR, startWithNewLine);
    }

    return lenR;
}


/*
 * This routine is used to scan the stdout/stderr pipes from the
 * children on this particular Client host, and forward the data to
 * the Server.
 */
int ClientScanStdoutStderr(lampiState_t *s)
{
    int *ServerFD = &(s->client->socketToServer_m);
    int i, RetVal, NumberReads, nfds;
    int maxfd;
    ulm_fd_set_t ReadSet;
    ssize_t lenR = 0;
    ssize_t lenW = 0;
    struct timeval WaitTime;
    WaitTime.tv_sec = 0;
    WaitTime.tv_usec = 10000;
    NumberReads = 0;

    /* check to see if there is any data to read */
    maxfd = 0;
    nfds = 0;
    ULM_FD_ZERO(&ReadSet);
    for (i = 0; i < s->local_size + 1; i++) {
        if (s->STDOUTfdsFromChildren[i] >= 0) {
            ULM_FD_SET(s->STDOUTfdsFromChildren[i], &ReadSet);
            maxfd = ULM_MAX(maxfd, s->STDOUTfdsFromChildren[i]);
            nfds++;
        }
    }
    for (i = 0; i < s->local_size + 1; i++) {
        if (s->STDERRfdsFromChildren[i] >= 0) {
            ULM_FD_SET(s->STDERRfdsFromChildren[i], &ReadSet);
            maxfd = ULM_MAX(maxfd, s->STDERRfdsFromChildren[i]);
            nfds++;
        }
    }
    if (s->commonAlivePipe[0] >= 0) {
        FD_SET(s->commonAlivePipe[0], &ReadSet);
        maxfd = ULM_MAX(maxfd, s->commonAlivePipe[0]);
        nfds++;
    }

    if (nfds == 0) {
        return 0;
    }

    RetVal = select(maxfd + 1, (fd_set *) &ReadSet, NULL, NULL, &WaitTime);
    if (RetVal <= 0) {
        return RetVal;
    }

    if ((s->commonAlivePipe[0] >= 0)
        && (ULM_FD_ISSET(s->commonAlivePipe[0], &ReadSet))) {
        /* all processes must have exited...pipe has been closed */
        for (i = 0; i < s->local_size; i++) {
            s->IAmAlive[i] = 0;
        }
        close(s->commonAlivePipe[0]);
        s->commonAlivePipe[0] = -1;
    }

    /* read data from standard error */
    for (i = 0; i < s->local_size + 1; i++) {
        if ((s->STDERRfdsFromChildren[i] > 0)
            && (ULM_FD_ISSET(s->STDERRfdsFromChildren[i], &ReadSet))) {
            /* clear s->STDERRfdsFromChildren[i] from ReadSet in case it is
             * also standard output */
            ULM_FD_CLR(s->STDERRfdsFromChildren[i], &ReadSet);
            lenR =
                readFromDescriptor(s->STDERRfdsFromChildren[i], ServerFD,
                                   STDERR_FILENO, s->NewLineLast + i,
                                   lastClientStderrFD,
                                   lastNewLineLastToServerStderr,
                                   s->IOPreFix[i], s->LenIOPreFix[i], &lenW,
                                   s);
            if (lenR > 0) {
                NumberReads++;
                if (lenW > 0) {
                    s->StderrBytesWritten += lenW;
                    lastClientStderrFD = s->STDERRfdsFromChildren[i];
                    lastNewLineLastToServerStderr = s->NewLineLast[i];
                }
            } else {
                // lenR <= 0
                close(s->STDERRfdsFromChildren[i]);
                s->STDERRfdsFromChildren[i] = -1;
            }
        }
    }

    /* read data from standard out */
    for (i = 0; i < s->local_size + 1; i++) {
        if ((s->STDOUTfdsFromChildren[i] > 0)
            && (ULM_FD_ISSET(s->STDOUTfdsFromChildren[i], &ReadSet))) {
            /* clear s->STDERRfdsFromChildren[i] from ReadSet in case it is
             * also standard output */
            ULM_FD_CLR(s->STDOUTfdsFromChildren[i], &ReadSet);
            lenR =
                readFromDescriptor(s->STDOUTfdsFromChildren[i], ServerFD,
                                   STDOUT_FILENO, s->NewLineLast + i,
                                   lastClientStdoutFD,
                                   lastNewLineLastToServerStdout,
                                   s->IOPreFix[i], s->LenIOPreFix[i], &lenW,
                                   s);
            if (lenR > 0) {
                NumberReads++;
                if (lenW > 0) {
                    s->StdoutBytesWritten += lenW;
                    lastClientStdoutFD = s->STDOUTfdsFromChildren[i];
                    lastNewLineLastToServerStdout = s->NewLineLast[i];
                }
            } else {
                close(s->STDOUTfdsFromChildren[i]);
                s->STDOUTfdsFromChildren[i] = -1;
            }
        }
    }

    return NumberReads;
}
