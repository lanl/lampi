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

#include <stdio.h>
#include <unistd.h>
#include <strings.h>            // for bzero
#include <sys/uio.h>

#include "internal/constants.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "client/SocketGeneric.h"
#include "internal/log.h"
#include "run/globals.h"  // for server


/* error macros */
#define TagError(a)  \
{ ulm_err(("Error: SendInitialInputDataToClients: error packing " \
				a "tag \n"));                       \
	    returnValue = ULM_ERROR;                              \
	    return returnValue; }
#define DataError(a)  \
{ ulm_err(("Error: SendInitialInputDataToClients: error packing " \
				a "\n"));                       \
	    returnValue = ULM_ERROR;                              \
	    return returnValue; }
/*
 * This routine is used to send setup data to the clients.
 */
 
int SendInitialInputDataMsgToClients(ULMRunParams_t *RunParameters,
		adminMessage *server)
{
    /* local variables */
    int 			returnValue = ULM_SUCCESS;
    int 			tag, nhosts = RunParameters->NHosts, host, errorCode, bytes;
	unsigned int	cid;

    /* initialize data */

    /*
     * Broadcast data that is identical for all hosts
     */
    if (!server->reset(adminMessage::SEND) ) {
	    ulm_err(("Error: SendInitialInputDataToClients: unable to reset sendbuffer\n"));
	    returnValue = ULM_ERROR;
	    return returnValue;
    }           

    /* number of hosts and process count per host */
    tag = adminMessage::NHOSTS;
    if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
	    TagError("NHOSTS");
		
    if( !server->pack(&nhosts, (adminMessage::packType)sizeof(int), 1))
	    DataError("nhosts");
    if( !server->pack( RunParameters->ProcessCount, 
			    (adminMessage::packType)sizeof(int),nhosts))
	    DataError("ProcessCount");

    /* CTChannel ID of mpirun's CTClient */
    tag = adminMessage::CHANNELID;
    if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
	    TagError("NHOSTS");
		
	cid = server->channelID();
    if( !server->pack(&cid, (adminMessage::packType)sizeof(unsigned int), 1))
	    DataError("ChannelID");

    /* thread usage */
    tag = adminMessage::THREADUSAGE;
    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
	    TagError("THREADUSAGE");
    if(!server->pack(&(RunParameters->UseThreads), 
			    (adminMessage::packType)sizeof(int), 1))
	    DataError("UseThreads");

	    /* Totalview debug settings */
	    tag = adminMessage::TVDEBUG;
	    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
		    TagError("TVDEBUG");
	    if(!server->pack(&(RunParameters->TVDebug),
				    (adminMessage::packType)sizeof(int), 1))
		    DataError("TVDebug");
	    if(!server->pack(&(RunParameters->TVDebugApp),
				    (adminMessage::packType)sizeof(int), 1))
		    TagError("TVDebugApp");


	    /* CRC parameters */
	    tag = adminMessage::CRC;
	    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
		    TagError("CRC");
	    if(!server->pack(&(RunParameters->UseCRC),
				    (adminMessage::packType)sizeof(int), 1))
		    DataError("UseCRC");

	    // MPI argument checking
	    tag = adminMessage::CHECKARGS;
	    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
		    TagError("CHECKARGS");
	    if(!server->pack(&(RunParameters->CheckArgs),
				    (adminMessage::packType)sizeof(int), 1))
		    DataError("CheckArgs");

	    // output prefix
	    tag = adminMessage::OUTPUT_PREFIX;
	    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
		    TagError("OUTPUT_PREFIX");
	    if(!server->pack(&(RunParameters->OutputPrefix), (adminMessage::packType)sizeof(int), 1))
		    DataError("CheckArgs");

#if ENABLE_NUMA
	    /* cpu list */
	    if (RunParameters->CpuListLen != 0) {
		    tag = adminMessage::CPULIST;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("CPULIST");

		    if(!server->pack(&RunParameters->CpuListLen,
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("CpuListLen");

		    if(!server->pack(&RunParameters->CpuList,
					    (adminMessage::packType)sizeof(int), 
					    RunParameters->CpuListLen))
			    DataError("CpuList");
	    }

	    /* number of cpus per node */
	    if (RunParameters->nCpusPerNode != 0) {
		    tag = adminMessage::NCPUSPERNODE;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("NCPUSPERNODE");
		    if(!server->pack(&RunParameters->nCpusPerNode, (adminMessage::packType)sizeof(int), 1))
			    DataError("nCpusPerNode");
	    }

	    /* resource affinity */
	    if (RunParameters->useResourceAffinity != 0) {
		    tag = adminMessage::USERESOURCEAFFINITY;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("USERESOURCEAFFINITY");
		    if(!server->pack(&(RunParameters->useResourceAffinity[0]), 
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("useResourceAffinity");
	    }

	    /* default resource affinigy */
	    if (RunParameters->defaultAffinity != 0) {
		    tag = adminMessage::DEFAULTRESOURCEAFFINITY;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("DEFAULTRESOURCEAFFINITY");
		    if(!server->pack(&(RunParameters->defaultAffinity[0]), 
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("defaultAffinity");
	    }

	    /* mandatory resource affinity flag */
	    if (RunParameters->mandatoryAffinity != 0) {
		    tag = adminMessage::MANDATORYRESOURCEAFFINITY;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("MANDATORYRESOURCEAFFINITY");
		    if(!server->pack(&(RunParameters->mandatoryAffinity[0]), 
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("mandatoryAffinity");
	    }
#endif /* ENABLE_NUMA */

	    /* maximum number of communicators - needed for shared memory resrouces */
	    if (RunParameters->maxCommunicators != 0) {
		    tag = adminMessage::MAXCOMMUNICATORS;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("MAXCOMMUNICATORS");
		    if(!server->pack(RunParameters->maxCommunicators,
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("maxCommunicators");
	    }

	    /* flag indicating to return error status when out of irecv headers, 
	     *   rather than abort */
	    if (RunParameters->irecvResources_m.outOfResourceAbort != NULL) {
		    tag = adminMessage::IRECVOUTOFRESRCABORT;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("IRECVOUTOFRESRCABORT");
	    }
		
	    /* flag indicating how many consective times to try and get more irecv 
	     *   headers before deciding none are available. */
	    if (RunParameters->irecvResources_m.retries_m != NULL) {
		    tag = adminMessage::IRECVRESOURCERETRY;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("IRECVRESOURCERETRY");
		    if(!server->pack(RunParameters->irecvResources_m.retries_m,
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("irecvResources_m.retries_m");
	    }

	    /* send end of input parameters tag */
	    tag = adminMessage::ENDRUNPARAMS;
	    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
		    TagError("ENDRUNPARAMS");
    
    /* broadcast the setup parameters */
    if (!server->broadcast(adminMessage::RUNPARAMS, &errorCode)) {
        ulm_err(("Error: Can't broadcast RUNPARAMS message (error %d)\n",
                 errorCode));
        Abort();
    }

    /* calculate how much buffer space is needed for RUNPARAMS scatter */
    bytes = 0;
    for( host = 0 ; host < nhosts ; host++ ) {
        bytes += (sizeof(int) * (9 + RunParameters->NPathTypes[host])) + (sizeof(ssize_t) * 3);
    }

    /* scatter host specific setup data */

	if (!server->reset(adminMessage::SCATTERV, bytes) ) {
		ulm_err(("Error: SendInitialInputDataToClients: unable to reset sendbuffer - II\n"));
		returnValue = ULM_ERROR;
		return returnValue;
	}           

    /* data is packed in node label order, so we need to translate to host.
       ASSERT: num of labels == num  of hosts.
    */
    
    for( host = 0 ; host < nhosts ; host++ ) {

		/* first item MUST be the sending tag. */
	    tag = adminMessage::RUNPARAMS;
	    if( !server->pack(&tag, (adminMessage::packType)sizeof(int),1) )
		    TagError("RUNPARAMS");

	    /* send host id */
	    tag = adminMessage::HOSTID;
	    if( !server->pack(&tag, (adminMessage::packType)sizeof(int),1) )
		    TagError("HOSTID");
        if( !server->pack( &host, (adminMessage::packType)sizeof(int),1)) 
		    DataError("host index");
        
	    /* send network device information */
	    /* number of devices */
	    tag = adminMessage::NPATHTYPES;
	    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
		    TagError("NPATHTYPES");
	    if( !server->pack( RunParameters->NPathTypes+host, 
				    (adminMessage::packType)sizeof(int),1))
		    DataError("NPathTypes");

	    /* list of devices */
	    if( RunParameters->NPathTypes[host] > 0 ) {
		    if(!server->pack(RunParameters->ListPathTypes[host], 
					    (adminMessage::packType)sizeof(int),
					    RunParameters->NPathTypes[host]))
			    DataError("ListPathTypes");
	    }

	    // minimum number of pages for irecv descriptors per context
	    if (RunParameters->irecvResources_m.minPagesPerContext_m != NULL) {
		    tag = adminMessage::IRCEVMINPAGESPERCTX;
		    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
			    TagError("IRCEVMINPAGESPERCTX");
		    if( !server->pack( &((RunParameters->irecvResources_m.minPagesPerContext_m)[host]),
					    (adminMessage::packType)sizeof(ssize_t), 1)) 
			    DataError("irecvResources_m.minPagesPerContext_m");
	    }

	    // maximum number of pages for irecv descriptors headers per context
	    if (RunParameters->irecvResources_m.maxPagesPerContext_m != NULL) {
		    tag = adminMessage::IRECVMAXPAGESPERCTX;
		    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
			    TagError("IRECVMAXPAGESPERCTX");
		    if( !server->pack( &((RunParameters->irecvResources_m.maxPagesPerContext_m)[host]),
					    (adminMessage::packType)sizeof(ssize_t), 1)) 
			    DataError("irecvResources_m.maxPagesPerContext_m");

	    }
        // upper limit on total number of pages for irecv descriptors
	    if (RunParameters->irecvResources_m.maxTotalPages_m != NULL) {
		    tag = adminMessage::IRECVMAXTOTPAGES;
		    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
			    TagError("IRECVMAXTOTPAGES");
		    if( !server->pack( &((RunParameters->irecvResources_m.maxTotalPages_m)[host]),
					    (adminMessage::packType)sizeof(ssize_t), 1)) 
			    DataError("irecvResources_m.maxTotalPages_m");
	    }

	    /* send end of input parameters tag */
	    tag = adminMessage::ENDRUNPARAMS;
	    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
		    TagError("ENDRUNPARAMS");
    
		server->setScattervHost();
    }
	
    ulm_dbg(("\nmpirun: scattering host RUNPARAMS...\n"));
	/* scatter the setup parameters */
	if ( !server->scatterv(0, adminMessage::RUNPARAMS, &errorCode) )
	{
		ulm_err(("unable to send RUNPARAMS message (error %d)\n",
					errorCode));
			Abort();
	}

    ulm_dbg(("\nmpirun: synching %d members before sending device info...\n", RunParameters->NHosts + 1));
	//server->synchronize(RunParameters->NHosts + 1);
    ulm_dbg(("\nmpirun: done synching %d members before sending device info...\n", RunParameters->NHosts + 1));

    /* 
     * send device specific information 
     */

    /* Shared memory input parameters (host-specific)  */
    returnValue = SendSharedMemInputToClients(RunParameters, server);
    if( returnValue != ULM_SUCCESS ) {
	    ulm_err((" Error returned from SendSharedMemInputToClients - %d \n",returnValue));
	    Abort();
    }

    /* quadrics input parameters */
    returnValue = SendQuadricsInputToClients(RunParameters, server);
    if( returnValue != ULM_SUCCESS ) {
	    ulm_err((" Error returned from SendQuadricsInputToClients - %d \n",returnValue));
	    Abort();
    }

    /* Myrinet GM input parameters */
    returnValue = SendGMInputToClients(RunParameters, server);
    if (returnValue != ULM_SUCCESS) {
        ulm_err((" Error returned from SendGMInputToClients - %d\n", returnValue));
        Abort();
    }

    /* InfiniBand input parameters */
    returnValue = SendIBInputToClients(RunParameters, server);
    if (returnValue != ULM_SUCCESS) {
        ulm_err((" Error returned from SendIBInputToClients - %d\n", returnValue));
        Abort();
    }

    /* TCP input parameters */
    returnValue = SendTCPInputToClients(RunParameters, server);
    if (returnValue != ULM_SUCCESS) {
        ulm_err((" Error returned from SendTCPInputToClients - %d\n", returnValue));
        Abort();
    }

    /* Interface input parameters */
    returnValue = SendInterfaceListToClients(RunParameters, server);
    if (returnValue != ULM_SUCCESS) {
        ulm_err((" Error returned from SendInterfaceListToClients - %d\n", returnValue));
        Abort();
    }

    /* return */
    return returnValue;
}

 

int SendInitialInputDataToClients(ULMRunParams_t *RunParameters,
		adminMessage *server)
{

#if ENABLE_CT
	return SendInitialInputDataMsgToClients(RunParameters, server);
#endif



    /* local variables */
    int returnValue = ULM_SUCCESS;
    int tag, nhosts = RunParameters->NHosts, host, errorCode;

    /* initialize data */

    /*
     * Broadcast data that is identical for all hosts
     */
    if (!server->reset(adminMessage::SEND) ) {
	    ulm_err(("Error: SendInitialInputDataToClients: unable to reset sendbuffer\n"));
	    returnValue = ULM_ERROR;
	    return returnValue;
    }           

    /* number of hosts and process count per host */
    tag = adminMessage::NHOSTS;
    if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
	    TagError("NHOSTS");
		
    if( !server->pack(&nhosts, (adminMessage::packType)sizeof(int), 1))
	    DataError("nhosts");
    if( !server->pack( RunParameters->ProcessCount, 
			    (adminMessage::packType)sizeof(int),nhosts))
	    DataError("ProcessCount");

    /* thread usage */
    tag = adminMessage::THREADUSAGE;
    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
	    TagError("THREADUSAGE");
    if(!server->pack(&(RunParameters->UseThreads), 
			    (adminMessage::packType)sizeof(int), 1))
	    DataError("UseThreads");

	    /* Totalview debug settings */
	    tag = adminMessage::TVDEBUG;
	    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
		    TagError("TVDEBUG");
	    if(!server->pack(&(RunParameters->TVDebug),
				    (adminMessage::packType)sizeof(int), 1))
		    DataError("TVDebug");
	    if(!server->pack(&(RunParameters->TVDebugApp),
				    (adminMessage::packType)sizeof(int), 1))
		    TagError("TVDebugApp");


	    /* CRC parameters */
	    tag = adminMessage::CRC;
	    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
		    TagError("CRC");
	    if(!server->pack(&(RunParameters->UseCRC),
				    (adminMessage::packType)sizeof(int), 1))
		    DataError("UseCRC");

	    // MPI argument checking
	    tag = adminMessage::CHECKARGS;
	    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
		    TagError("CHECKARGS");
	    if(!server->pack(&(RunParameters->CheckArgs),
				    (adminMessage::packType)sizeof(int), 1))
		    DataError("CheckArgs");

	    // output prefix
	    tag = adminMessage::OUTPUT_PREFIX;
	    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
		    TagError("OUTPUT_PREFIX");
	    if(!server->pack(&(RunParameters->OutputPrefix), (adminMessage::packType)sizeof(int), 1))
		    DataError("CheckArgs");

#if ENABLE_NUMA
	    /* cpu list */
	    if (RunParameters->CpuListLen != 0) {
		    tag = adminMessage::CPULIST;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("CPULIST");

		    if(!server->pack(&RunParameters->CpuListLen,
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("CpuListLen");

		    if(!server->pack(&RunParameters->CpuList,
					    (adminMessage::packType)sizeof(int), 
					    RunParameters->CpuListLen))
			    DataError("CpuList");
	    }

	    /* number of cpus per node */
	    if (RunParameters->nCpusPerNode != 0) {
		    tag = adminMessage::NCPUSPERNODE;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("NCPUSPERNODE");
		    if(!server->pack(&RunParameters->nCpusPerNode, (adminMessage::packType)sizeof(int), 1))
			    DataError("nCpusPerNode");
	    }

	    /* resource affinity */
	    if (RunParameters->useResourceAffinity != 0) {
		    tag = adminMessage::USERESOURCEAFFINITY;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("USERESOURCEAFFINITY");
		    if(!server->pack(&(RunParameters->useResourceAffinity[0]), 
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("useResourceAffinity");
	    }

	    /* default resource affinigy */
	    if (RunParameters->defaultAffinity != 0) {
		    tag = adminMessage::DEFAULTRESOURCEAFFINITY;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("DEFAULTRESOURCEAFFINITY");
		    if(!server->pack(&(RunParameters->defaultAffinity[0]), 
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("defaultAffinity");
	    }

	    /* mandatory resource affinity flag */
	    if (RunParameters->mandatoryAffinity != 0) {
		    tag = adminMessage::MANDATORYRESOURCEAFFINITY;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("MANDATORYRESOURCEAFFINITY");
		    if(!server->pack(&(RunParameters->mandatoryAffinity[0]), 
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("mandatoryAffinity");
	    }
#endif /* ENABLE_NUMA */

	    /* maximum number of communicators - needed for shared memory resrouces */
	    if (RunParameters->maxCommunicators != 0) {
		    tag = adminMessage::MAXCOMMUNICATORS;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("MAXCOMMUNICATORS");
		    if(!server->pack(RunParameters->maxCommunicators,
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("maxCommunicators");
	    }

	    /* flag indicating to return error status when out of irecv headers, 
	     *   rather than abort */
	    if (RunParameters->irecvResources_m.outOfResourceAbort != NULL) {
		    tag = adminMessage::IRECVOUTOFRESRCABORT;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("IRECVOUTOFRESRCABORT");
	    }
		
	    /* flag indicating how many consective times to try and get more irecv 
	     *   headers before deciding none are available. */
	    if (RunParameters->irecvResources_m.retries_m != NULL) {
		    tag = adminMessage::IRECVRESOURCERETRY;
		    if(!server->pack(&tag, (adminMessage::packType)sizeof(int), 1))
			    TagError("IRECVRESOURCERETRY");
		    if(!server->pack(RunParameters->irecvResources_m.retries_m,
					    (adminMessage::packType)sizeof(int), 1))
			    DataError("irecvResources_m.retries_m");
	    }

    /* broadcast the setup parameters */
    if (!server->broadcast(adminMessage::RUNPARAMS, &errorCode)) {
        ulm_err(("Error: Can't broadcast RUNPARAMS message (error %d)\n",
                 errorCode));
        Abort();
    }

    /* send host specific setup data */

    for( host = 0 ; host < nhosts ; host++ ) {

	    if (!server->reset(adminMessage::SEND) ) {
		    ulm_err(("Error: SendInitialInputDataToClients: unable to reset sendbuffer - II\n"));
		    returnValue = ULM_ERROR;
		    return returnValue;
	    }           

	    /* send host id */
	    tag = adminMessage::HOSTID;
	    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
		    TagError("HOSTID");
	    if( !server->pack( &host, (adminMessage::packType)sizeof(int),1)) 
		    DataError("host index");
        
	    /* send network device information */
	    /* number of devices */
	    tag = adminMessage::NPATHTYPES;
	    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
		    TagError("NPATHTYPES");
	    if( !server->pack( RunParameters->NPathTypes+host, 
				    (adminMessage::packType)sizeof(int),1))
		    DataError("NPathTypes");

	    /* list of devices */
	    if( RunParameters->NPathTypes[host] > 0 ) {
		    if(!server->pack(RunParameters->ListPathTypes[host], 
					    (adminMessage::packType)sizeof(int),
					    RunParameters->NPathTypes[host]))
			    DataError("ListPathTypes");
	    }

	    // minimum number of pages for irecv descriptors per context
	    if (RunParameters->irecvResources_m.minPagesPerContext_m != NULL) {
		    tag = adminMessage::IRCEVMINPAGESPERCTX;
		    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
			    TagError("IRCEVMINPAGESPERCTX");
		    if( !server->pack( &((RunParameters->irecvResources_m.minPagesPerContext_m)[host]),
					    (adminMessage::packType)sizeof(ssize_t), 1)) 
			    DataError("irecvResources_m.minPagesPerContext_m");
	    }

	    // maximum number of pages for irecv descriptors headers per context
	    if (RunParameters->irecvResources_m.maxPagesPerContext_m != NULL) {
		    tag = adminMessage::IRECVMAXPAGESPERCTX;
		    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
			    TagError("IRECVMAXPAGESPERCTX");
		    if( !server->pack( &((RunParameters->irecvResources_m.maxPagesPerContext_m)[host]),
					    (adminMessage::packType)sizeof(ssize_t), 1)) 
			    DataError("irecvResources_m.maxPagesPerContext_m");

	    }
        // upper limit on total number of pages for irecv descriptors
	    if (RunParameters->irecvResources_m.maxTotalPages_m != NULL) {
		    tag = adminMessage::IRECVMAXTOTPAGES;
		    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
			    TagError("IRECVMAXTOTPAGES");
		    if( !server->pack( &((RunParameters->irecvResources_m.maxTotalPages_m)[host]),
					    (adminMessage::packType)sizeof(ssize_t), 1)) 
			    DataError("irecvResources_m.maxTotalPages_m");
	    }

	    /* send end of input parameters tag */
	    tag = adminMessage::ENDRUNPARAMS;
	    if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
		    TagError("ENDRUNPARAMS");
    
	    /* send the setup parameters */
	    if (!server->send(host,adminMessage::RUNPARAMS, &errorCode)) {
		    ulm_err(("unable to send RUNPARAMS message (error %d)\n",
			   		    errorCode));
	    	    Abort();
	    }
    }

    /* 
     * send device specific information 
     */

    /* Shared memory input parameters */
    returnValue = SendSharedMemInputToClients(RunParameters, server);
    if( returnValue != ULM_SUCCESS ) {
	    ulm_err((" Error returned from SendSharedMemInputToClients - %d \n",returnValue));
	    Abort();
    }

    /* quadrics input parameters */
    returnValue = SendQuadricsInputToClients(RunParameters, server);
    if( returnValue != ULM_SUCCESS ) {
	    ulm_err((" Error returned from SendQuadricsInputToClients - %d \n",returnValue));
	    Abort();
    }


    /* Myrinet GM input parameters */
    returnValue = SendGMInputToClients(RunParameters, server);
    if (returnValue != ULM_SUCCESS) {
        ulm_err((" Error returned from SendGMInputToClients - %d\n", returnValue));
        Abort();
    }

    /* InfiniBand input parameters */
    returnValue = SendIBInputToClients(RunParameters, server);
    if (returnValue != ULM_SUCCESS) {
        ulm_err((" Error returned from SendIBInputToClients - %d\n", returnValue));
        Abort();
    }

    /* TCP input parameters */
    returnValue = SendTCPInputToClients(RunParameters, server);
    if (returnValue != ULM_SUCCESS) {
        ulm_err((" Error returned from SendTCPInputToClients - %d\n", returnValue));
        Abort();
    }

    /* Interface input parameters */
    returnValue = SendInterfaceListToClients(RunParameters, server);
    if (returnValue != ULM_SUCCESS) {
        ulm_err((" Error returned from SendInterfaceListToClients - %d\n", returnValue));
        Abort();
    }

    /* return */
    return returnValue;


}

int SendSharedMemInputToClients(ULMRunParams_t *RunParameters,
		adminMessage *server)
{
	int 			host, returnValue = ULM_SUCCESS;
	int 			errorCode, tag;

	/* will use shared memory to send to self, so always do
	 * shared memory setup */
	RunParameters->Networks.UseSharedMemory = 1;

	/* send input data */
#if ENABLE_CT
	if (!server->reset(adminMessage::SCATTERV) ) {
		ulm_err(("Error: SendSharedMemInputToClients: unable to reset sendbuffer\n"));
		returnValue = ULM_ERROR;
		return returnValue;
	}           
#endif

	/* loop over application hosts */
    for (host = 0; host < RunParameters->NHosts ; host++) {

	    /* initialize send buffer */
#if ENABLE_CT == 0
	    if (!server->reset(adminMessage::SEND) ) {
		    ulm_err(("Error: SendSharedMemInputToClients: unable to reset sendbuffer\n"));
		    returnValue = ULM_ERROR;
		    return returnValue;
	    }           
#else        
		/* first item MUST be the sending tag. */
	    tag = dev_type_params::START_SHARED_MEM_INPUT;
	    if( !server->pack(&tag, (adminMessage::packType)sizeof(int),1) )
		    TagError("START_SHARED_MEM_INPUT");

#endif

        /* number of bytes per process for the shared memory descriptor pool */
        if (RunParameters->smDescPoolBytesPerProc != 0) {
		tag = shared_mem_intput_params::SMDESCPOOLBYTESPERPROC;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMDESCPOOLBYTESPERPROC");
		if( !server->pack(((RunParameters->smDescPoolBytesPerProc) + host),
				       	(adminMessage::packType)sizeof(ssize_t),
				       	1))
			DataError("smDescPoolBytesPerProc");
        }

        /* shared memory per proc page limit */
        if (RunParameters->Networks.SharedMemSetup.PagesPerProc != NULL) {
		tag = shared_mem_intput_params::SMPSMPAGESPERPROC;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPSMPAGESPERPROC");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							PagesPerProc) + host),
				       	(adminMessage::packType)sizeof(int), 1))
			DataError("PagesPerProc");
        }
        /* flag indicating to return error status when out of SMP message
	 * fragment headers, rather than abort */
        if (RunParameters->Networks.SharedMemSetup.recvFragResources_m.
			outOfResourceAbort != NULL) {
		tag = shared_mem_intput_params::SMPFRAGOUTOFRESRCABORT;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPFRAGOUTOFRESRCABORT");
        }
        // flag indicating how many consective times to try and get more SMP message fragment
        //   headers before deciding none are available.
        if (RunParameters->Networks.SharedMemSetup.recvFragResources_m.
            retries_m != NULL) {
		tag = shared_mem_intput_params::SMPFRAGRESOURCERETRY;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPFRAGRESOURCERETRY");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							recvFragResources_m.
							retries_m) + host),
				       	(adminMessage::packType)sizeof(long),1))
			DataError("recvFragResources_m.retries_m");
        }
        /* minimum number of pages for SMP message fragment headers per context */
        if (RunParameters->Networks.SharedMemSetup.recvFragResources_m.
			minPagesPerContext_m != NULL) {
		tag = shared_mem_intput_params::SMPFRAGMINPAGESPERCTX;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPFRAGMINPAGESPERCTX");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							recvFragResources_m.
							minPagesPerContext_m)+host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("recvFragResources_m.minPagesPerContext_m");
        }
        /* maximum number of pages for SMP message fragment headers per context */
        if (RunParameters->Networks.SharedMemSetup.recvFragResources_m.
			maxPagesPerContext_m != NULL) {
		tag = shared_mem_intput_params::SMPFRAGMAXPAGESPERCTX;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPFRAGMAXPAGESPERCTX");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							recvFragResources_m.
							maxPagesPerContext_m)+host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("recvFragResources_m.maxPagesPerContext_m");
        }
        /* upper limiton total number of pages for SMP message fragment headers */
        if (RunParameters->Networks.SharedMemSetup.recvFragResources_m.
			maxTotalPages_m != NULL) {
		tag = shared_mem_intput_params::SMPFRAGMAXTOTPAGES;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPFRAGMAXTOTPAGES");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							recvFragResources_m.
							maxTotalPages_m) + host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("recvFragResources_m.maxTotalPages_m");
        }
        /* flag indicating to return error status when out of SMP isend message
	 *  headers, rather than abort */
        if (RunParameters->Networks.SharedMemSetup.isendDescResources_m.
			outOfResourceAbort != NULL) {
		tag = shared_mem_intput_params::SMPISENDOUTOFRESRCABORT;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPISENDOUTOFRESRCABORT");
        }
        /* flag indicating how many consective times to try and get more SMP
	 * isend message headers before deciding none are available. */
        if (RunParameters->Networks.SharedMemSetup.isendDescResources_m.
			retries_m != NULL) {
		tag = shared_mem_intput_params::SMPISENDRESOURCERETRY;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPISENDRESOURCERETRY");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							isendDescResources_m.
							retries_m) + host),
				       	(adminMessage::packType)sizeof(long), 1))
			DataError("isendDescResources_m.retries_m");
        }
        /* minimum number of pages for SMP message isend descriptors per context */
        if (RunParameters->Networks.SharedMemSetup.isendDescResources_m.
			minPagesPerContext_m != NULL) {
		tag = shared_mem_intput_params::SMPISENDMINPAGESPERCTX;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPISENDMINPAGESPERCTX");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							isendDescResources_m.
							minPagesPerContext_m) + host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("isendDescResources_m.minPagesPerContext_m");
        }
        /* maximum number of pages for SMP message isend descriptors headers
	 * per context */
        if (RunParameters->Networks.SharedMemSetup.isendDescResources_m.
			maxPagesPerContext_m != NULL) {
		tag = shared_mem_intput_params::SMPISENDMAXPAGESPERCTX;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPISENDMAXPAGESPERCTX");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							isendDescResources_m.
							maxPagesPerContext_m) + host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("isendDescResources_m.maxPagesPerContext_m");
        }
        /* upper limiton total number of pages for SMP message isend descriptors */
        if (RunParameters->Networks.SharedMemSetup.isendDescResources_m.
			maxTotalPages_m != NULL) {
		tag = shared_mem_intput_params::SMPISENDMAXTOTPAGES;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPISENDMAXTOTPAGES");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							isendDescResources_m.
							maxTotalPages_m) + host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("isendDescResources_m.maxTotalPages_m");
        }
        /* flag indicating to return error status when out of SMP data buffers,
	 * rather than abort */
        if (RunParameters->Networks.SharedMemSetup.
			SMPDataBuffersResources_m.outOfResourceAbort != NULL) {
		tag = shared_mem_intput_params::SMPDATAPAGESOUTOFRESRCABORT;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPDATAPAGESOUTOFRESRCABORT");
        }
        /* flag indicating how many consective times to try and get more SMP
	 * data buffers before deciding none are available. */
        if (RunParameters->Networks.SharedMemSetup.
			SMPDataBuffersResources_m.retries_m != NULL) {
		tag = shared_mem_intput_params::SMPDATAPAGESRESOURCERETRY;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPDATAPAGESRESOURCERETRY");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							SMPDataBuffersResources_m.
							retries_m) + host),
				       	(adminMessage::packType)sizeof(long), 1))
			DataError("SMPDataBuffersResources_m.retries_m");
        }
        /* minimum number of pages for SMP data buffers per context */
        if (RunParameters->Networks.SharedMemSetup.SMPDataBuffersResources_m.
			minPagesPerContext_m != NULL) {
		tag = shared_mem_intput_params::SMPDATAPAGESMINPAGESPERCTX;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPDATAPAGESMINPAGESPERCTX");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							SMPDataBuffersResources_m.
							minPagesPerContext_m) + host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("SMPDataBuffersResources_m.minPagesPerContext_m");
        }
        /* maximum number of pages for SMP data buffers headers per context */
        if (RunParameters->Networks.SharedMemSetup.
			SMPDataBuffersResources_m.maxPagesPerContext_m != NULL){
		tag = shared_mem_intput_params::SMPDATAPAGESMAXPAGESPERCTX;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPDATAPAGESMAXPAGESPERCTX");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							SMPDataBuffersResources_m.
							maxPagesPerContext_m) + host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("SMPDataBuffersResources_m.maxPagesPerContext_m");
        }
        /* upper limiton total number of pages for SMP data buffers */
        if (RunParameters->Networks.SharedMemSetup.
			SMPDataBuffersResources_m.maxTotalPages_m != NULL) {
		tag = shared_mem_intput_params::SMPDATAPAGESMAXTOTPAGES;
		if( !server->pack(&tag, (adminMessage::packType)sizeof(int), 1) )
			TagError("SMPDATAPAGESMAXTOTPAGES");
		if( !server->pack(((RunParameters->Networks.SharedMemSetup.
							SMPDataBuffersResources_m.
							maxTotalPages_m) + host),
				       	(adminMessage::packType)sizeof(ssize_t), 1))
			DataError("SMPDataBuffersResources_m.maxTotalPages_m");
        }
	/* send end of input parameters tag */
       	tag = dev_type_params::END_SHARED_MEM_INPUT;
	if( !server->pack(&tag,(adminMessage::packType)sizeof(int),1) )
		TagError("END_SHARED_MEM_INPUT");
    
#if ENABLE_CT
	server->setScattervHost();
#else
	/* send the setup parameters */
	if (!server->send(host,dev_type_params::START_SHARED_MEM_INPUT,
				&errorCode)) {
		ulm_err(("Error: Can't send START_SHARED_MEM_INPUT message (error %d)\n", errorCode));
    		Abort();
	}
#endif

    }                           // end loop over hosts

#if ENABLE_CT
	/* send the setup parameters */
    ulm_dbg( ("mpirun: scattering shared mem input...\n") );
	if (!server->scatterv(0,dev_type_params::START_SHARED_MEM_INPUT,
				&errorCode)) {
		ulm_err(("Error: Can't send START_SHARED_MEM_INPUT message (error %d)\n", errorCode));
    		Abort();
	}
#endif

	/* return */
	return returnValue;
}

