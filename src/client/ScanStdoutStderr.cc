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

#include "client/ULMClient.h"
#include "client/SocketGeneric.h"
#include "queue/globals.h"

// data used to set startWithNewLine boolean of ClientWriteToServer
static int lastInputFDToServerStdout = -1;
static int lastInputFDToServerStderr = -1;
static int lastNewLineLastToServerStdout = 0;
static int lastNewLineLastToServerStderr = 0;


static int readFromDescriptor(int fd, int *NewLineLast,
                              int ToServerFD, int lastInputFD,
                              int lastNewLine, char *IOPreFix,
                              int LenIOPreFix, ssize_t *lenWritten, lampiState_t *state)
{
    ssize_t lenR;
    char 	ReadBuffer[ULM_MAX_TMP_BUFFER];
    bool 	startWithNewLine;

    do {
        lenR = read(fd, ReadBuffer, ULM_MAX_TMP_BUFFER - 1);
    } while ((lenR < 0) && (errno == EINTR));

    if (lenR > 0)
    {
        startWithNewLine = false;
        ReadBuffer[lenR] = '\0';
        if ((lastInputFD != fd) && (lastInputFD >= 0))
        {
            if (!lastNewLine)
                startWithNewLine = true;
            else
                *NewLineLast = 1;
        }
#ifdef ENABLE_CT
        *lenWritten =
            ClientWriteIOToServer(ReadBuffer,
                                  (char *) IOPreFix,
                                  LenIOPreFix,
                                  NewLineLast,
                                  ToServerFD, lenR,
                                  startWithNewLine, state);
#else
        *lenWritten =
            ClientWriteToServer(ReadBuffer,
                                (char *) IOPreFix,
                                LenIOPreFix,
                                NewLineLast,
                                ToServerFD, lenR,
                                startWithNewLine);
#endif
    }

    return lenR;
}





/*
 * This routine is used to scan the stdout/stderr pipes from the
 * children on this particular Client host, and forward the data to
 * the Server.
 */
