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
#include <stdlib.h>
#include <string.h>

#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "internal/log.h"
#include "internal/new.h"
#include "run/Run.h"
#include "run/Input.h"
#include "run/globals.h"
#include "internal/new.h"
#include "util/Utility.h"
#include "run/LSFResource.h"

static bool gotClientProcessCount = false;

#ifdef BPROC
#include <sys/bproc.h>
#include "init/environ.h"
#endif

int getBprocNodes(void) 
{
    int nNodes = 0;
#ifdef BPROC
    char *bproc_states[BPROC_NODE_NSTATES] = BPROC_NODE_STATE_STRINGS;
    char *node_str = 0, *p;
    char tmp_str[512];
    int i, tmp_node;

    lampi_environ_find_string("NODES", &node_str);
    tmp_str[511] = '\0';
    strncpy(tmp_str, node_str, 511);
    p = tmp_str;
    while (p && *p != '\0') {
        tmp_node = bproc_getnodebyname(strsep(&p, ","));
        if ((bproc_nodestatus(tmp_node) != bproc_node_up)) {
		    ulm_err(("Error: A BJS reserved node (%i) is not up, but in state \"%s\"\n",
			    tmp_node, bproc_states[bproc_nodestatus(tmp_node)]));
            Abort();
        }
        nNodes++;
    }

    if (nNodes) {
        if (RunParameters.NHostsSet) {
            /* limit the number of hosts to a smaller specified value (option -N) */
            nNodes = (nNodes > RunParameters.NHosts) ? RunParameters.NHosts : nNodes;
        }
        /* use the BJS reserved nodes specified */
        RunParameters.HostList = ulm_new(HostName_t, nNodes);
        RunParameters.HostListSize = nNodes;
        /* fill in host information */
        for (i = 0; i < nNodes; i++) {
            strcpy(RunParameters.HostList[i], strsep(&node_str, ","));
        }
    } 
    else {
        /* use the current node only */
        RunParameters.HostList = ulm_new(HostName_t, 1);
        RunParameters.HostListSize = 1;
        tmp_node = bproc_currnode();
        sprintf(RunParameters.HostList[0], "%d", tmp_node);
    }
#endif
    return nNodes;
}

/*
 * Get the process count for each client host.
 */