int SendQuadricsInputToClients(ULMRunParams_t *RunParameters,
		adminMessage *server)
{
    int returnValue = ULM_SUCCESS;
    int quadricshosts = 0, tag, errorCode;


    for (int i = 0; i < RunParameters->NHosts; i++) {
        for (int j = 0; j < RunParameters->NPathTypes[i]; j++) {
            if (RunParameters->ListPathTypes[i][j] == PATH_QUADRICS) {
                quadricshosts++;
                break;
            }
        }
    }

    if (quadricshosts == 0) {
        return returnValue;
    } else if (quadricshosts != RunParameters->NHosts) {
        ulm_err(("Error: broadcastQuadricsFlags %d hosts out of %d using Quadrics!\n", quadricshosts, RunParameters->NHosts));
        returnValue = ULM_ERROR;
        return returnValue;
    }
    RunParameters->Networks.UseQSW = 1;

    // we don't do any of this if there is only one host...
    if (RunParameters->NHosts == 1)
        return returnValue;

    ulm_dbg( ("mpirun: Bcasting quadrics input...\n") );

    server->reset(adminMessage::SEND);
    server->pack(&(RunParameters->quadricsHW), 
           (adminMessage::packType)sizeof(int), 1);
    server->pack(&(RunParameters->quadricsDoAck), 
    	    (adminMessage::packType)sizeof(int), 1);
    server->pack(&(RunParameters->quadricsDoChecksum), 
		    (adminMessage::packType)sizeof(int), 1);
    tag = dev_type_params::END_QUADRICS_INPUT;
    server->pack(&tag, adminMessage::INTEGER, 1);
    if ( !server->broadcast(dev_type_params::START_QUADRICS_INPUT,
		    &errorCode) )
	{
		ulm_err(("Error: Can't broadcast quadrics input message (error %d)\n", errorCode));
    		Abort();
	}
        
    return returnValue;
}

