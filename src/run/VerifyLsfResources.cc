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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "internal/constants.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "util/ParseString.h"
#include "util/Utility.h"
#include "run/Run.h"
#include "internal/new.h"
#include "run/globals.h"
#include "run/Input.h"
#include "run/LSFResource.h"

/*
 * This routine verifys the hosts and their assoicated process counts.
 */

void VerifyLsfResources(const ULMRunParams_t *RunParameters)
{
    int Host;
    int LSFHost;

#ifdef __osf__
/* LSF/RMS integration creates 1 virtual host with all of our processes, so
 * we can't compare the real hosts to the virtual host...we'll just check
 * to see if we have the same number of total processes...
 */
    int lsfprocs = 0, numprocs = 0;
    for (LSFHost = 0; LSFHost < LSFNumHosts; LSFHost++) {
        lsfprocs += LSFProcessCount[LSFHost];
    }
    for (Host = 0; Host < RunParameters->NHosts; Host++) {
        numprocs += RunParameters->ProcessCount[Host];
    }
    if (numprocs > lsfprocs) {
        ulm_err(("Error: the number of desired processes (%d) exceeds the number of LSF processes allowed (%d)\n", numprocs, lsfprocs));
        Abort();
    }
#else
    for (Host = 0; Host < RunParameters->NHosts; Host++) {
        for (LSFHost = 0; LSFHost < LSFNumHosts; LSFHost++) {
            if (strcmp(RunParameters->HostList[Host], LSFHostList[LSFHost])
                == 0) {
                if (LSFProcessCount[LSFHost] >=
                    RunParameters->ProcessCount[Host]) {
                    LSFProcessCount[LSFHost] -=
                        RunParameters->ProcessCount[Host];
                    break;
                } else {
                    printf("Error: your request of %d PE on %s exceeds ",
                           RunParameters->ProcessCount[Host],
                           RunParameters->HostList[Host]);
                    printf("the %d PE allocated by LSF.\n",
                           LSFProcessCount[LSFHost]);
                    Abort();
                }               /* end of process count checking */
            }
            /* end of host name found */
        }                       /* end of for loop for LSF */
        if (LSFHost >= LSFNumHosts) {   /* can't find host in HostNames */
            printf("Error: machine %s not in LSF resource list\n",
                   RunParameters->HostList[Host]);
            Abort();
        }

    }                           /* end of for loop for RunParameters->NHosts */
#endif


    /* release the spaces allocated for LSF resource */
    ulm_delete(LSFHostList);
    ulm_delete(LSFProcessCount);
}
