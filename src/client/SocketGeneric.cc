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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * This file contains generic socket based communications routines to be used
 *  by mpirun and libmpi.  The basic model employed is that all send/recv
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

/* static variables local to this file */

static int SendRecvSetup = 0;   /* if == 0 ulm_SocketSendRecvSetup not called yet */
static int MaxReadFD = 0;       /* Largest Read FD */
static int *RemainingReadRecords;       /* Number of records to read before next Next
                                         *  authorization record */
static unsigned int AuthorizationData[3] = { 0, 0, 0 };

#define MAXIOVEC 100

/*
 *  This routine is used to initialize the authorization string used
 *  by the socket communications in between run and its clients.
 */
void ULMInitSocketAuth()
{
    AuthorizationData[0] = getpid();
    AuthorizationData[1] = getuid();
    AuthorizationData[2] = time(NULL);
}


/*
 *  This routine is used to set the authorization string used in
 *  socket communications.
 */
void ulm_SetAuthString(unsigned int *authdata)
{
    int i;

    /* set string */
    for (i = 0; i < 3; i++)
        AuthorizationData[i] = authdata[i];
}


/*
 * This routine is used to obtain the authorization string used in
 * socket communications.
 */
void ulm_GetAuthString(unsigned int *authdata)
{
    int i;

    /* set string */
    for (i = 0; i < 3; i++)
        authdata[i] = AuthorizationData[i];
}


/*
 *  This routine is used to setup all the data needed for
 *  communications using the send/recv functions in this file.  This
 *  routine can be called multiple time with a cummulative effect.
 */
int ulm_SocketSendRecvSetup(int *ReadFDs, int NumReadFDs)
{
    int Max, i, j;
    int *TmpRemainingReadRecords = 0;

    /* find the index of the highest receiving file descriptor - this will
     *  be used for figuring out when to look for authorization data */
    if (SendRecvSetup == 0) {
        /* if first time in, just search the input list */
        Max = 0;
        for (i = 0; i < NumReadFDs; i++) {
            if (ReadFDs[i] > Max)
                Max = ReadFDs[i];
        }
    } else {
        /* else compare current max with new list, and check to make sure no
         *  duplicate entries are listed - if so, return error */
        for (j = 0; j < NumReadFDs; j++) {
            if ((ReadFDs[j] <= MaxReadFD)
                && (RemainingReadRecords[ReadFDs[j]] >= 0))
                return -1;
        }
        Max = MaxReadFD;
        for (i = 0; i < NumReadFDs; i++) {
            if (ReadFDs[i] > Max)
                Max = ReadFDs[i];
        }
    }

    /* Allocate array of ints to hold number of tag/data records to be
     *  receved per ulm_iovec_t sent from the sending side  */
    if (SendRecvSetup == 1) {
        /* if not first time called, create temporary array to hold old data,
         * reallocate, and copy old data in */
        TmpRemainingReadRecords =
            (int *) ulm_malloc(sizeof(int) * (MaxReadFD + 1));
        if (TmpRemainingReadRecords == NULL) {
            return -2;
        }
        for (i = 0; i < MaxReadFD + 1; i++)
            TmpRemainingReadRecords[i] = RemainingReadRecords[i];
        ulm_free(RemainingReadRecords);
    }
    RemainingReadRecords = (int *) ulm_malloc(sizeof(int) * (Max + 1));
    if (RemainingReadRecords == NULL)
        return -3;
    for (i = 0; i <= Max; i++)
        RemainingReadRecords[i] = -1;
    if (SendRecvSetup == 1) {
        for (i = 0; i < MaxReadFD + 1; i++)
            RemainingReadRecords[i] = TmpRemainingReadRecords[i];
        ulm_free(TmpRemainingReadRecords);
        TmpRemainingReadRecords = NULL;
    }

    for (i = 0; i < NumReadFDs; i++)
        RemainingReadRecords[ReadFDs[i]] = 0;

    /* set flag indicating that this routine has already been called */
    SendRecvSetup = 1;
    /* set value of highest read file descriptor */
    MaxReadFD = Max;
    /* check for some simple error conditions */
    if (MAXIOVEC < 3) {
        return -5;
    }

    return 0;                   /* normal termination */
}


/*
 * This routine is used to send data over an already
 *  established socket connection.
 */

ssize_t _ulm_Send_Socket(int DestinationFD, int NumRecordsToSend,
                         ulm_iovec_t * InputSendData)
{
    ssize_t RetVal;

    RetVal = ulm_writev(DestinationFD, InputSendData, NumRecordsToSend);

    return RetVal;
}


/*
 * This routine will be used to read data from a given socket
 */
ssize_t _ulm_Recv_Socket(int SourceFD, void *OutputBuffer,
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
                ulm_err(("return from readv %d errno %d pid %u\n", RetVal, errno,getpid()));
            break;
        }
        else if (RetVal == 0) {
            break;
        }
        TotalRead += (RetVal < 0) ? 0 : RetVal;
    }

    return TotalRead;
}