int SendGMInputToClients(ULMRunParams_t *RunParameters,
		adminMessage *server)
{
    int returnValue = ULM_SUCCESS;
#if ENABLE_GM
    int tag,errorCode;

    if (RunParameters->Networks.GMSetup.NGMHosts == 0) {
        return returnValue;
    } else if (RunParameters->Networks.GMSetup.NGMHosts != RunParameters->NHosts) {
        ulm_err(("Error: SendGMInputToClients %d hosts out of %d using Myrinet GM!\n", 
            RunParameters->Networks.GMSetup.NGMHosts, RunParameters->NHosts));
        returnValue = ULM_ERROR;
        return returnValue;
    }
    RunParameters->Networks.UseGM = 1;

    // we don't do any of this if there is only one host...
    if (RunParameters->NHosts == 1)
        return returnValue;

    ulm_dbg( ("mpirun: Bcasting GM input...\n") );
    server->reset(adminMessage::SEND);
    server->pack(&(RunParameters->Networks.GMSetup.fragSize), 
		    (adminMessage::packType)sizeof(size_t), 1);
    server->pack(&(RunParameters->Networks.GMSetup.doAck), 
		    (adminMessage::packType)sizeof(bool), 1);
    server->pack(&(RunParameters->Networks.GMSetup.doChecksum), 
		    (adminMessage::packType)sizeof(bool), 1);
    tag = dev_type_params::END_GM_INPUT;
    server->pack(&tag, adminMessage::INTEGER, 1);
    if ( !server->broadcast(dev_type_params::START_GM_INPUT,
		    &errorCode) )
	{
		ulm_err(("Error: Can't broadcast GM input message (error %d)\n", errorCode));
    		Abort();
	}
#endif
        
    return returnValue;
}


