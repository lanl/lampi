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
#include <stdlib.h>
#include <unistd.h>
#include <sys/uio.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>             // needed for bzero()
#include <sys/time.h>

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "internal/types.h"
#include "internal/log.h"
#include "client/daemon.h"
#include "client/SocketGeneric.h"
#include "queue/globals.h"

/*
 * Receive and process control messages from mpirun
 */
int ClientCheckForControlMsgs(lampiState_t *s)
 {
     ulm_fd_set_t fdset;
     int rc, NotifyServer, NFDs, maxfd, error;
     ssize_t size;
     struct timeval WaitTime;
     ulm_iovec_t iovec;
     unsigned int message;
     unsigned int tag;
     bool again = true;

     WaitTime.tv_sec = 0;
     WaitTime.tv_usec = 100000;

     if (s->client->socketToServer_m == -1) {
         return 0;
     }

     ULM_FD_ZERO(&fdset);
     if (s->client->socketToServer_m >= 0) {
         ULM_FD_SET(s->client->socketToServer_m, &fdset);
     }
     maxfd = s->client->socketToServer_m;

     rc = select(maxfd + 1, (fd_set *) &fdset, NULL, NULL, &WaitTime);
     if (rc <= 0) {
         return rc;
     }

     if ((s->client->socketToServer_m >= 0)
         && (ULM_FD_ISSET(s->client->socketToServer_m, &fdset))) {

         /* read tag value */
         tag = 0;
         size = RecvSocket(s->client->socketToServer_m, &tag,
                           sizeof(unsigned int), &error);

         /* socket connection closed */
         if (size == 0) {
             s->client->socketToServer_m = -1;
             return 0;
         }
         if (size < 0 || error != ULM_SUCCESS) {
             ulm_exit(("Error: reading tag.  rc = %ld, "
                       "error = %d\n", size, error));
         }

         /* process according to message type */
         switch (tag) {

         case HEARTBEAT:
             if (s->verbose) {
                 ulm_err(("*** heartbeat from mpirun (period = %d)\n",
                          s->HeartbeatPeriod));
             }
             break;

         case TERMINATENOW:
             /* recieve request to teminate immediately */
             message = ACKTERMINATENOW;
             NotifyServer = 1;
             ClientScanStdoutStderr(s);
             ClientAbort(s, message, NotifyServer);
             break;

         case ALLHOSTSDONE:
             /* set flag indicating app process can terminate */
             ulm_dbg(("client ALLHOSTSDONE arrived on host %d\n",
                      s->hostid));
             tag = ACKALLHOSTSDONE;
             iovec.iov_base = (char *) &tag;
             iovec.iov_len = (ssize_t) sizeof(unsigned int);
             size = SendSocket(s->client->socketToServer_m, 1, &iovec);
             if (size < 0) {
                 ulm_exit(("Error: sending ACKALLHOSTSDONE.  "
                           "rc: %ld\n", size));
             }

             /* drain stdio */
             maxfd = 0;
             NFDs = s->local_size + 1;
             for (int i = 0; i < NFDs; i++) {
                 if (s->STDOUTfdsFromChildren[i] > maxfd) {
                     maxfd = s->STDOUTfdsFromChildren[i];
                 }
                 if (s->STDERRfdsFromChildren[i] > maxfd) {
                     maxfd = s->STDERRfdsFromChildren[i];
                 }
             }
             maxfd++;

             /* check to see if control socket is still open - if not exit */
             again = true;
             while (again) {

                 ClientScanStdoutStderr(s);

                 /*
                  * scan again only if pipes to children are still open; note
                  * that we avoid the last set of descriptors since they are
                  * the client daemon's....
                  */
                 again = false;
                 for (int i = 0; i < s->local_size; i++) {
                     if (s->STDOUTfdsFromChildren[i] >= 0 ||
                         s->STDERRfdsFromChildren[i] >= 0) {
                         again = true;
                     }
                }
            }

            s->STDOUTfdsFromChildren[s->local_size] = -1;
            s->STDERRfdsFromChildren[s->local_size] = -1;

            *s->sync.AllHostsDone = 1;

            // ok, the client daemon can now exit...
            exit(EXIT_SUCCESS);
            break;

        case STDIOMSG:
            if (s->STDINfdToChild >= 0) {
                ClientRecvStdin(&(s->client->socketToServer_m),
                                &(s->STDINfdToChild));
            }
            break;

        default:
            ulm_exit(("Error: Unrecognized control message (%u)\n", tag));
        }
    }

    return 0;
}
