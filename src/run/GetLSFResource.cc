/*
 * Copyright 2002-2003. The Regents of the University of
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

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "init/environ.h"
#include "internal/constants.h"
#include "internal/log.h"
#include "internal/new.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Input.h"
#include "run/Run.h"
#include "run/globals.h"
#include "util/ParseString.h"
#include "util/Utility.h"

#define _LSF_RESOURCE_INIT
#include "run/LSFResource.h"

/*
 * This routine sets up the LSF host and process information from
 * LSB_MCPU_HOST enviironment variable.
 */

void GetLSFResource()
{
    struct hostent *NetEntry;
    int NSeparators = 1, Host, Count;
    char SeparatorList[] = { " " };
    size_t len;
    char *HnameNumProcs;


    lampi_environ_find_string("LSB_MCPU_HOSTS", &HnameNumProcs);

    if (HnameNumProcs == NULL)
        return;

    /* parse host list */
    ParseString LSFData(HnameNumProcs, NSeparators, SeparatorList);
    LSFNumHosts = (LSFData.GetNSubStrings() + 1) / 2;

    LSFHostList = ulm_new(HostName_t, LSFNumHosts);
    LSFProcessCount = ulm_new(int, LSFNumHosts);


    Count = 0;
    /* fill in host data */
    for (ParseString::iterator i = LSFData.begin(); i != LSFData.end();
         i++) {
        Host = Count / 2;
        if (Count % 2) {        /* fill in the number of processes for this host */
            LSFProcessCount[Host] = atoi(*i);
        } else {                /* fill in the host name */
            NetEntry = gethostbyname(*i);
            if (NetEntry == NULL) {
                ulm_err(("Error: Unrecognized host name %s\n",
                         *i));
                Abort();
            }
            len = strlen(NetEntry->h_name);
            if (len > ULM_MAX_HOSTNAME_LEN) {
                ulm_err(("Error: Host name too long for library buffer, length = %ld\n", len));
                Abort();
            }
            strcpy((char *) LSFHostList[Host], NetEntry->h_name);
        }
        Count++;
    }
}
