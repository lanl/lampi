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

/*
 * This routine is used to reset RunParameters data structures when the
 *   number of hosts actually used in the job does not match the number
 *   of hosts requested.  Also, the host list is generated in this
 *   routine.
 */
#include "run/JobParams.h"
#include "internal/new.h"
#include "run/globals.h"
#include "run/Run.h"

void FixRunParameters(ULMRunParams_t *RunParameters, int nhosts)
{
	int				h,*tmpNProc,host,nProcs;
	pid_t 			*tmpPIDs;
	adminMessage	*server;
	admin_host_info_t *	groupHostData;
    bool			useDottedIP;
    
	server = RunParameters->server;
    // free and reallocate memory for HostList if needed
    if (!RunParameters->HostList || (nhosts != RunParameters->HostListSize)) {
        if (RunParameters->HostList) {
            ulm_delete(RunParameters->HostList);
        }
        RunParameters->HostList = ulm_new(HostName_t,  nhosts);
    }
    // get the new HostList info from the server adminMessage object

    // Totalview has trouble if given a dotted IP address as the host name when
    // using Bulk Launch to launch the tvdsvr process on the remote hosts.
    useDottedIP = ( RunParameters->TVDebug ) ? false : true;
    for (int i = 0; i < nhosts; i++) {
        if (!server->
            peerName(i, RunParameters->HostList[i],
                     ULM_MAX_HOSTNAME_LEN, useDottedIP)) {
            ulm_err(("server could not get remote IP hostname/address for hostrank %d\n", i));
            Abort();
        }
    }

    // resize ProcessCount and DaemonPIDs arrays and get info from other hosts
    if (nhosts != RunParameters->NHosts) {
        tmpNProc = ulm_new(int, nhosts);
        tmpPIDs = ulm_new(pid_t,  nhosts);
	for( h=0 ; h < nhosts ; h++ ){
		tmpNProc[h]= server->processCountForHostRank(h);
		tmpPIDs[h]= server->daemonPIDForHostRank(h);
	}

        ulm_delete(RunParameters->ProcessCount);
        ulm_delete(RunParameters->DaemonPIDs);

        RunParameters->ProcessCount = tmpNProc;
        RunParameters->DaemonPIDs = tmpPIDs;
    }

    // resize and copy the first element from the following lists: DirName_t WorkingDirList[], ExeName_t ExeList[],
    // DirName_t UserAppDirList[], int NPathTypes[], int *ListPathTypes[] if necessary
    if (RunParameters->NHosts < nhosts) {
        DirName_t *wdl = RunParameters->WorkingDirList;
        ExeName_t *el = RunParameters->ExeList;
        DirName_t *uadl = RunParameters->UserAppDirList;
        int *nndt = RunParameters->NPathTypes;
        int **lndt = RunParameters->ListPathTypes;

        RunParameters->WorkingDirList = ulm_new(DirName_t,  nhosts);
        RunParameters->ExeList = ulm_new(ExeName_t,  nhosts);
        RunParameters->NPathTypes = ulm_new(int, nhosts);
        RunParameters->ListPathTypes = ulm_new( int *, nhosts);
        if (uadl) {
            RunParameters->UserAppDirList = ulm_new(DirName_t,  nhosts);
        }

        for (int i = 0; i < nhosts; i++) {
            strncpy(RunParameters->WorkingDirList[i], wdl[0],
                    ULM_MAX_PATH_LEN);
            strncpy(RunParameters->ExeList[i], el[0], ULM_MAX_PATH_LEN);
            if (uadl) {
                strncpy(RunParameters->UserAppDirList[i], uadl[0],
                        ULM_MAX_PATH_LEN);
            }
            RunParameters->NPathTypes[i] = nndt[0];
            RunParameters->ListPathTypes[i] =
                ulm_new(int, nndt[0]);
            for (int j = 0; j < nndt[0]; j++) {
                RunParameters->ListPathTypes[i][j] = lndt[0][j];
            }
        }

        ulm_delete(wdl);
        ulm_delete(el);
        if (uadl) {
            ulm_delete(uadl);
        }
        for (int i = 0; i < RunParameters->NHosts; i++) {
            ulm_delete(lndt[i]);
        }
        ulm_delete(lndt);
        ulm_delete(nndt);
    }

    // set the number of hosts to the true known number...
    RunParameters->NHosts = nhosts;

    // fill in some adminMessage data - needed for collectives
    //   while mpirun still participates in the collectives.
    nProcs=0;
    groupHostData = (admin_host_info_t *)
	    ulm_malloc(nhosts*sizeof(admin_host_info_t));
    for( host=0 ; host < nhosts ; host++ ){
	    groupHostData[host].groupProcIDOnHost = (int *)0;
	    groupHostData[host].nGroupProcIDOnHost =
		    RunParameters->ProcessCount[host];
	    nProcs += RunParameters->ProcessCount[host];
    }
    server->setGroupHostData(groupHostData);
    server->setLenSharedMemoryBuffer(adminMessage::MINCOLLMEMPERPROC*nProcs);

}
