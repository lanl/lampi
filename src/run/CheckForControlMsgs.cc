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

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include "internal/profiler.h"
#include "client/SocketGeneric.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/RunParams.h"
#include "ulm/errors.h"


static abnormal_term_msg_t abnormal_term_msg;


static int GetIOFromClient(int *fd)
{
    int bytes;
    int error;
    int stdio_fd;
    ssize_t size;
    void *buf;

    size = RecvSocket(*fd, &stdio_fd, sizeof(stdio_fd), &error);
    if (size != sizeof(stdio_fd) || error != ULM_SUCCESS) {
        *fd = -1;
        return 0;
    }

    size = RecvSocket(*fd, &bytes, sizeof(bytes), &error);
    if (size != sizeof(bytes) || error != ULM_SUCCESS || bytes == 0) {
        *fd = -1;
        return 0;
    }

    buf = (char*) malloc(bytes);
    size = RecvSocket(*fd, buf, bytes, &error);
    if (size != bytes || error != ULM_SUCCESS) {
        *fd = -1;
        free(buf);
        return 0;
    }

    if (RunParams.Verbose) {
        if (stdio_fd == 1) {
            ulm_err(("*** stdout: received %ld bytes\n", (long) size));
        } else if (stdio_fd == 2) {
            ulm_err(("*** stderr: received %ld bytes\n", (long) size));
        } else {
            ulm_err(("WARNING: received %ld bytes for fd = %d\n",
                     (long) size, stdio_fd));
        }
    }
    
    write(stdio_fd, buf, bytes);
    free(buf);

    return 1;
}


/*
 * Read control messages sent to mpirun from the client daemons
 */
