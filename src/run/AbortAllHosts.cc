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
#include <unistd.h>
#include <sys/uio.h>

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/globals.h"
#include "client/SocketGeneric.h"

/*
 * This routine is used to send an abort message to all
 *  hosts participating in the job.
 */

int mpirunAbortAllHosts(int *ClientSocketFDList, int NHosts, adminMessage *server)
{
    int NFailed = 0;
#ifndef ENABLE_RMS
    int Tag;
#ifdef ENABLE_CT
    int errorCode;
#else
    int i;
    ulm_iovec_t IOVec;
    ssize_t IOReturn;
#endif

    /* send abort message to each host */
    NFailed = 0;
    Tag = TERMINATENOW;
#ifdef ENABLE_CT
    server->reset(adminMessage::SEND);
    if ( false == server->broadcastMessage(Tag, &errorCode) )
    {
        ulm_err( ("Error: bcasting TERMINATENOW.\n") );
        Abort();
    }

#else

    IOVec.iov_base = (char *) &Tag;
    IOVec.iov_len = (ssize_t) (sizeof(unsigned int));
    if (ClientSocketFDList) {
        for (i = 0; i < NHosts; i++) {
            /* send only to hosts that are still assumed to be alive */
            if (ClientSocketFDList[i] > 0) {
                IOReturn = _ulm_Send_Socket(ClientSocketFDList[i], 1, &IOVec);
                /*  With failed send, register host as down !!!! */
                if (IOReturn <= 0) {
                    close(ClientSocketFDList[i]);
                    ClientSocketFDList[i] = -1;
                    NFailed++;
                    HostsAbNormalTerminated++;
                }
            }
        }
    }
#endif

#endif
    return NFailed;
}
