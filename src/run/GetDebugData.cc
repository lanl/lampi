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

#include "internal/constants.h"
#include "internal/types.h"
#include "internal/log.h"
#include "run/Run.h"
#include "run/Input.h"
#include "run/globals.h"

/*
 * Set the Totalview debug flags such that debugging will start with
 * the daemon processes.
 */
void GetTVDaemon(const char *InfoStream)
{
    /* find TotalViewDebugStartup index in option list */
    int OptionIndex = MatchOption("TotalViewDebugStartup", ULMInputOptions,
                                  SizeOfInputOptionsDB);
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TotalViewDebugStartup not found in Input parameter database\n"));
        Abort();
    }
    RunParameters.TVDebug = 1;
    RunParameters.TVDebugApp = 0;
    /* shut off heartbeat - so won't time out while debugging */
    RunParameters.HeartBeatTimeOut = -1;
}

/*
 * Set the Totalview debug flags such that debugging will start after
 * the app processes have forked.
 */
void GetTVAll(const char *InfoStream)
{
    /* 
     * we are using the MPIR_being_debugged from totalview to invoke
     * this routine.  We don't need to match on the input parameter
     * index.
     */

    RunParameters.TVDebug = 1;
    RunParameters.TVDebugApp = 1;
    /* shut off heartbeat - so won't time out while debugging */
    RunParameters.HeartBeatTimeOut = -1;
}

void GetGDB(const char *InfoStream)
{
    /* find GDBDebugStartup index in option list */
    int OptionIndex = MatchOption("GDBDebugStartup", ULMInputOptions,
                                  SizeOfInputOptionsDB);
    if (OptionIndex < 0) {
        ulm_err(("Error: Option GDBDebugStartup not found in Input parameter database\n"));
        Abort();
    }
    RunParameters.TVDebug = 1;
    RunParameters.TVDebugApp = 1;
    RunParameters.GDBDebug = 1;
    /* shut off heartbeat - so won't time out while debugging */
    RunParameters.HeartBeatTimeOut = -1;
}