int SendIBInputToClients(ULMRunParams_t *RunParameters,
		adminMessage *server)
{
    int returnValue = ULM_SUCCESS;
#if ENABLE_INFINIBAND
    int tag,errorCode;

    if (RunParameters->Networks.IBSetup.NIBHosts == 0) {
        return returnValue;
    } else if (RunParameters->Networks.IBSetup.NIBHosts != RunParameters->NHosts) {
        ulm_err(("Error: SendIBInputToClients %d hosts out of %d using InfiniBand!\n", 
            RunParameters->Networks.IBSetup.NIBHosts, RunParameters->NHosts));
        returnValue = ULM_ERROR;
        return returnValue;
    }

    // we don't do any of this if there is only one host...
    if (RunParameters->NHosts == 1)
        return returnValue;

    ulm_dbg( ("mpirun: Bcasting IB input...\n") );
    server->reset(adminMessage::SEND);
    server->pack(&(RunParameters->Networks.IBSetup.ack), 
		    (adminMessage::packType)sizeof(bool), 1);
    server->pack(&(RunParameters->Networks.IBSetup.checksum), 
		    (adminMessage::packType)sizeof(bool), 1);
    tag = dev_type_params::END_IB_INPUT;
    server->pack(&tag, adminMessage::INTEGER, 1);
    if ( !server->broadcast(dev_type_params::START_IB_INPUT,
		    &errorCode) )
	{
		ulm_err(("Error: Can't broadcast IB input message (error %d)\n", errorCode));
    		Abort();
	}
#endif
        
    return returnValue;
}


