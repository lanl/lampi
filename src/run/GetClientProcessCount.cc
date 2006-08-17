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
#include <string.h>

#include "internal/profiler.h"
#include "internal/constants.h"
#include "internal/types.h"
#include "internal/log.h"
#include "internal/new.h"
#include "run/Input.h"
#include "run/Run.h"
#include "run/LSFResource.h"
#include "run/RunParams.h"
#include "util/Utility.h"

static bool gotClientProcessCount = false;

#if ENABLE_BPROC
#include <sys/bproc.h>
#include "init/environ.h"

static int nodestatus(int node, char *status, int len)
{
// return 1 if a BPROC node is up, 0 otherwise.
// string describing status is written to char status[len] 
#ifdef BPROC_NODE_NSTATES
    // obsolete!
    bproc_nodestatus(node);
    if (bproc_nodestatus(node) != bproc_node_up) {
        strncpy(status, "unknown", len);
        return 0;
    }
    strncpy(status, "up", len);
    return 1;
#else
    bproc_nodestatus(node, status, len);
    return (0 == strncmp(status, "up", 2));
#endif
}
#endif


/*
 * Get the number of processors from the NODES environment variable
 */
 #if ENABLE_BPROC
static int bproc_get_nprocs(void)
{
    FILE *fp;
    char *s;
    int nprocs;

    s = getenv("NODES");
    if (s == NULL) {
        return -1;
    }

    fp = popen("bpsh $NODES cat /proc/cpuinfo | grep processor | wc -l", "r");
    if (fp == NULL) {
        return -1;
    }
    if (1 != fscanf(fp, "%d", &nprocs)) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    return nprocs;
}
#endif


void getBJSNodes(void)
{
#if ENABLE_BPROC
    int *nodes = 0;
    int nNodes = 0;
    char *node_str = 0, *p;
    char *tmp_str;
#define NODE_STATUS_LEN 10
    char node_status[NODE_STATUS_LEN + 1];      // add 1 for terminating 0
    int i, j, tmp_node;

    lampi_environ_find_string("NODES", &node_str);

    if (strlen(node_str) > 0) {
        tmp_str = (char *) ulm_malloc(strlen(node_str) + 1);
        /* nodes allocated via BJS */
        nodes = ulm_new(int, bproc_numnodes() + 1);
        tmp_str[strlen(node_str)] = '\0';
        strncpy(tmp_str, node_str, strlen(node_str));
        p = tmp_str;
        while (p && *p != '\0') {
            tmp_node = atoi(strsep(&p, ","));
            if (!nodestatus(tmp_node, node_status, NODE_STATUS_LEN)) {
                ulm_err(("Error: An allocated node (%i) is in state \"%s\"\n", tmp_node, node_status));
                Abort();
            }
            if (bproc_access(tmp_node, BPROC_X_OK) != 0) {
                ulm_err(("Error: can not execute on allocated node (%i) stat=%i\n", tmp_node, bproc_access(tmp_node, BPROC_X_OK)));
                Abort();
            }
            nodes[nNodes++] = tmp_node;
        }
        ulm_free(tmp_str);
    }

    if (nNodes) {
        if (RunParams.HostListSize > 0) {
            /* compare each host entry against nodes list */
            for (i = 0; i < RunParams.HostListSize; i++) {
                bool found = false;
                tmp_node = atoi(RunParams.HostList[i]);
                for (j = 0; j < nNodes; j++) {
                    if (tmp_node == nodes[j])
                        found = true;
                }
                if (!found) {
                    ulm_err(("Error: requested node (%s) is not allocated.\n"
                             "Try running without the -H option when using "
                             "a scheduler (LSF or BJS).\n",
                             RunParams.HostList[i]));
                    // we do not have to abort here - then app will run on
                    // subset of requested nodes.  But this seems not what
                    // the user would expect
                    Abort();
                    strcpy(RunParams.HostList[i], "");
                }

            }
            nNodes = 0;
            j = 0;
            for (i = 0; i < RunParams.HostListSize; i++) {
                if (RunParams.HostList[i][0] != '\0') {
                    if (i == j) {
                        j++;
                    } else {
                        strcpy(RunParams.HostList[j++],
                               RunParams.HostList[i]);
                    }
                }
            }
            if (j == 0) {
                ulm_delete(RunParams.HostList);
                RunParams.HostList = 0;
            }
            RunParams.HostListSize = j;
            if (j == 0) {
                ulm_err(("Error: No valid allocated nodes specified. "
                         "Try running without the -H option\n", j));
                Abort();
            }
        } else {
            /* store each host entry of nodes list */
            RunParams.HostList = ulm_new(HostName_t, nNodes);
            RunParams.HostListSize = nNodes;
            for (i = 0; i < nNodes; i++) {
                sprintf(RunParams.HostList[i], "%d", nodes[i]);
            }
        }
    }

    if (nodes) {
        ulm_delete(nodes);
    }
#endif
    return;
}

