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

#define MPIR_DEBUG_SPAWNED  1
#define MPIR_DEBUG_ABORTING 2

struct MPIR_PROCDESC {
    char *host_name;        /* something that can be passed to inet_addr */
    char *executable_name;  /* name of binary */
    int pid;                /* process pid */
};

MPIR_PROCDESC *MPIR_proctable = NULL;
int MPIR_proctable_size = 0;
int MPIR_being_debugged = 0;
int MPIR_force_to_main = 0;
volatile int MPIR_debug_state = 0;
volatile int MPIR_i_am_starter = 0;
volatile int MPIR_debug_gate = 0;
volatile int MPIR_acquired_pre_main = 0;
volatile int debugger_dummy = 0;


static void dump(void)
{
    fprintf(stderr, "MPIR_being_debugged = %d\n", MPIR_being_debugged);
    fprintf(stderr, "MPIR_debug_gate = %d\n", MPIR_debug_gate);
    fprintf(stderr, "MPIR_debug_state = %d\n", MPIR_debug_state);
    fprintf(stderr, "MPIR_acquired_pre_main = %d\n", MPIR_acquired_pre_main);
    fprintf(stderr, "MPIR_i_am_starter = %d\n", MPIR_i_am_starter);
    fprintf(stderr, "RunParams.TVDebug = %d\n", RunParams.TVDebug);
    fprintf(stderr, "RunParams.TVDebugApp = %d\n", RunParams.TVDebugApp);

    fprintf(stderr,
            "MPIR_proctable_size = %d\n"
            "MPIR_proctable:\n", MPIR_proctable_size);
    for (int i = 0; i < MPIR_proctable_size; i++) {
        fprintf(stderr,
                "  (i, host, exe, pid) = (%d, %s, %s, %ld)\n",
                i,
                MPIR_proctable[i].host_name,
                MPIR_proctable[i].executable_name,
                (long) MPIR_proctable[i].pid);
    }
}


static void spawn_gdb_in_xterms(void)
{
    /*
     * spawn xterm/gdb session to attach to remote process
     *
     * !!! intended for development -- bproc only !!!
     */

    for (int i = 0; i < MPIR_proctable_size; i++) {
        char title[64];
        char cmd[256];
        bool use_gdbserver = (getenv("LAMPI_USE_GDBSERVER") != 0) ? true : false;

        sprintf(title, "rank: %d (pid: %ld host: %s)", i,
                (long) MPIR_proctable[i].pid, MPIR_proctable[i].host_name);

        if (use_gdbserver) {
            sprintf(cmd, "xterm -title \"%s\" -e gdb %s &", title,
                    MPIR_proctable[i].executable_name);
            ulm_warn(("spawning: \"%s\"\n", cmd));
            system(cmd);
            sprintf(cmd, "bpsh %s gdbserver localhost:%d --attach %ld &",
                    (MPIR_proctable[i].host_name + 1), (9000 + i),
                    (long) MPIR_proctable[i].pid);
            ulm_warn(("spawning: \"%s\"\n", cmd));
            system(cmd);
        } else {
            sprintf(cmd, "xterm -title \"%s\" -e bpsh %s gdb %s %ld &",
                    title, (MPIR_proctable[i].host_name + 1),
                    MPIR_proctable[i].executable_name,
                    (long) MPIR_proctable[i].pid);
            ulm_warn(("spawning: \"%s\"\n", cmd));
            system(cmd);
        }
    }
}


void DebuggerInit()
{
    adminMessage *server = RunParams.server;

    if (RunParams.TVDebug && RunParams.TVDebugApp == 0) {
        /* init for debugging daemon processes */
        MPIR_proctable_size = RunParams.NHosts;
    } else {
        /* init for debugging app processes */
        MPIR_proctable_size = RunParams.TotalProcessCount;
    }

    /* allocate memory for process table */
    MPIR_proctable = (MPIR_PROCDESC *) ulm_malloc(sizeof(MPIR_PROCDESC) *
                                                  MPIR_proctable_size);
    if (MPIR_proctable == NULL) {
        ulm_err(("Out of memory\n"));
        Abort();
    }

    /* allocate memory for list of hosts used by TotalView */
    RunParams.TVHostList = (HostName_t *) ulm_malloc(sizeof(HostName_t) *
                                                     RunParams.NHosts);
    if (RunParams.TVHostList == NULL) {
        ulm_err(("Out of memory\n"));
        Abort();
    }
    for (int h = 0; h < RunParams.NHosts; h++) {
        bzero((RunParams.TVHostList)[h], ULM_MAX_HOSTNAME_LEN);
        strcpy((RunParams.TVHostList)[h], (char *)(RunParams.HostList[h]));
    }

    /* 
     * fill in proc table
     */

    if (RunParams.TVDebug == 1 && RunParams.TVDebugApp == 0) {

        /*
         * Debugging daemons
         */

        ulm_dbg(("debugging daemons\n"));

        MPIR_being_debugged = 1;

        for (int h = 0; h < RunParams.NHosts; h++) {
            MPIR_proctable[h].host_name = (char *) &RunParams.TVHostList[h];
            MPIR_proctable[h].executable_name = (char *) RunParams.ExeList[h];
            MPIR_proctable[h].pid = server->daemonPIDForHostRank(h);
        }

    } else {
        
        /*
         * Debugging applications or not being debugged.
         *
         * Either way, fill in the proc table for the application
         * processes in case someone attaches later.
         */

        int i = 0;

        for (int h = 0; h < RunParams.NHosts; h++) {
            /* loop over procs */
            for (int p = 0; p < (RunParams.ProcessCount)[h]; p++) {
                /* fill in application process information */
                MPIR_proctable[i].host_name = (char *) &RunParams.TVHostList[h];
                MPIR_proctable[i].executable_name = (char *) RunParams.ExeList[h];
                MPIR_proctable[i].pid = RunParams.AppPIDs[h][p];
                i++;
            }
        }

        if(ENABLE_BPROC && RunParams.GDBDebug) {
            spawn_gdb_in_xterms();
            return;
        }
    }

    if (RunParams.Verbose) {
        dump();
    }

    if (RunParams.TVDebug) {
        MPIR_debug_state = MPIR_DEBUG_SPAWNED;
    }
}

/**********************************************************************/

/*
 * breakpoint function for parallel debuggers
 */
extern "C" void *MPIR_Breakpoint(void)
{
    return NULL;
}
