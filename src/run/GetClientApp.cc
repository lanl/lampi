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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>             // needed for getcwd

#include "internal/profiler.h"
#include "internal/constants.h"
#include "internal/types.h"
#include "internal/log.h"
#include "internal/new.h"
#include "run/Run.h"
#include "util/ParseString.h"

/*
 * This routine is used to parse the command line option -exe, and set
 * up the executable information.
 */
void GetClientApp(const char *InfoStream)
{
    int NSeparators = 2, cnt, j, OptionIndex1, OptionIndex2;
    char SeparatorList[] = { " , " };
    ParseString::iterator i;
    DirName_t DirName;

    /* find BinaryName index in option list */
    int OptionIndex = MatchOption("BinaryName");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option BinaryName not found in input parameter database\n"));
        Abort();
    }

    /* parse executable list */
    ParseString ExeData(Options[OptionIndex].InputData,
                        NSeparators, SeparatorList);

    /* Check to see if expected data is present */
    cnt = ExeData.GetNSubStrings();
    if ((cnt != 1) && (cnt != RunParams.NHosts)) {
        ulm_err(("Error: Wrong number of data elements specified for %s or %s\n", Options[OptionIndex].AbreviatedName, Options[OptionIndex].FileName));
        ulm_err(("Number or arguments specified is %d -- should be 1 or %d\n", cnt, RunParams.NHosts));
        ulm_err(("Input line: %s\n", Options[OptionIndex].InputData));
        Abort();
    }

    /* allocate memory for the array */
    RunParams.ExeList = ulm_new(DirName_t, RunParams.NHosts);

    /* fill in data */

    if (cnt == 1) {
        /* 1 data elements */
        i = ExeData.begin();
        if ((*i)[0] != '/') {
            /* root directory not specified */
            OptionIndex1 = MatchOption("DirectoryOfBinary");
            OptionIndex2 = MatchOption("WorkingDir");
            if ((Options[OptionIndex1].InputData)[0] == '/') {
                /* DirectoryOfBinary was specified */
                for (int j = 0; j < RunParams.NHosts; j++) {
                    sprintf(RunParams.ExeList[j], "%s/%s",
                            (char *) RunParams.UserAppDirList[j], (*i));
                }
            } else if ((Options[OptionIndex2].InputData)[0] == '/') {
                /* WorkingDir specified */
                for (int j = 0; j < RunParams.NHosts; j++) {
                    sprintf(RunParams.ExeList[j], "%s/%s",
                            (char *) RunParams.WorkingDirList[j], (*i));
                }
            } else {
                /* else use $PWD */
                if (getcwd(DirName, ULM_MAX_PATH_LEN) == NULL) {
                    ulm_err(("getcwd() call failed\n"));
                    perror("getcwd");
                    Abort();
                }
                for (int j = 0; j < RunParams.NHosts; j++) {
                    sprintf(RunParams.ExeList[j], "%s/%s",
                            (char *) DirName, (*i));
                }
            }
        } else {
            /* root directory specified */
            for (int j = 0; j < RunParams.NHosts; j++) {
                sprintf(RunParams.ExeList[j], "%s", (*i));
            }
        }
    } else {
        /* cnt data elements */
        for (j = 0, i = ExeData.begin();
             i != ExeData.end(), j < RunParams.NHosts; j++, i++) {
            if ((*i)[0] != '/') {
                /* root directory not specified */
                OptionIndex1 = MatchOption("DirectoryOfBinary");
                OptionIndex2 = MatchOption("WorkingDir");
                if ((Options[OptionIndex1].InputData)[0] == '/') {
                    /* DirectoryOfBinary was specified */
                    sprintf(RunParams.ExeList[j], "%s/%s",
                            (char *) RunParams.UserAppDirList[j], (*i));
                } else if ((Options[OptionIndex2].InputData)[0] == '/') {
                    /* WorkingDir specified */
                    sprintf(RunParams.ExeList[j], "%s/%s",
                            (char *) RunParams.WorkingDirList[j], (*i));
                } else {
                    /* root directory not specified */
                    if (getcwd(DirName, ULM_MAX_PATH_LEN) == NULL) {
                        ulm_err(("getcwd() call failed\n"));
                        perror("getcwd");
                        Abort();
                    }
                    sprintf(RunParams.ExeList[j], "%s/%s",
                            (char *) DirName, (*i));
                }
            } else {
                /* root directory specified */
                sprintf(RunParams.ExeList[j], "%s", (*i));
            }
        }                       /* end loop over entries */
    }                           /* end fill in data */

    return;
}