void pickNodesFromList(int cnt)
{
#if ENABLE_BPROC
    int i, j = 0, tmp_node;
    char node_status[NODE_STATUS_LEN + 1];      // add 1 for terminating 0

    if (RunParams.HostListSize > 0) {
        /* find cnt nodes among the already listed nodes */
        for (i = 0; i < RunParams.HostListSize; i++) {
            tmp_node = atoi(RunParams.HostList[i]);
            if ((nodestatus(tmp_node, node_status, NODE_STATUS_LEN)) &&
                (bproc_access(tmp_node, BPROC_X_OK) == 0)) {
                if (i == j) {
                    j++;
                } else {
                    strcpy(RunParams.HostList[j++], RunParams.HostList[i]);
                }
            }
            if (j == cnt)
                break;
        }
    } else {
        /* find cnt nodes among all nodes, master last */
        RunParams.HostList = ulm_new(HostName_t, cnt);
        RunParams.HostListSize = cnt;
        for (i = 0; i < bproc_numnodes(); i++) {
            if ((nodestatus(i, node_status, NODE_STATUS_LEN)) &&
                (bproc_access(i, BPROC_X_OK) == 0)) {
                sprintf(RunParams.HostList[j++], "%d", i);
            }
            if (j == cnt)
                break;
        }
        /* use the master node as a last resort */
        if ((j == (cnt - 1))
            &&
            (nodestatus(BPROC_NODE_MASTER, node_status, NODE_STATUS_LEN))
            && (bproc_access(BPROC_NODE_MASTER, BPROC_X_OK) == 0))
            sprintf(RunParams.HostList[j++], "%d", BPROC_NODE_MASTER);
    }
    if (RunParams.HostListSize && (j == 0)) {
        ulm_delete(RunParams.HostList);
        RunParams.HostList = 0;
    }
    RunParams.HostListSize = j;
#endif
    return;
}

