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
 * Debugger support for mpirun
 */

/*
 * We interpret the MPICH debugger interface as follows:
 *
 * a) The launcher
 *      - spawns the other processes,
 *      - fills in the table MPIR_proctable, and sets MPIR_proctable_size
 *      - sets MPIR_debug_state to MPIR_DEBUG_SPAWNED ( = 1)
 *      - calls MPIR_Breakpoint() which the debugger will have a
 *	  breakpoint on.
 *
 *  b) Applications start and then spin until MPIR_debug_gate is set
 *     non-zero by the debugger.
 *
 * This file implements a) in two ways depending on whether we want to
 * debug the daemons or the applications.
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

extern "C" void *MPIR_Breakpoint(void);

#define DUMP_INT(X) fprintf(stderr, "  %s = %d\n", # X, X);

static void dump(void)
{
    DUMP_INT(RunParams.dbg.Spawned);
    DUMP_INT(RunParams.dbg.WaitInDaemon);
    DUMP_INT(MPIR_being_debugged);
    DUMP_INT(MPIR_debug_gate);
    DUMP_INT(MPIR_debug_state);
    DUMP_INT(MPIR_acquired_pre_main);
    DUMP_INT(MPIR_i_am_starter);
    DUMP_INT(MPIR_proctable_size);
    fprintf(stderr, "MPIR_proctable:\n");
    for (int i = 0; i < MPIR_proctable_size; i++) {
        fprintf(stderr,
                "    (i, host, exe, pid) = (%d, %s, %s, %ld)\n",
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

    if (RunParams.dbg.Spawned && RunParams.dbg.WaitInDaemon) {
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

    /* 
     * fill in proc table
     */

    if (RunParams.dbg.Spawned && RunParams.dbg.WaitInDaemon) {

        /*
         * Debugging daemons
         */

        ulm_err(("Warning: debugging daemons\n"));

        MPIR_being_debugged = 1;

        for (int h = 0; h < RunParams.NHosts; h++) {
            MPIR_proctable[h].host_name = RunParams.HostList[h];
            MPIR_proctable[h].executable_name = RunParams.ExeList[h];
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
            for (int p = 0; p < (RunParams.ProcessCount)[h]; p++) {
                /* fill in application process information */
                MPIR_proctable[i].host_name = RunParams.HostList[h];
                MPIR_proctable[i].executable_name = RunParams.ExeList[h];
                MPIR_proctable[i].pid = RunParams.AppPIDs[h][p];
                i++;
            }
        }

        if (ENABLE_BPROC && RunParams.dbg.GDB) {
            spawn_gdb_in_xterms();
            return;
        }
    }

    if (RunParams.Verbose) {
        dump();
    }

    if (RunParams.dbg.Spawned) {
        MPIR_debug_state = MPIR_DEBUG_SPAWNED;
    }

    (void) MPIR_Breakpoint();
}
