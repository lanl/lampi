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
 *  This routine is used to setup the data TotalView needs
 *    to use, so that it can attach to the remote processes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>

#include "internal/profiler.h"
#include "client/adminMessage.h"
#include "internal/constants.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/RunParams.h"
#include "run/TV.h"

MPIR_PROCDESC *MPIR_proctable = NULL;
int MPIR_proctable_size = 0;
int MPIR_being_debugged = 0;
volatile int MPIR_debug_state = 0;
volatile int TVDummy = 0;       /* NEVER change this - this is a variable the
                                   MPIR_Breakpoint uses to trick the compiler
                                   thinking that this might be modified, and
                                   therefore not optimize MPIR_Breakpoint away */
volatile int MPIR_i_am_starter = 1;
volatile int MPIR_debug_gate = 0;

void MPIrunTVSetUp(void)
{
    adminMessage *server = RunParams.server;
    int HostRank, NumberOfProcsToDebug;

    /* check to see if code being debugged */
    if (RunParams.TVDebug == 0)
        return;

    /* compute size of MPIR_proctable */
    NumberOfProcsToDebug = RunParams.NHosts;
    if (RunParams.TVDebugApp != 0) {
        /* no daemon processes */
        NumberOfProcsToDebug = 0;
        /* app processes */
        for (HostRank = 0; HostRank < RunParams.NHosts; HostRank++)
            NumberOfProcsToDebug +=
                ((RunParams.ProcessCount)[HostRank]);
    }

    /* allocate memory for process table */
    MPIR_proctable_size = NumberOfProcsToDebug;
    MPIR_proctable =
        (MPIR_PROCDESC *) ulm_malloc(sizeof(MPIR_PROCDESC) *
                                     MPIR_proctable_size);
    if (MPIR_proctable == NULL) {
        printf("Unable to allocate MPIR_proctable.\n");
        Abort();
    }
    /* allocate memory for list of hosts used by TotalView */
    RunParams.TVHostList =
        (HostName_t *) ulm_malloc(sizeof(HostName_t) *
                                  RunParams.NHosts);
    if (RunParams.TVHostList == NULL) {
        printf("Unable to allocate TVHostList array.\n");
        Abort();
    }
    for (HostRank = 0; HostRank < RunParams.NHosts; HostRank++) {
        bzero((RunParams.TVHostList)[HostRank], ULM_MAX_HOSTNAME_LEN);
        strcpy((RunParams.TVHostList)[HostRank], (char *)(RunParams.HostList[HostRank]));
    }

/* debug */
    printf
        (" IN MPIrunTVSetUp ::  MPIR_proctable_size %d RunParams.TVDebugApp %d\n",
         MPIR_proctable_size, RunParams.TVDebugApp);
    fflush(stdout);
/* end debug */
    /* if app being debugged, setup will be finished after the fork() */
    if (RunParams.TVDebugApp != 0)
        return;
    /* debug */
    fprintf(stderr," debugging deamons\n");
    fflush(stderr);
    /* end debug */

    /* set flag indicating TotalView is going to be used */
    MPIR_being_debugged = 1;

    /* fill in MPIR_proctable */
    for (HostRank = 0; HostRank < RunParams.NHosts; HostRank++) {
        MPIR_proctable[HostRank].host_name =
            (char *) &((RunParams.TVHostList)[HostRank]);
        MPIR_proctable[HostRank].executable_name =
            (char *) (RunParams.ExeList[HostRank]);
        MPIR_proctable[HostRank].pid = server->daemonPIDForHostRank(HostRank);
    }

    /* set debug state */
    MPIR_debug_state = MPIR_DEBUG_SPAWNED;

    /* provide function in which TotalView will set a breakpoint */
    MPIR_Breakpoint();
}

/* dummy subroutine - need to make sure that compiler will
 *   not optimize this routine away, so that TotalView can set
 *   a breakpoint, and do what it needs to.
 */
extern "C" void MPIR_Breakpoint(void)
{
    if (TVDummy != 0) {
        ulm_err(("Error: Debugger setup: TVDummy != 0\n"));
        Abort();
    }
    while (TVDummy == 1) ;
}
