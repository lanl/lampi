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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "internal/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "internal/constants.h"
#include "internal/new.h"
#include "internal/types.h"
#include "run/Run.h"
#include "run/Input.h"
#include "internal/new.h"
#include "run/globals.h"

/*
 * This routine is used to rerange the host list and all the data that
 *   assumes host order.
 */
void RearrangeHostList(const char *InfoStream)
{
    int i, j, k, TmpStore, Indx, TmpNDevs, *ListDevTmp;
    size_t len;
    struct hostent *NetEntry;
    DirName_t TmpDir, TmpDirTC;
    ExeName_t TmpExe;
    int NSeparators = 2;
    char SeparatorList[] = { " , " };
    HostName_t *HostList = RunParameters.HostList;
    int *ProcessCnt = RunParameters.ProcessCount;
    DirName_t *WorkingDirList = RunParameters.WorkingDirList;
    ExeName_t *ExeList = RunParameters.ExeList;
    int NHosts = RunParameters.NHosts;
    DirName_t *ClientDirList = RunParameters.UserAppDirList;
    int *NPathTypes = RunParameters.NPathTypes;
    int **ListPathTypes = RunParameters.ListPathTypes;

    /* get input data */
    int OptionIndex =
        MatchOption("MasterHost", ULMInputOptions, SizeOfInputOptionsDB);
    if (OptionIndex < 0) {
        ulm_err(("Error: Option TotalViewDebugStartup not found\n"));
        Abort();
    }
    /* parse MasterHost data */
    ParseString MHData(ULMInputOptions[OptionIndex].InputData, NSeparators,
                       SeparatorList);

    i = MHData.GetNSubStrings();
    if (i != 1) {
        ulm_err(("Error: Incorrect number of host listed for MasterHost.\n"
                 "\tNumber listed %d, but should be 1.\n", i));
        Abort();
    }


    /* get host name */
    NetEntry = gethostbyname(*(MHData.begin()));
    if (NetEntry == NULL) {
        ulm_err(("Error: Unrecognized host name %s\n",
                *(MHData.begin())));
        perror(" gethostbyname ");
        Abort();
    }
    len = strlen(NetEntry->h_name);
    if (len > ULM_MAX_HOSTNAME_LEN) {
        printf
            (" Error: Host name too long for library buffer, length = %ld\n",
             (long) len);
        Abort();
    }
    /* find server host in host list */
    Indx = -1;
    for (j = 0; j < NHosts; j++) {
        if (strncmp(HostList[j], NetEntry->h_name, len) == 0) {
            Indx = j;
            break;
        }
    }                           /* end Server Host processing */
    if (Indx == -1) {
        printf
            ("Error: Server Host %s is not in the list of host for this job.\n",
             NetEntry->h_name);
        Abort();
    }

    /* rearange order of data */
    TmpStore = ProcessCnt[Indx];
    TmpNDevs = NPathTypes[Indx];
    sprintf(TmpDir, "%s", WorkingDirList[Indx]);
    if (ClientDirList != NULL)
        sprintf(TmpDirTC, "%s", ClientDirList[Indx]);
    sprintf(TmpExe, "%s", ExeList[Indx]);
    ListDevTmp = ulm_new(int, TmpNDevs);
    for (j = 0; j < TmpNDevs; j++)
        ListDevTmp[j] = ListPathTypes[Indx][j];

    /* change order of hosts */
    for (k = Indx; k > 0; k--) {
        sprintf(HostList[k], "%s", HostList[k - 1]);
        ProcessCnt[k] = ProcessCnt[k - 1];      /* processor count */
        sprintf(WorkingDirList[k], "%s", WorkingDirList[k - 1]);        /* working directory */
        if (ClientDirList != NULL)
            sprintf(ClientDirList[k], "%s", ClientDirList[k - 1]);      /* Client */
        sprintf(ExeList[k], "%s", ExeList[k - 1]);      /* Executable */
        NPathTypes[k] = NPathTypes[k - 1];    /* number net devs */
        /* list of network devs */
        ulm_delete(ListPathTypes[k]);
        ListPathTypes[k] = ulm_new(int,
             NPathTypes[k]);
        for (j = 0; j < NPathTypes[k]; j++)
            ListPathTypes[k][j] =
                ListPathTypes[k - 1][j];
    }                           /* end loop over k */

    /* set the data for host 0 - the server host */
    sprintf(HostList[0], "%s", NetEntry->h_name);
    ProcessCnt[0] = TmpStore;
    sprintf(WorkingDirList[0], "%s", TmpDir);
    if (ClientDirList != NULL)
        sprintf(ClientDirList[0], "%s", TmpDirTC);
    sprintf(ExeList[0], "%s", TmpExe);
    NPathTypes[0] = TmpNDevs;
    ulm_delete(ListPathTypes[0]);
    ListPathTypes[0] = ulm_new(int, TmpNDevs);
    for (j = 0; j < TmpNDevs; j++)
        ListPathTypes[0][j] = ListDevTmp[j];
    ulm_delete(ListDevTmp);
}
