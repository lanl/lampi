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
#include <unistd.h>
#include <sys/uio.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "client/SocketGeneric.h"


static char stdin_buff[ULM_MAX_IO_BUFFER];
static int stdin_offset = 0;
static int stdin_size = 0;


/*
 * Receive an I/O message from mpirun. Buffer the data - and attempt
 * to write to the application. If the pipe/pty is full, we queue the
 * data and poll the descriptor until all data is written.
 */
int ClientRecvStdin(int *src, int *dst)
{
    int error;
    ssize_t size = RecvSocket(*src, &stdin_size, sizeof(stdin_size), &error);

    /* socket connection closed */
    if (size == 0) {
        close(*src);
        *src = -1;
        return -1;
    }

    if (size < 0 || error != ULM_SUCCESS) {
        ulm_exit(("Error: reading STDIOMSG.  RetVal = %ld, error = %d\n",
                  size, error));
    }

    /* close stdin to child */
    if (stdin_size == 0) {
        close(*dst);
        *dst = -1;
        return 0;
    }

    size = RecvSocket(*src, stdin_buff, stdin_size, &error);
    if (size == 0) {
        close(*src);
        close(*dst);
        *src = *dst = -1;
        return -1;
    }

    if (size < 0 || error != ULM_SUCCESS) {
        ulm_exit(("ClientSendStdin: error reading STDIOMSG, error = %d\n",
                  error));
    }
    return ClientSendStdin(src, dst);
}


/*
 * Attempt to write the buffer to the app - when the entire buffer is
 * written send a flow control message to the source.
 */
int ClientSendStdin(int *server, int *client)
{
    /* is there anything to do */
    if (stdin_size == 0)
        return 0;

    int size = write(*client, stdin_buff + stdin_offset,
                     stdin_size - stdin_offset);
    if (size < 0) {
        close(*client);
        *client = -1;
        return (-1);
    }

    stdin_offset += size;
    if (stdin_offset == stdin_size) {

        stdin_offset = 0;
        stdin_size = 0;

        /* send ack back to mpirun */
        unsigned int tag = STDIOMSG_CTS;
        ulm_iovec_t ack;
        ack.iov_base = &tag;
        ack.iov_len = sizeof(tag);
        if (ulm_writev(*server, &ack, 1) != sizeof(tag)) {
            ulm_err(("ClientScanStdin: write to server failed, errno=%d\n",
                     errno));
            close(*server);
            *server = -1;
            return (-1);
        }
    }
    return (0);
}