int CheckForControlMsgs(int MaxDesc,
                        int *fd, /* ClientSocketFDList */
                        int NHosts,
                        double *HeartBeat,
                        int *HostsNormalTerminated,
                        int *HostsAbNormalTerminated,
                        int *ActiveHosts,
                        int *ProcessCnt,
                        pid_t **PIDsOfAppProcs,
                        double *TimeFirstCheckin,
                        int *ActiveClients)
{
    int error = 0;
    int rc;
    ssize_t size;
    struct timeval timeout;
    ulm_fd_set_t fdset;
    ulm_iovec_t iov;
    unsigned tag;

    extern int StdInCTS;   // stdin flow control (defined in Daemonize.cc)

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    memset(&fdset, 0, sizeof(fdset));
    for (int i = 0; i < NHosts; i++) {
        if (fd[i] > 0) {
            FD_SET(fd[i], (fd_set *) & fdset);
        }
    }

    rc = select(MaxDesc, (fd_set *) & fdset, NULL, NULL, &timeout);
    if (rc <= 0) {
        return rc;
    }

    for (int host = 0; host < NHosts; host++) {
        if ((fd[host] > 0) && (FD_ISSET(fd[host], (fd_set *) & fdset))) {
            /* Read tag value */
            size = RecvSocket(fd[host], &tag, sizeof(unsigned), &error);
            /* one end of pipe closed */
            if (size == 0 || error != ULM_SUCCESS) {
                ulm_dbg(("RecvSocket: host=%i size=%ld error=%d\n",
                         host, (long) size, error));
                fd[host] = -1;
                if (ActiveHosts[host]) {
                    (*HostsAbNormalTerminated)++;
                } else {
                    (*ActiveClients)--;
                }
                continue;
            }
            if (size < 0) {
                ulm_err(("Error: RecvSocket: host=%i size=%ld error=%d\n",
                         host, (long) size, error));
                Abort();
            }

            switch (tag) {      /* process according to message type */

            case HEARTBEAT:
                if (RunParams.Verbose) {
                    ulm_err(("*** heartbeat from host %d "
                             "(period = %d, timeout = %d)\n",
                             host,
                             RunParams.HeartbeatPeriod,
                             RunParams.HeartbeatTimeout));
                }
                if (RunParams.doHeartbeat) {
#ifndef HAVE_CLOCK_GETTIME
                    struct timeval t;
                    gettimeofday(&t, NULL);
                    HeartBeat[host] =
                        (double) t.tv_sec + ((double) t.tv_usec) * 1e-6;
#else
                    struct timespec t;
                    clock_gettime(CLOCK_REALTIME, &t);
                    HeartBeat[host] =
                        (double) t.tv_sec + ((double) t.tv_nsec) * 1e-9;
#endif
                }
                break;

            case NORMALTERM:
                struct rusage *ru;

                ru = (struct rusage *) ulm_malloc(sizeof(struct rusage)
                                                  *
                                                  RunParams.
                                                  ProcessCount[host]);
                if (NULL == ru) {
                    ulm_err(("Error: Out of memory\n"));
                    Abort();
                }
                size = RecvSocket(fd[host], ru, sizeof(struct rusage)
                                  * RunParams.ProcessCount[host], &error);
                if (size < 0 || error != ULM_SUCCESS) {
                    ulm_err(("Error: RecvSocket: host=%i size=%ld error=%d\n",
                             host, (long) size, error));
                    Abort();
                }

                if (RunParams.Verbose) {
                    ulm_err(("*** host %d reports normal exit\n", host));
                }

                if (RunParams.PrintRusage) {
                    for (int proc = 0; proc < RunParams.ProcessCount[host];
                         proc++) {
                        char name[128];
                        snprintf(name, sizeof(name), "host %d, process %d",
                                 host, proc);
                        UpdateTotalRusage(ru + proc);
                        if (RunParams.Verbose) {
                            PrintRusage(name, ru + proc);
                        }
                    }
                }

                ulm_free(ru);

                if (ActiveHosts[host]) {
                    (*HostsNormalTerminated)++;
                }
                ActiveHosts[host] = 0;

                // if all hosts have terminated normally notify
                // all hosts, so that they can stop network
                // processing and shut down.

                if ((*HostsNormalTerminated) == NHosts) {

                    if (RunParams.Verbose) {
                        ulm_err(("*** all hosts reported normal exit\n"));
                    }

                    tag = ALLHOSTSDONE;
                    iov.iov_base = (char *) &tag;
                    iov.iov_len = (ssize_t) (sizeof(unsigned));
                    for (int j = 0; j < NHosts; j++) {
                        /* send request if socket still open */
                        if (fd[j] > 0) {
                            if (RunParams.Verbose) {
                                ulm_err(("*** all hosts done to host %d\n",
                                         j));
                            }
                            size = SendSocket(fd[j], 1, &iov);
                            /* if size <= 0 assume connection lost */
                            if (size <= 0) {
                                if (ActiveHosts[host]) {
                                    (*HostsAbNormalTerminated)++;
                                }
                                ActiveHosts[host] = 0;
                            }
                        }
                    }
                }               // end sending ALLHOSTSDONE
                break;

            case ACKALLHOSTSDONE:
                if (RunParams.Verbose) {
                    ulm_err(("*** host %d done\n", host));
                }
                (*ActiveClients)--;
                fd[host] = -1;
                break;

            case ABNORMALTERM:
                size = RecvSocket(fd[host], &abnormal_term_msg,
                                  sizeof(abnormal_term_msg), &error);
                if (size > 0 || error != ULM_SUCCESS) {
                    ulm_err(("Error: Process %d exited abnormally\n",
                             abnormal_term_msg.grank));
                    ulm_err(("host=%d proc=%d pid=%d sig=%d stat=%d\n",
                             host,
                             abnormal_term_msg.lrank,
                             abnormal_term_msg.pid,
                             abnormal_term_msg.signal,
                             abnormal_term_msg.status));
                }
                Abort();
                break;

            case STDIOMSG:
                if (RunParams.Verbose) {
                    ulm_err(("*** stdio: receiving output from host %d\n",
                             host));
                }
                ActiveHosts[host] = GetIOFromClient(&fd[host]);
                break;

            case STDIOMSG_CTS:
                if (RunParams.Verbose) {
                    ulm_err(("*** stdin: clear to send from host %d\n", host));
                }
                StdInCTS = true;
                break;

            default:
                ulm_err(("Error: Unknown control message: %d\b", tag));
                Abort();
            }                   /* end switch */
        }
    }                           /* loop over hosts */

    return 0;
}
