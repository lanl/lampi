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
 *  This routine is used to get the list of "interfaces" 
 *  to be used on each host.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/profiler.h"
#include "internal/constants.h"
#include "internal/types.h"
#include "internal/new.h"
#include "internal/log.h"
#include "run/Input.h"
#include "run/Run.h"
#include "run/RunParams.h"
#include "util/if.h"


void GetInterfaceNoInput(const char *InfoStream)
{
    RunParams.NInterfaces = 0;
    RunParams.InterfaceList = 0;
}


void GetInterfaceList(const char *InfoStream)
{
    /* parse host list */
    int NSeparators = 2;
    char SeparatorList[] = { " , " };
    size_t size;

    int OptionIndex = MatchOption("InterfaceList");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option InterfaceList not found\n"));
        Abort();
    }

    ParseString InterfaceList(Options[OptionIndex].InputData,
                              NSeparators, SeparatorList);

    RunParams.NInterfaces = InterfaceList.GetNSubStrings();
    if (RunParams.NInterfaces == 0) {
        ulm_err(("usage: mpirun [-i <interface-name-list>]\n"));
        Abort();
    }

    size = sizeof(InterfaceName_t) * RunParams.NInterfaces;
    RunParams.InterfaceList = (InterfaceName_t *) ulm_malloc(size);
    if (RunParams.InterfaceList == 0) {
        ulm_err(("GetInterfaceList: ulm_malloc(%d) failed.\n", size));
        Abort();
    }
    memset(RunParams.InterfaceList, 0, size);

    int index = 0;
    for (ParseString::iterator i = InterfaceList.begin();
         i != InterfaceList.end(); i++) {
        HostName_t if_addr;
        if (ulm_ifnametoaddr(*i, if_addr, sizeof(if_addr)) != ULM_SUCCESS) {
            ulm_err(("Invalid interface: %s\n", *i));
            Abort();
        }
        strcpy(RunParams.InterfaceList[index++], *i);
    }
}


void GetInterfaceCount(const char *InfoStream)
{
    int NSeparators = 1;
    char SeparatorList[] = { " " };

    int OptionIndex = MatchOption("InterfaceCount");
    if (OptionIndex < 0) {
        ulm_err(("Error: Option InterfaceCount not found\n"));
        Abort();
    }

    ParseString params(Options[OptionIndex].InputData,
                       NSeparators, SeparatorList);

    int ifCount = 0;
    for (ParseString::iterator i = params.begin(); i != params.end(); i++) {
        ifCount = atol(*i);
        if (ifCount == 0) {
            ulm_err(("Error: invalid value for option -ni \"%s\"\n", *i));
            Abort();
        }
    }

    // check to see how many interfaces are available
    if (ifCount > 1) {
        int ifDiscovered = ulm_ifcount();
        if (ifCount > ifDiscovered)
            ifCount = ifDiscovered;
    }
    if (ifCount <= 1)
        return;

    // fill in list of interface names
    RunParams.NInterfaces = ifCount;
    RunParams.InterfaceList =
        (InterfaceName_t *) ulm_malloc(ifCount * sizeof(InterfaceName_t));
    int index = 0;
    int ifIndex = ulm_ifbegin();
    while (ifIndex > 0 && index < ifCount) {
        ulm_ifindextoname(ifIndex, RunParams.InterfaceList[index++],
                          sizeof(InterfaceName_t));
        ifIndex = ulm_ifnext(ifIndex);
    }
}