ssize_t ClientWriteIOToServer(char *String, char *PrependString,
                              int lenPrependString, int *NewLineLast,
                              int Writefd, ssize_t lenString,
                              bool startWithNewLine, lampiState_t *state)
{
    int 		buffLen = state->client->sendBufferSize();
    int 		len, LeftToWrite, LenToWrite, buffSpace;
    caddr_t 	PtrToNewLine, PtrToStartWrite;
    ssize_t 	TotalBytesSent, n;
    int			errorCode, tag;
    bool		done, bufferFull, shouldReset, prepend;
    char		nl;

    LenToWrite = 0;
    TotalBytesSent = 0;
    tag = STDIOMSG;
    if (startWithNewLine) {
        nl = '\n';
        tag = STDIOMSG;
        state->client->reset(adminMessage::SEND);
        state->client->pack(&Writefd, adminMessage::INTEGER, 1);
        state->client->pack(&nl, adminMessage::BYTE, 1);
        TotalBytesSent++;
        if (false == state->client->sendMessage(0, tag, state->channelID, &errorCode)) {
            ulm_err(("Error in sending STDIOMSG.  errorCode = %ld\n",
                     (long) errorCode));
            return -1;
        }
        *NewLineLast = 1;
    }

    if (lenPrependString <= 0)
    {
        n = (ssize_t) strlen(String);
        if (n > lenString)
            n = (ssize_t) lenString;

        LeftToWrite = n;
        while ( LeftToWrite )
        {
            LenToWrite = (LeftToWrite > buffLen) ? buffLen : LeftToWrite;
            state->client->reset(adminMessage::SEND);
            state->client->pack(&Writefd, adminMessage::INTEGER, 1);
            state->client->pack(String, adminMessage::BYTE, LenToWrite);
            if (false == state->client->sendMessage(0, tag, state->channelID, &errorCode)) {
                ulm_err(("Error in sending STDIOMSG.  errorCode = %ld\n",
                         (long) errorCode));
                return -1;
            }
            LeftToWrite -= LenToWrite;
        }

        return (ssize_t) n + TotalBytesSent;

    }

    /* if last character to print out was new line - startout with new-line */
    shouldReset = true;
    if ( *NewLineLast )
    {
        prepend = true;
    }
    *NewLineLast = 0;

    /* write out buffer */
    len = strlen(String);
    if ((ssize_t) len > lenString)
        len = lenString;
    LeftToWrite = len;
    PtrToStartWrite = (caddr_t) String;

    LenToWrite = 0;
    done = false;
    prepend = false;
    while ( !done )
    {

        state->client->reset(adminMessage::SEND);
        state->client->pack(&Writefd, adminMessage::INTEGER, 1);
        buffSpace = buffLen - 1;

        if ( prepend )
        {
            state->client->pack(PrependString, adminMessage::BYTE, lenPrependString);
            TotalBytesSent += lenPrependString;
            buffSpace -= lenPrependString;
            prepend = false;
        }

        bufferFull = false;
        while ( (LeftToWrite > 0) && !bufferFull )
        {
            PtrToNewLine = strchr(String + (len - LeftToWrite), '\n');
            if (PtrToNewLine != NULL)
            {
                // check first if we need to add prepend string.
                if ( LeftToWrite - LenToWrite > 0 )
                    prepend = true;
                LenToWrite = PtrToNewLine - PtrToStartWrite + 1;
                if (PtrToStartWrite == (caddr_t) (String + len))
                    *NewLineLast = 1;
            }
            else
            {
                LenToWrite = (size_t) (String + len - PtrToStartWrite);
            }

            if ( LenToWrite > buffSpace )
            {
                LenToWrite = buffSpace;
                bufferFull = true;
            }

            state->client->pack(PtrToStartWrite, adminMessage::BYTE, LenToWrite);
            TotalBytesSent += LenToWrite;
            LeftToWrite -= LenToWrite;
            buffSpace -= LenToWrite;

            if ( prepend && (lenPrependString < buffSpace) )
            {
                state->client->pack(PrependString, adminMessage::BYTE, lenPrependString);
                TotalBytesSent += lenPrependString;
                buffSpace -= lenPrependString;
                prepend = false;
            }
        }	/* while ( (LeftToWrite > 0) && !bufferFull ) */

        if (false == state->client->sendMessage(0, tag, state->channelID, &errorCode)) {
            ulm_err(("Error in sending STDIOMSG.  errorCode = %ld\n",
                     (long) errorCode));
            return -1;
        }

        if (LeftToWrite <= 0)
            done = true;
    }	/* while (!done) */


    /* return total bytes sent */
    return TotalBytesSent;
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
#define MAXIOVEC 100
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

/*
 *  This routine is used to read the stderr/stdout data forwarded by
 *  Client from the user applications to mpirun.
 */
ssize_t mpirunRecvAndWriteData(int ReadFD, FILE * WriteFile)
{
    ssize_t RetVal, BuffLen = ULM_MAX_TMP_BUFFER - 1;
    char InBuff[ULM_MAX_TMP_BUFFER];
    bool again = false;

    do {
        RetVal = read(ReadFD, (void *)InBuff, BuffLen);
        if (RetVal >= 0) {
            /* successful read or end-of-file */
            InBuff[RetVal] = '\0';
            again = false;
        }
        else if (errno == EINTR) {
            /* trying reading again */
            again = true;
        }
        else {
            /* all other errors should lead to us closing ReadFD */
            return 0;
        }
    } while (again);

    if (RetVal > 0) {
        fprintf(WriteFile, "%s", InBuff);
        fflush(WriteFile);
    }

    return (RetVal < 0) ? 0 : RetVal;
}
