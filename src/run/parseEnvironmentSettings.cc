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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "internal/constants.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/Input.h"
#include "run/globals.h"
#include "util/ParseTree.h"

void parseEnvironmentSettings(const char *InfoStream)
{
    // get number of hosts
    int nHosts = RunParameters.NHosts;

    // get pointer to input data
    int OptionIndex =
        MatchOption("EnvVars", ULMInputOptions, SizeOfInputOptionsDB);
    if (OptionIndex < 0) {
        fprintf(stderr, "Option EnvironmentVariables not found in "
                "Input parameter database\n");
        Abort();
    }
    // the input format is as follows
    //  -env "var0='hosta%VAL0:VAL1:...;hostb%VAL10:VAL11:...',
    //        var1='hostc%VAL0:VAL1:...;hostd%VAL10:VAL11:...', ... "
    //   where the environment variables are set only for the hosts
    //    listed.
    // and
    //  -env "var0='VAL0:VAL1:...',
    //        var1='VAL0:VAL1:...', ... "
    //   where the environment variables are set the same on each host
    //    listed.
    //  both forms can be mixed in a single command line.

    // Parse Environement input
    ParseTree P(", =", "''\"\"", ULMInputOptions[OptionIndex].InputData);
    if (!P.isCorrect()) {
        fprintf(stderr, P.getError());
        Abort();
    }
    // Extract leaves of parse tree
    int leaf_cnt = P.countLeaves(P.getRoot());
    if (leaf_cnt == 1) {
        fprintf(stderr, "Expression %s did not parse.\n",
                P.getExpression());
        Abort();
    }
    Node **leaves = ulm_new(Node *, leaf_cnt);
    P.getLeaves(P.getRoot(), leaves);

    // Count number of environment variables
    int env_cnt = 0;
    int leaf_i = 0;
    while (leaf_i < leaf_cnt) {
        leaf_i += P.countLeaves(leaves[leaf_i]->getParent());
        env_cnt++;
    }
    RunParameters.nEnvVarsToSet = env_cnt;
    if (RunParameters.nEnvVarsToSet == 0)
        return;

    // Allocate space to store environment variable names and
    // settings
    RunParameters.envVarsToSet =
        ulm_new(ulmENVSettings, RunParameters.nEnvVarsToSet);

    // Extract indices of environment variables in leaf array
    // and set environment variable names in RunParameters
    int *env_indices = ulm_new(int, env_cnt);
    char *expr, *buf;
    int i;
    int env_i = 0;
    leaf_i = 0;
    while (leaf_i < leaf_cnt) {
        env_indices[env_i] = leaf_i;
        expr = leaves[leaf_i]->getExpression();
        buf = (char *) ulm_malloc(strlen(expr) + 1);
        if (!buf) {
            fprintf(stderr, "Error: allocating memory for var_m\n"
                    "  Memory requested %ld\n", (long) (strlen(expr) + 1));
            Abort();
        }
        strcpy(buf, expr);
        buf[strlen(expr)] = '\0';
        RunParameters.envVarsToSet[env_i].var_m = buf;
        env_i++;
        leaf_i += P.countLeaves(leaves[leaf_i]->getParent());
    }


    // perform error checking for duplicate values
    int j, ubound, host_cnt, val_cnt, host;
    Node *n, *s;
    for (i = 0; i < env_cnt; i++) {
        host_cnt = 0, val_cnt = 0;
        ubound = i < env_cnt - 1 ? env_indices[i + 1] : leaf_cnt;
        for (j = env_indices[i] + 1; j < ubound; j++) {
            n = leaves[j];
            s = n->getSibling();
            // hosts can only be left children, and they must have '='
            // as their parent
            if (s != NULL && *(n->getParent()->getExpression()) == '=') {
                host_cnt++;
            } else
                val_cnt++;
        }
        // Exit if there are errors
        if (host_cnt > val_cnt) {       // duplicate values

            fprintf(stderr, "Duplicate values specified in Environment\n"
                    "Variable settings.  Input :: %s\n",
                    P.getExpression());
            Abort();
        }
        if (host_cnt > nHosts) {        // too many hosts specified

            fprintf(stderr, "Duplicate hosts specified in environment\n"
                    "variable settings.  Input :: %s\n",
                    P.getExpression());
            Abort();
        }
        // if there are no host names, set input data for all hosts
        if (host_cnt == 0) {
            // if there are no host names, set input data for
            // all hosts
            RunParameters.envVarsToSet[i].setForAllHosts_m = true;
            RunParameters.envVarsToSet[i].envString_m =
                (char **) ulm_malloc(sizeof(char *));
            if (!RunParameters.envVarsToSet[i].envString_m) {
                fprintf(stderr,
                        "Error: allocating memory for envString_m\n"
                        "  Memory requested %ld\n",
                        (long) (sizeof(char *)));
                Abort();
            }
        } else {
            // set input data for each host - default setting is
            // no data
            RunParameters.envVarsToSet[i].setForAllHosts_m = false;
            RunParameters.envVarsToSet[i].setForThisHost_m =
                (bool *) ulm_malloc(sizeof(bool) * nHosts);
            if (!RunParameters.envVarsToSet[i].setForThisHost_m) {
                fprintf(stderr,
                        "Error: allocating memory for setForThisHost_m\n"
                        "  Memory Requested %ld\n",
                        (long) (sizeof(bool) * nHosts));
                Abort();
            }
            for (host = 0; host < nHosts; host++) {
                RunParameters.envVarsToSet[i].setForThisHost_m[host] =
                    false;
            }
            RunParameters.envVarsToSet[i].envString_m =
                (char **) ulm_malloc(sizeof(char *) * nHosts);
            if (!RunParameters.envVarsToSet[i].envString_m) {
                fprintf(stderr,
                        "Error: allocating memory for envString_m\n"
                        "  Memory requested %ld\n",
                        (long) (sizeof(char *) * nHosts));
                Abort();
            }
        }
    }

    // for each environment variable, process host names
    char *name, *val;
    for (i = 0; i < env_cnt; i++) {
        ubound = i < env_cnt - 1 ? env_indices[i + 1] : leaf_cnt;
        for (j = env_indices[i] + 1; j < ubound; j++) {
            n = leaves[j];
            s = n->getSibling();

            // host must be a left child and must have '=' as
            // a parent
            if (s == NULL || *(n->getParent()->getExpression()) != '=')
                continue;

            // make sure host name exists and is in the RunParameters
            // HostList
            name = n->getExpression();
            struct hostent *NetEntry = gethostbyname(name);
            if (NetEntry == NULL) {
                fprintf(stderr, "Unrecognized host name %s\n", name);
                Abort();
            }
            int host_i = -1;
            for (int host = 0; host < nHosts; host++) {
                int cmp = strcmp(NetEntry->h_name,
                                 RunParameters.HostList[host]);
                if (cmp == 0)
                    host_i = host;
            }
            if (host_i == -1) {
                fprintf(stderr, "Specified host not in host list.\n"
                        "Host name %s\n", NetEntry->h_name);
                Abort();
            }
            // fill in data for the host
            val = s->getExpression();
            RunParameters.envVarsToSet[i].envString_m[host_i] =
                (char *) ulm_malloc(strlen(val) + 1);
            if (!RunParameters.envVarsToSet[i].envString_m[host_i]) {
                fprintf(stderr,
                        "Error: allocating memory for envString_m\n"
                        "  Memory requested %ld\n",
                        (long) (strlen(val) + 1));
                Abort();
            }
            strcpy(RunParameters.envVarsToSet[i].envString_m[host_i], val);
            RunParameters.envVarsToSet[i].
                envString_m[host_i][strlen(val)] = '\0';
            RunParameters.envVarsToSet[i].setForThisHost_m[host_i] = true;
        }
    }

    // for each environment variable, process standalone values
    for (i = 0; i < env_cnt; i++) {
        ubound = i < env_cnt - 1 ? env_indices[i + 1] : leaf_cnt;
        for (j = env_indices[i] + 1; j < ubound; j++) {
            n = leaves[j];
            val = n->getExpression();

            // values must either have ', ' as a parent, or the must be
            // next to an environment variable leaf as a right child
            if (*(n->getParent()->getExpression()) != ',' &&
                (j > env_indices[i] + 1 || n->getSibling() != NULL))
                continue;

            if (RunParameters.envVarsToSet[i].setForAllHosts_m) {
                RunParameters.envVarsToSet[i].envString_m[0] =
                    (char *) ulm_malloc(strlen(val) + 1);
                if (!RunParameters.envVarsToSet[i].envString_m[0]) {
                    fprintf(stderr,
                            "Error: allocating memory for envString_m\n"
                            "  Memory requested %ld\n",
                            (long) (strlen(val) + 1));
                    Abort();
                }
                strcpy(RunParameters.envVarsToSet[i].envString_m[0], val);
                RunParameters.envVarsToSet[i].envString_m[0][strlen(val)] =
                    '\0';
            } else {
                for (host = 0; host < nHosts; host++) {
                    if (!RunParameters.envVarsToSet[i].
                        setForThisHost_m[host]) {
                        RunParameters.envVarsToSet[i].
                            setForThisHost_m[host] = true;
                        RunParameters.envVarsToSet[i].envString_m[host] =
                            (char *) ulm_malloc(strlen(val) + 1);
                        if (!RunParameters.envVarsToSet[i].
                            envString_m[host]) {
                            fprintf(stderr,
                                    "Error: allocating memory for envString_m\n"
                                    "  Memory requested %ld\n",
                                    (long) (strlen(val) + 1));
                            Abort();
                        }
                        strcpy(RunParameters.envVarsToSet[i].
                               envString_m[host], val);
                        RunParameters.envVarsToSet[i].
                            envString_m[host][strlen(val)] = '\0';
                    }
                }
            }
        }
    }

    // clean up locally allocated memory
    ulm_delete(env_indices);
    ulm_delete(leaves);
}
