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

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/uio.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"

#include "client/ULMClient.h"
#include "client/SocketGeneric.h"

/*
 * This routine is used for orderly application shutdown, and is called
 * after all the children have terminated in an orderly manner.
 * This routine needs to make sure that the "stdio" from the user
 * application is deliverd properly to mpirun, for output.
 */
void ClientOrderlyShutdown(size_t *StderrBytesWritten,
                           size_t *StdoutBytesWritten,
                           int ControlSocketToULMRunFD,
                           lampiState_t *state)
{
    size_t STDIOSent[2];
    unsigned int Tag;
#if ENABLE_CT
    int			errorCode;
#else
    ssize_t IOReturn;
    ulm_iovec_t IOVec[2];
#endif
    static int Finished = 0;

    /* check to see if mpirun already notified.  If so, return */
    if (Finished) {
        return;
    }

    /* notify mpirun that apps on host are done
     * ClientCheckForControlMsgs will wait for the ack from mpirun
     * that it is ok to shutdown
     */
    STDIOSent[0] = *StderrBytesWritten;
    STDIOSent[1] = *StdoutBytesWritten;
    Tag = NORMALTERM;
#if ENABLE_CT
    // create admin msg to send to mpirun
    ulm_fdbg(("Node %d: sending NORMALTERM to mpirun.\n", state->client->nodeLabel()));
    state->client->reset(adminMessage::SEND);
    if ( false == state->client->sendMessage(0, Tag, state->channelID, &errorCode) )
    {
        ulm_exit((-1, "Error: sending NORMALTERM.  RetVal: %ld\n",
                  errorCode));
    }
#else
    IOVec[0].iov_base = (char *) &Tag;
    IOVec[0].iov_len = (ssize_t) (sizeof(unsigned int));
    IOVec[1].iov_base = (char *) STDIOSent;
    IOVec[1].iov_len = (ssize_t) (2 * sizeof(ssize_t));
    IOReturn = _ulm_Send_Socket(ControlSocketToULMRunFD, 2, IOVec);

    /* abort here on error, since not aborting will cause Client to
     *  spin wainting on an ack that will never arive, since the Normalterm
     *  control message never went out.
     */
    if (IOReturn < 0) {
        ulm_exit((-1, "Error: sending NORMALTERM.  RetVal: %ld\n",
                  IOReturn));
    }
#endif

    /* set flag indicating mpirun has been notified of normal termination */
    Finished = 1;
}
