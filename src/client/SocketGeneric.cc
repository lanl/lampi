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

/*
 * This file contains generic socket based communications routines to be used
 * by mpirun and libmpi.  The basic model employed is that all send/recv
 *  trafic employ's iovec's.  On the send side the format of sending will be
 *    -authroization data
 *    -number of tag/data records sent in this request
 *      tag/data
 *      tag/data
 *    ....
 *   where the tags are defined in ulm_constants.h
 *  On the receive side, data is read in pairs, keeping track of when
 *  next to check for authorization data.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/uio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/SocketGeneric.h"
#include "run/Run.h"
#include "ulm/errors.h"


#define MAXIOVEC 100

/*
 * Send data over an already established socket connection.
 */
ssize_t SendSocket(int DestinationFD, int NumRecordsToSend,
                   ulm_iovec_t *InputSendData)
{
    ssize_t RetVal;

    RetVal = ulm_writev(DestinationFD, InputSendData, NumRecordsToSend);

    return RetVal;
}


/*
 * Read data from a given socket
 */
ssize_t RecvSocket(int SourceFD, void *OutputBuffer,
                         size_t SizeOfInputBuffer, int *error)
{
    ssize_t RetVal, TotalRead;
    ulm_iovec_t RecvIovec[2];
    char *BufferPtr;

    *error = ULM_SUCCESS;

    /* read data */
    TotalRead = 0;
    RetVal = 0;
    while (TotalRead < (ssize_t) SizeOfInputBuffer) {
        BufferPtr = (char *) OutputBuffer + TotalRead;
        RecvIovec[0].iov_base = (void *) BufferPtr;
        RecvIovec[0].iov_len = SizeOfInputBuffer - TotalRead;
        RetVal = ulm_readv(SourceFD, RecvIovec, 1);
        if ((RetVal < 0) && (errno != EINTR)) {
            *error = errno;
            if (errno != ECONNRESET)
                ulm_err(("return from readv %d errno %d pid %u\n",
                         RetVal, errno,getpid()));
            break;
        } else if (RetVal == 0) {
            break;
        }
        TotalRead += (RetVal < 0) ? 0 : RetVal;
    }

    return TotalRead;
}


/*
 *  This routine interupts data sent from the "application" processes
 *  prepends a prefix at the begining of each line, and forwards the
 *  string to mpirun.
 */
ssize_t ClientWriteToServer(int *ServerFD, char *String, char *PrependString,
                            int lenPrependString, int *NewLineLast,
                            int StdioFD, ssize_t lenString,
                            bool startWithNewLine)
{
    int niov;
    size_t len, LeftToWrite, LenToWrite;
    caddr_t PtrToNewLine, PtrToStartWrite;
    ulm_iovec_t send[MAXIOVEC];
    ssize_t TotalBytesSent, n, nsend;
    unsigned int tag = STDIOMSG;
    int bytes = 0;
    bool again;

    TotalBytesSent = 0;

    niov = 0;
    send[niov].iov_base = &tag;
    send[niov].iov_len = sizeof(tag);
    niov++;
    send[niov].iov_base = &StdioFD;
    send[niov].iov_len = sizeof(StdioFD);
    niov++;
    send[niov].iov_base = &bytes;
    send[niov].iov_len = sizeof(bytes);
    niov++;

    if (startWithNewLine) {
        char newline_char = '\n';
        send[niov].iov_base = (void *) &newline_char;
        send[niov].iov_len = (ssize_t) sizeof(newline_char);
        niov++;
        bytes += sizeof(char);
        *NewLineLast = 1;
    }

    if (lenPrependString <= 0)
        goto NOPREPEND;

    /* initialize data */
    /* if last character to print out was new line - startout with new-line */
    if ((*NewLineLast)) {
        send[niov].iov_base = (void *) PrependString;
        send[niov].iov_len = lenPrependString;
        niov++;
        bytes += lenPrependString;
    }
    *NewLineLast = 0;

    /* write out buffer */
    len = strlen(String);
    if ((ssize_t) len > lenString)
        len = lenString;
    LeftToWrite = len;
    PtrToStartWrite = (caddr_t) String;

CONTINUE:;

    /* each iteration may generate 2 iovec's */
    while ((LeftToWrite > 0) && (niov < (MAXIOVEC - 1))) {
        PtrToNewLine = strchr(String + (len - LeftToWrite), '\n');
        if (PtrToNewLine != NULL) {
            LenToWrite = PtrToNewLine - PtrToStartWrite + 1;
            send[niov].iov_base = (void *) PtrToStartWrite;
            send[niov].iov_len = LenToWrite;
            niov++;
            bytes += LenToWrite;
            PtrToStartWrite += LenToWrite;
            LeftToWrite -= LenToWrite;
            if (PtrToStartWrite == (caddr_t) (String + len))
                *NewLineLast = 1;
            if (LeftToWrite > 0) {
                send[niov].iov_base = (void *) PrependString;
                send[niov].iov_len = lenPrependString;
                niov++;
                bytes += lenPrependString;
            }
        } else {
            LenToWrite = (size_t) (String + len - PtrToStartWrite);
            send[niov].iov_base = (void *) PtrToStartWrite;
            send[niov].iov_len = LenToWrite;
            niov++;
            bytes += LenToWrite;
            LeftToWrite -= LenToWrite;
        }
    }

    do {
        again = false;
        /* write out data */
        nsend = ulm_writev(*ServerFD, send, niov);
        if ((nsend < 0) && (errno == EINTR)) {
            again = true;
        }
    } while (again);

    if (nsend <= 0) {
        close(*ServerFD);
        *ServerFD = -1;
        return nsend;
    }

    TotalBytesSent += bytes;
    if (LeftToWrite > 0) {
        niov = 3;
        bytes = 0;
        goto CONTINUE;
    }

    /* return total bytes sent */
    return TotalBytesSent;

NOPREPEND:;
    n = (ssize_t) strlen(String);
    if (n > lenString)
        n = (ssize_t) lenString;

    send[niov].iov_base = (void *) String;
    send[niov].iov_len = (ssize_t) (n);
    niov++;
    bytes += n;

    do {
        again = false;
        nsend = ulm_writev(*ServerFD, send, niov);
        if ((nsend < 0) && (errno == EINTR)) {
            again = true;
        }
    } while (again);

    if (nsend <= 0) {
        close(*ServerFD);
        *ServerFD = -1;
        return nsend;
    }

    return (ssize_t) n;
}