int SendTCPInputToClients(ULMRunParams_t *RunParameters, adminMessage *server)
{
#if ENABLE_TCP
    int tag,errorCode;
    int tcphosts = 0;

    // we don't do any of this if there is only one host...
    if (RunParameters->NHosts == 1)
        return ULM_SUCCESS;

    // skip this if TCP is not configured
    for (int i = 0; i < RunParameters->NHosts; i++) {
        for (int j = 0; j < RunParameters->NPathTypes[i]; j++) {
            if (RunParameters->ListPathTypes[i][j] == PATH_TCP) {
                tcphosts++;
                break;
            }
        }
    }
    if(tcphosts == 0)
        return ULM_SUCCESS;

    if (RunParameters->NHosts != tcphosts) {
        ulm_err(("Error: SendTCPInputToClients %d hosts out of %d using TCP!\n", 
            tcphosts, RunParameters->NHosts));
        return ULM_ERROR;
    }

    ulm_dbg(("mpirun: Bcasting TCP input...\n"));
    server->reset(adminMessage::SEND);
    server->pack(&RunParameters->Networks.TCPSetup.MaxFragmentSize, 
		    (adminMessage::packType)sizeof(size_t), 1);
    server->pack(&RunParameters->Networks.TCPSetup.MaxEagerSendSize, 
		    (adminMessage::packType)sizeof(size_t), 1);
    server->pack(&RunParameters->Networks.TCPSetup.MaxConnectRetries, 
		    (adminMessage::packType)sizeof(int), 1);
    tag = dev_type_params::END_TCP_INPUT;
    server->pack(&tag, adminMessage::INTEGER, 1);
    if ( !server->broadcast(dev_type_params::START_TCP_INPUT, &errorCode) ) {
        ulm_err(("Error: Can't broadcast TCP input message (error %d)\n", errorCode));
        Abort();
    }
#endif
    return ULM_SUCCESS;
}