void pickCurrentNode(void)
{
#if ENABLE_BPROC
    /* use the current node only */
    RunParams.HostList = ulm_new(HostName_t, 1);
    RunParams.HostListSize = 1;
    int tmp_node = bproc_currnode();
    sprintf(RunParams.HostList[0], "%d", tmp_node);
#endif
    return;
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
    } else {
        gotClientProcessCount = true;
    }

    /* find Procs index in option list */
    int OptionIndex = MatchOption("Procs");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option Procs not found in Input parameter database\n"));
        Abort();
    }

    /* parse Procs data */
    ParseString ProcData(Options[OptionIndex].InputData,
                         NSeparators, SeparatorList);

    totalProcsSpecified = (ProcData.GetNSubStrings() == 1) ? true : false;

    if (RunParams.UseLSF) {
        /* build HostList from LSFHost */
        if (RunParams.HostListSize == 0) {
            nhosts = RunParams.NHostsSet ? RunParams.NHosts : LSFNumHosts;
            if (nhosts < 1) {
                ulm_err(("Error: Invalid argument to -N (%d)\n", nhosts));
                Abort();
            }

            RunParams.HostList = ulm_new(HostName_t, nhosts);
            RunParams.HostListSize = nhosts;
            /* fill in host information */
            for (int Host = 0; Host < nhosts; Host++) {
                if (Host < LSFNumHosts) {
                    strcpy(RunParams.HostList[Host], LSFHostList[Host]);
                }
            }
        }
        if (!RunParams.NHostsSet) {
            /* this should work for both LSF (with real machines) and 
             * LSF/RMS combinations (where one virtual host is reserved 
             * with all processors); although we will have to fix everything 
             * up for LSF/RMS after we spawn...
             */
            RunParams.NHosts = RunParams.HostListSize;
        }
    } else if (RunParams.UseBproc) {
        /* check BJS and reconcile nodes with -H specified nodes */
        getBJSNodes();
        if (RunParams.HostListSize > 0) {
            nhosts = (RunParams.NHostsSet) ? ((RunParams.NHosts >
                                               RunParams.
                                               HostListSize) ? RunParams.
                                              HostListSize : RunParams.
                                              NHosts) : RunParams.HostListSize;
            pickNodesFromList(nhosts);
        } else {
            nhosts = (RunParams.NHostsSet) ? RunParams.NHosts : 1;
            pickNodesFromList(nhosts);
            if ((nhosts == 1) && (RunParams.HostListSize == 0))
                pickCurrentNode();
        }
        if (RunParams.NHostsSet
            && (RunParams.HostListSize < RunParams.NHosts)) {
            ulm_err(("Error: %d nodes found, %d requested\n",
                     RunParams.HostListSize, RunParams.NHosts));
            Abort();
        } else if (RunParams.HostListSize == 0) {
            ulm_err(("Error: no available nodes found\n"));
            Abort();
        }
        RunParams.NHosts = RunParams.HostListSize;
    } else {
        if (RunParams.HostListSize == 0) {
            if (ENABLE_RMS) {
                if (!RunParams.NHostsSet) {
                    RunParams.NHosts = 1;
                }
            } else {
                GetAppHostDataNoInputRSH(InfoStream);
                if (!RunParams.NHostsSet) {
                    RunParams.NHosts = RunParams.HostListSize;
                } else if (RunParams.NHosts != 1) {
                    ulm_err(("Error: -N %d specified, but no HostList (-H)\n",
                             RunParams.NHosts));
                    Abort();
                }
            }
        } else if (!RunParams.NHostsSet) {
            RunParams.NHosts = RunParams.HostListSize;
        }
    }

    /* allocate memory for the array */
    RunParams.ProcessCount = ulm_new(int, RunParams.NHosts);

    for (int i = 0; i < RunParams.NHosts; i++) {
        RunParams.ProcessCount[i] = 0;
    }

    if (totalProcsSpecified) {
        int totalProcs = atoi(*(ProcData.begin()));
        if (totalProcs < 1) {
            ulm_err(("Error: -np/-n option must specify at least one process.\n"));
            Abort();
        }

        if (ENABLE_RMS) {
            /* distribute totalProcs across the hosts */
            for (int host = 0; host < RunParams.NHosts; host++) {
                RunParams.ProcessCount[host] = 0;
            }
            for (int proc = 0, host = 0; proc < totalProcs; proc++) {
                RunParams.ProcessCount[host] += 1;
                host = (host + 1) % RunParams.NHosts;
            }
        } else {
            if (RunParams.NHosts == 1) {
                RunParams.ProcessCount[0] = totalProcs;
            } else if (RunParams.UseLSF) {
                /* minimize the number of hosts by using all available processors */
                int procsAllocated = 0;
                for (int i = 0; i < RunParams.NHosts; i++) {
                    for (int j = 0; j < LSFNumHosts; j++) {
                        if (strcmp(RunParams.HostList[i], LSFHostList[j])
                            == 0) {
                            int procs =
                                ((totalProcs - procsAllocated) >
                                 LSFProcessCount[j]) ? LSFProcessCount[j]
                                : (totalProcs - procsAllocated);
                            RunParams.ProcessCount[i] = procs;
                            procsAllocated += procs;
                            break;
                        }
                    }
                    if (RunParams.ProcessCount[i] == 0) {
                        RunParams.NHosts = i;
                        break;
                    }
                }
                if (totalProcs != procsAllocated) {
                    ulm_err(("Error: unable to allocate %d processes "
                             "(allocated %d processes) across %d hosts.\n",
                             totalProcs, procsAllocated,
                             RunParams.NHosts));
                    Abort();
                }
            } else {
                /*
                 * assign as close as possible to the same number of
                 * processes for each host
                 */
                int baseProcs = totalProcs / RunParams.NHosts;
                int extraProcs = totalProcs % RunParams.NHosts;
                for (int i = 0; i < RunParams.NHosts; i++) {
                    RunParams.ProcessCount[i] = baseProcs;
                    if (extraProcs > 0) {
                        RunParams.ProcessCount[i]++;
                        extraProcs--;
                    }
                    if (RunParams.ProcessCount[i] == 0) {
                        RunParams.NHosts = i;
                        break;
                    }
                }
            }
        }
    } else {
        /* fill in data */
        FillIntData(&ProcData, RunParams.NHosts,
                    RunParams.ProcessCount, Options, OptionIndex, 1);

        if (ENABLE_RMS) {
            /*
             * RMS determines the distribution of processes, so
             * reshuffle ProcessCount
             */

            int totalProcs = 0;

            for (int host = 0; host < RunParams.NHosts; host++) {
                totalProcs += RunParams.ProcessCount[host];
                RunParams.ProcessCount[host] = 0;
            }
            for (int proc = 0, host = 0; proc < totalProcs; proc++) {
                RunParams.ProcessCount[host] += 1;
                host = (host + 1) % RunParams.NHosts;
            }
        }
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

#if ENABLE_BPROC
    /*
     * On BProc systems if there is a NODES environment variable then
     * by default start 2 processes on each specified node.
     */

    int nprocs = bproc_get_nprocs();
    if (nprocs > 0) {
        int OptionIndex = MatchOption("Procs");
        sprintf(Options[OptionIndex].InputData,
                "%d", nprocs);
        return GetClientProcessCount(InfoStream);
    }
    ulm_err(("Error: No argument to -np/-n and failed to guess from BProc environment\n"));
    Abort();
#endif

    if (!RunParams.UseLSF) {
        ulm_err(("Error: No argument to -np/-n.\n"));
        Abort();
    }

    /* build HostList from LSFHost */
    if (RunParams.HostListSize == 0) {
        nhosts = RunParams.NHostsSet ? RunParams.NHosts : LSFNumHosts;
        if (nhosts < 1) {
            ulm_err(("Error: Invalid argument to -N (%d)\n",
                     nhosts));
            Abort();
        }

        RunParams.HostList = ulm_new(HostName_t, nhosts);
        RunParams.HostListSize = nhosts;
        /* fill in host information */
        for (int Host = 0; Host < nhosts; Host++) {
            if (Host < LSFNumHosts) {
                strcpy(RunParams.HostList[Host], LSFHostList[Host]);
            }
        }
    }

    if (!RunParams.NHostsSet) {
        /* this should work for both LSF (with real machines) and 
         * LSF/RMS combinations (where one virtual host is reserved 
         * with all processors); although we will have to fix everything 
         * up for LSF/RMS after we spawn...
         */
        RunParams.NHosts = RunParams.HostListSize;
    }

    /* allocate memory for the array */
    RunParams.ProcessCount = ulm_new(int, RunParams.NHosts);

    /* use LSFProcessCount as defaults. */
    if (RunParams.UseLSF) {

        for (Host = 0; Host < RunParams.NHosts; Host++) {
            for (LSFHost = 0; LSFHost < LSFNumHosts; LSFHost++) {
                if (strcmp(RunParams.HostList[Host], LSFHostList[LSFHost])
                    == 0) {
                    RunParams.ProcessCount[Host] =
                        LSFProcessCount[LSFHost];
                    break;
                }
            }                   /* end of for loop for LSF */
            if (LSFHost >= LSFNumHosts) {
                ulm_err(("Error: There is no PE allocated on machine %s\n",
                         RunParams.HostList[Host]));
                Abort();
            }
        }                       /* end of for loop for RunParams.NHosts */
    }
}