int ClientScanStdoutStderr(int *ClientStdoutFDs, int *ClientStderrFDs,
                           int ToServerStdoutFD, int ToServerStderrFD,
                           int NFDs, int MaxDescriptor,
                           PrefixName_t *IOPreFix, int *LenIOPreFix,
                           size_t *StderrBytesWritten,
                           size_t *StdoutBytesWritten, int *NewLineLast,
                           lampiState_t *state)
{
    int i, RetVal, NumberReads, nfds;
    fd_set ReadSet;
    ssize_t lenR, lenW;
    struct timeval WaitTime;
    int		*daemonNewLineLast = state->daemonNewLineLast;
    WaitTime.tv_sec = 0;
    WaitTime.tv_usec = 10000;
    NumberReads = 0;

    /* check to see if there is any data to read */
    nfds = 0;
    FD_ZERO(&ReadSet);
    for (i = 0; i < NFDs; i++) {
        if (ClientStdoutFDs[i] >= 0) {
            FD_SET(ClientStdoutFDs[i], &ReadSet);
            nfds++;
        }
    }
    for (i = 0; i < NFDs; i++) {
        if (ClientStderrFDs[i] >= 0) {
            FD_SET(ClientStderrFDs[i], &ReadSet);
            nfds++;
        }
    }

    if (state->daemonSTDERR[0] >= 0)
    {
        FD_SET(state->daemonSTDERR[0], &ReadSet);
        nfds++;
        if ( state->daemonSTDERR[0] > MaxDescriptor )
            MaxDescriptor = state->daemonSTDERR[0] + 1;
    }

    if (state->daemonSTDOUT[0] >= 0)
    {
        FD_SET(state->daemonSTDOUT[0], &ReadSet);
        nfds++;
        if ( state->daemonSTDOUT[0] > MaxDescriptor )
            MaxDescriptor = state->daemonSTDOUT[0] + 1;
    }

    if (nfds == 0)
        return 0;

    RetVal = select(MaxDescriptor, &ReadSet, NULL, NULL, &WaitTime);
    if (RetVal <= 0)
        return RetVal;

    /*!!!! NewLineLast should really be 2*NFDs long to have a separate flag for
        * the standard output and error descriptors! -Mitch
        */
#ifdef ENABLE_CT
    ToServerStderrFD = STDERR_FILENO;
    ToServerStdoutFD = STDOUT_FILENO;
#endif

    /* read data from standard error */
    for (i = 0; i < NFDs; i++)
    {
        if ((ClientStderrFDs[i] > 0)
            && (FD_ISSET(ClientStderrFDs[i], &ReadSet)))
        {
            /* clear ClientStderrFDs[i] from ReadSet in case it is also standard output */
            FD_CLR(ClientStderrFDs[i], &ReadSet);
            lenR = readFromDescriptor(ClientStderrFDs[i], NewLineLast + i,
                                      ToServerStderrFD, lastInputFDToServerStderr,
                                      lastNewLineLastToServerStderr, IOPreFix[i],
                                      LenIOPreFix[i], &lenW, state);
            if (lenR > 0)
            {
                NumberReads++;
                if (lenW > 0)
                {
                    (*StderrBytesWritten) += lenW;
                    lastInputFDToServerStderr = ClientStderrFDs[i];
                    lastNewLineLastToServerStderr = NewLineLast[i];
                }
            }
            else
            {
                // lenR <= 0
                close(ClientStderrFDs[i]);
                ClientStderrFDs[i] = -1;
                lampiState.IAmAlive[i] = 0;
            }
        }
    }

    /* read data from standard out */
    for (i = 0; i < NFDs; i++)
    {
        if ((ClientStdoutFDs[i] > 0)
            && (FD_ISSET(ClientStdoutFDs[i], &ReadSet)))
        {
            /* clear ClientStderrFDs[i] from ReadSet in case it is also standard output */
            FD_CLR(ClientStdoutFDs[i], &ReadSet);
            lenR = readFromDescriptor(ClientStdoutFDs[i], NewLineLast + i,
                                      ToServerStdoutFD, lastInputFDToServerStdout,
                                      lastNewLineLastToServerStdout, IOPreFix[i],
                                      LenIOPreFix[i], &lenW, state);
            if (lenR > 0)
            {
                NumberReads++;
                if (lenW > 0)
                {
                    (*StdoutBytesWritten) += lenW;
                    lastInputFDToServerStdout = ClientStdoutFDs[i];
                    lastNewLineLastToServerStdout = NewLineLast[i];
                }
            }
            else
            {
                close(ClientStdoutFDs[i]);
                ClientStdoutFDs[i] = -1;
                lampiState.IAmAlive[i] = 0;
            }
        }
    }

    /* process daemon output. */
    if ((state->daemonSTDERR[0] > 0)
        && (FD_ISSET(state->daemonSTDERR[0], &ReadSet)))
    {
        /* clear ClientStderrFDs[i] from ReadSet in case it is also standard output */
        FD_CLR(state->daemonSTDERR[0], &ReadSet);
        lenR = readFromDescriptor(state->daemonSTDERR[0], &daemonNewLineLast[0],
                                  ToServerStderrFD, lastInputFDToServerStdout,
                                  lastNewLineLastToServerStderr, NULL,
                                  0, &lenW, state);
        if (lenR > 0)
        {
            NumberReads++;
            if (lenW > 0)
            {
                (*StderrBytesWritten) += lenW;
                lastInputFDToServerStderr = state->daemonSTDERR[0];
                lastNewLineLastToServerStderr = daemonNewLineLast[0];
            }
        }
        else
        {
            close(state->daemonSTDERR[0]);
            state->daemonSTDERR[0] = -1;
        }
    }

    if ((state->daemonSTDOUT[0] > 0)
        && (FD_ISSET(state->daemonSTDOUT[0], &ReadSet)))
    {
        /* clear ClientStderrFDs[i] from ReadSet in case it is also standard output */
        FD_CLR(state->daemonSTDOUT[0], &ReadSet);
        lenR = readFromDescriptor(state->daemonSTDOUT[0], &daemonNewLineLast[1],
                                  ToServerStdoutFD, lastInputFDToServerStdout,
                                  lastNewLineLastToServerStdout, NULL,
                                  0, &lenW, state);
        if (lenR > 0)
        {
            NumberReads++;
            if (lenW > 0)
            {
                (*StdoutBytesWritten) += lenW;
                lastInputFDToServerStdout = state->daemonSTDOUT[0];
                lastNewLineLastToServerStdout = daemonNewLineLast[1];
            }
        }
        else
        {
            close(state->daemonSTDOUT[0]);
            state->daemonSTDOUT[0] = -1;
        }
    }

    return NumberReads;
}