void GetClientProcessCount(const char *InfoStream)
{
    int NSeparators = 2;
    char SeparatorList[] = { " , " };
    int nhosts;
    bool totalProcsSpecified;

    /* make sure we only do GetClientProcessCount... once between -np and -n! */
    if (gotClientProcessCount) {
        return;
    }
    else {
        gotClientProcessCount = true;
    }

    /* find Procs or ProcsAlias index in option list */
    int OptionIndex = MatchOption("Procs", ULMInputOptions, SizeOfInputOptionsDB);
    if (OptionIndex < 0) {
        ulm_err(("mpirun: Error: Option Procs not found in Input parameter database\n"));
        Abort();
    }
    if (strlen(ULMInputOptions[OptionIndex].InputData) == 0) {
        OptionIndex = MatchOption("ProcsAlias", ULMInputOptions, SizeOfInputOptionsDB);
        if (OptionIndex < 0) {
            ulm_err(("mpirun: Error: Option ProcsAlias not found in Input parameter database\n"));
            Abort();
        }
    }
    
    /* parse Procs/ProcsAlias data */
    ParseString ProcData(ULMInputOptions[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    totalProcsSpecified = (ProcData.GetNSubStrings() == 1) ? true : false;

    if (RunParameters.UseLSF) {
        /* build HostList from LSFHost */
        if (RunParameters.HostListSize == 0) {
            nhosts = (RunParameters.NHostsSet) ? RunParameters.NHosts : LSFNumHosts;
            if (nhosts > LSFNumHosts) {
#ifndef __osf__
                ulm_err(("mpirun: Error: -N option specifies more hosts (%d) than are available via LSF (%d).\n",
                    nhosts, LSFNumHosts));
                Abort();
#endif
            }
            else if (nhosts < 1) {
                ulm_err(("mpirun: Error: -N option specifies less than 1 host (%d).\n",
                    nhosts));
                Abort();
            }

            RunParameters.HostList = ulm_new(HostName_t,  nhosts);
            RunParameters.HostListSize = nhosts;
            /* fill in host information */
            for (int Host = 0; Host < nhosts; Host++) {
                if (Host < LSFNumHosts) {
                    strcpy(RunParameters.HostList[Host], LSFHostList[Host]);
                }
            }
        }
        if (!RunParameters.NHostsSet) {
            /* this should work for both LSF (with real machines) and 
             * LSF/RMS combinations (where one virtual host is reserved 
             * with all processors); although we will have to fix everything 
             * up for LSF/RMS after we spawn...
             */
            RunParameters.NHosts = RunParameters.HostListSize;
        }
    }
    else if (RunParameters.UseBproc) {
        if (RunParameters.HostListSize == 0) {
            // is BJS NODES environment variable set?
            nhosts = getBprocNodes();
            if (RunParameters.NHostsSet && (nhosts < RunParameters.NHosts)) {
                ulm_err(("Error: -N option specifies more hosts (%d) than are reserved via BJS (%d).\n",
                    RunParameters.NHosts, nhosts));
                Abort();
            }
        }
        if (!RunParameters.NHostsSet) {
            RunParameters.NHosts = RunParameters.HostListSize;
        }
    }
    else {
        if (RunParameters.HostListSize == 0) {
#ifdef __osf__
            if (!RunParameters.NHostsSet) {
                RunParameters.NHosts = 1;
            }
#else
            GetAppHostDataNoInputRSH(InfoStream);
            if (!RunParameters.NHostsSet) {
                RunParameters.NHosts = RunParameters.HostListSize;
            }
            else if (RunParameters.NHosts != 1) {
                ulm_err(("mpirun: Error: -N option did not specify 1 host (machine) "
                    "but there is no host (-H) information.\n"));
                Abort();
            }
#endif
        }
        else if (!RunParameters.NHostsSet) {
            RunParameters.NHosts = RunParameters.HostListSize;
        }
    }
    
    /* allocate memory for the array */
    RunParameters.ProcessCount = ulm_new(int, RunParameters.NHosts);

    for (int i = 0; i < RunParameters.NHosts; i++) {
        RunParameters.ProcessCount[i] = 0;
    }

    if (totalProcsSpecified) {
        int totalProcs = atoi(*(ProcData.begin()));
        if (totalProcs < 1) {
            ulm_err(("mpirun: Error: -np/-n option must specify at least one process.\n"));
            Abort();
        }
#ifdef __osf__
        RunParameters.ProcessCount[0] = totalProcs;
#else
        if (RunParameters.NHosts == 1) {
            RunParameters.ProcessCount[0] = totalProcs;
        }
        else if (RunParameters.UseLSF) {
            /* minimize the number of hosts by using all available processors */
            int procsAllocated = 0;
            for (int i = 0; i < RunParameters.NHosts; i++) {
                for (int j = 0; j < LSFNumHosts; j++) {
                    if (strcmp(RunParameters.HostList[i], LSFHostList[j]) == 0) {
                        int procs = ((totalProcs - procsAllocated) > LSFProcessCount[j]) ? 
                            LSFProcessCount[j] : (totalProcs - procsAllocated);
                        RunParameters.ProcessCount[i] = procs;
                        procsAllocated += procs;
                        break;
                    }
                }
                if (RunParameters.ProcessCount[i] == 0) {
                    RunParameters.NHosts = i;
                    break;
                }
            }
            if (totalProcs != procsAllocated) {
                ulm_err(("mpirun: Error: unable to allocate %d processes "
                    "(allocated %d processes) across %d hosts.\n",
                    totalProcs, procsAllocated, RunParameters.NHosts));
                Abort();
            }
        }
        else {
            /* assign as close as possible to the same number of processes for each
             * host
             */
            int baseProcs = totalProcs / RunParameters.NHosts;
            int extraProcs = totalProcs % RunParameters.NHosts;
            for (int i = 0; i < RunParameters.NHosts; i++) {
                RunParameters.ProcessCount[i] = baseProcs;
                if (extraProcs > 0) {
                    RunParameters.ProcessCount[i]++;
                    extraProcs--;
                }
                if (RunParameters.ProcessCount[i] == 0) {
                    RunParameters.NHosts = i;
                    break;
                }
            }
        }
#endif
    }
    else {
        /* fill in data */
        FillIntData(&ProcData, RunParameters.NHosts,
                RunParameters.ProcessCount, ULMInputOptions, OptionIndex,
                1);
    }
}


void GetClientProcessCountNoInput(const char *InfoStream)
{
    int Host;
    int LSFHost;
    int nhosts;

    /* make sure we only do GetClientProcessCount... once between -np and -n! */
    if (gotClientProcessCount) {
        return;
    }

    if (!RunParameters.UseLSF) {
        ulm_err(("mpirun: Error: No input data provided for -np/-n.\n"));
        Abort();
    }

    /* build HostList from LSFHost */
    if (RunParameters.HostListSize == 0) {
        nhosts = (RunParameters.NHostsSet) ? RunParameters.NHosts : LSFNumHosts;
        if (nhosts > LSFNumHosts) {
#ifndef __osf__
            ulm_err(("mpirun: Error: -N option specifies more hosts (%d) than are available via LSF (%d).\n",
                nhosts, LSFNumHosts));
            Abort();
#endif
        }
        else if (nhosts < 1) {
            ulm_err(("mpirun: Error: -N option specifies less than 1 host (%d).\n",
                nhosts));
            Abort();
        }

        RunParameters.HostList = ulm_new(HostName_t,  nhosts);
        RunParameters.HostListSize = nhosts;
        /* fill in host information */
        for (int Host = 0; Host < nhosts; Host++) {
            if (Host < LSFNumHosts) {
                strcpy(RunParameters.HostList[Host], LSFHostList[Host]);
            }
        }
    }

    if (!RunParameters.NHostsSet) {
        /* this should work for both LSF (with real machines) and 
         * LSF/RMS combinations (where one virtual host is reserved 
         * with all processors); although we will have to fix everything 
         * up for LSF/RMS after we spawn...
         */
        RunParameters.NHosts = RunParameters.HostListSize;
    }

    /* allocate memory for the array */
    RunParameters.ProcessCount = ulm_new(int, RunParameters.NHosts);

    /* use LSFProcessCount as defaults. */

    for (Host = 0; Host < RunParameters.NHosts; Host++) {
        for (LSFHost = 0; LSFHost < LSFNumHosts; LSFHost++) {
            if (strcmp(RunParameters.HostList[Host], LSFHostList[LSFHost])
                == 0) {
                RunParameters.ProcessCount[Host] =
                    LSFProcessCount[LSFHost];
                break;
            }
        }                       /* end of for loop for LSF */
        if (LSFHost >= LSFNumHosts) {
            ulm_err(("mpirun: Error: There is no PE allocated on machine %s\n", RunParameters.HostList[Host]));
            Abort();
        }
    }                           /* end of for loop for RunParameters->NHosts */
}