int SendInterfaceListToClients(ULMRunParams_t *RunParameters, adminMessage *server)
{
    int errorCode;
    int tcphosts = 0, udphosts = 0;

    // we don't do any of this if there is only one host...
    if (RunParameters->NHosts == 1)
        return ULM_SUCCESS;

    // skip this if both TCP and UDP are disabled
    for (int i = 0; i < RunParameters->NHosts; i++) {
        for (int j = 0; j < RunParameters->NPathTypes[i]; j++) {
            if (RunParameters->ListPathTypes[i][j] == PATH_TCP) {
                tcphosts++;
                break;
            }
            if (RunParameters->ListPathTypes[i][j] == PATH_UDP) {
                tcphosts++;
                break;
            }
        }
    }
    if(tcphosts == 0 && udphosts == 0)
        return ULM_SUCCESS;

    ulm_dbg(("mpirun: Bcasting IF names input...\n"));
    if (!server->reset(adminMessage::SEND)) {
        ulm_err(("Error: SendInterfaceListToClients: unable to allocate send buffer!\n"));
        Abort();
    }
    if(!server->pack(&RunParameters->NInterfaces, adminMessage::BYTE,
                     sizeof(RunParameters->NInterfaces))) {
        ulm_err(("Error: SendInterfaceListToClients: unable to pack NInterfaces\n"));
        Abort();
    }
    if(RunParameters->NInterfaces &&
       !server->pack(RunParameters->InterfaceList, adminMessage::BYTE,
                     RunParameters->NInterfaces*sizeof(InterfaceName_t))) {
        ulm_err(("Error: SendInterfaceListToClients: unable to pack InterfaceList\n"));
        Abort();
    }
    if(!server->broadcast(adminMessage::IFNAMES, &errorCode)) {
        ulm_err(("Error: SendInterfaceListToClients: unable to broadcast interface list.\n"));
        Abort();
    }
    return ULM_SUCCESS;
}

