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



#include "internal/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal/constants.h"
#include "internal/types.h"
#include "internal/new.h"
#include "internal/log.h"
#include "run/Run.h"
#include "run/Input.h"
#include "util/Utility.h"
#include "run/globals.h"
#include "internal/new.h"

/*
 *  This routine is used to get the working directory on each
 *    client host when no input is specified.
 */
void GetClientWorkingDirectoryNoInp(const char *InfoStream)
{
    DirName_t DirName;

    /* find ProcsPerHost index in option list */
    int OptionIndex =
        MatchOption("WorkingDir", ULMInputOptions, SizeOfInputOptionsDB);
    if (OptionIndex < 0) {
        ulm_err(("Error: Option WorkingDir not found in Input parameter database\n"));
        Abort();
    }

    /* allocate memory for the array */
    RunParameters.WorkingDirList =
        ulm_new(DirName_t, RunParameters.NHosts);

    if (getcwd(DirName, ULM_MAX_PATH_LEN) == NULL) {
        ulm_err(("Error: getcwd() call failed\n"));
        perror(" getcwd ");
        Abort();
    }
    for (int i = 0; i < RunParameters.NHosts; i++) {
        sprintf(RunParameters.WorkingDirList[i], "%s", (char *) DirName);
    }
}

/*
 *  This routine is used to get the working directory on each
 *    client using data from the input sources.
 */
void GetClientWorkingDirectory(const char *InfoStream)
{
    GetDirectoryList("WorkingDir", &(RunParameters.WorkingDirList),
                     RunParameters.NHosts);

    return;
}
