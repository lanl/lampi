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

#include "ulm/ulm.h"
#include "queue/ReliabilityInfo.h"
#include "queue/globals.h"
#include "queue/barrier.h"
#include "path/udp/UDPNetwork.h"
#include "path/common/pathContainer.h"
#include "mem/MemoryPool.h"

void ulm_cleanup(void)
{
    int i, j;

#if ENABLE_RELIABILITY
    if (reliabilityInfo) {
        // Note the explicit d'tor call, we must do this since
        // reliabilityInfo was created in a segment of shared
        // memory using placement new.
        reliabilityInfo->ReliabilityInfo::~ReliabilityInfo();
    }
#endif

    if (ShareMemDescPool) {
        ShareMemDescPool->MemoryPoolShared_t::~MemoryPoolShared_t();
    }
    if (largeShareMemDescPool) {
        largeShareMemDescPool->MemoryPoolShared_t::~MemoryPoolShared_t();
    }
   
    if (communicators) {
        for (i = 0; i < communicatorsArrayLen; i++) {
            if (communicators[i]) {
                communicators[i]->canFreeCommunicator = true;
                ulm_comm_free(i);
            }
        } 
        ulm_free(communicators);
        communicators = 0;
    }
    if (activeCommunicators) {
        ulm_delete(activeCommunicators);
        activeCommunicators = 0;
    }

    if (UDPGlobals::UDPNet) {
        delete(UDPGlobals::UDPNet);
        UDPGlobals::UDPNet = 0;
    }

    if (lampiState.pathContainer) {
        delete(lampiState.pathContainer);
        lampiState.pathContainer = 0;
    }

    if (0) {  // this appears to cause trouble...
        // clean up the software barrier structures
        if (swBarrier.nElementsInPool && swBarrier.pool) {
            for (i = 0; i < swBarrier.nPools; i++) {
                for (j = 0; j < swBarrier.nElementsInPool[i]; j++) {
                    if (swBarrier.pool[i][j].barrierData) {
                        ulm_free(swBarrier.pool[i][j].barrierData);
                        swBarrier.pool[i][j].barrierData = 0;
                    }
                }
            }
            ulm_free(swBarrier.nElementsInPool);
            swBarrier.nElementsInPool = 0;

            ulm_free(swBarrier.pool);
            swBarrier.pool = 0;
        }
    }

    // clean up _ulm struct
    if (lampiState.map_global_rank_to_host) {
        ulm_delete(lampiState.map_global_rank_to_host);
        lampiState.map_global_rank_to_host = 0;
    }
    if (lampiState.map_host_to_local_size) {
        ulm_delete(lampiState.map_host_to_local_size);
        lampiState.map_host_to_local_size = 0;
    }
    if (lampiState.map_global_proc_to_on_host_proc_id) {
        ulm_free(lampiState.map_global_proc_to_on_host_proc_id);
        lampiState.map_global_proc_to_on_host_proc_id = 0;
    }

    // clean up the group pools
    if (grpPool.groups) {
        ulm_delete(grpPool.groups);
        grpPool.groups = 0;
    }

    // Clean up mpi operations
    if (((ULMOp_t *)MPI_MAX)->func) {
        ulm_free(((ULMOp_t *)MPI_MAX)->func);
        ((ULMOp_t *)MPI_MAX)->func = 0;
    }
    if (((ULMOp_t *)MPI_MIN)->func) {
        ulm_free(((ULMOp_t *)MPI_MIN)->func);
        ((ULMOp_t *)MPI_MIN)->func = 0;
    }
    if (((ULMOp_t *)MPI_SUM)->func) {
        ulm_free(((ULMOp_t *)MPI_SUM)->func);
        ((ULMOp_t *)MPI_SUM)->func = 0;
    }
    if (((ULMOp_t *)MPI_PROD)->func) {
        ulm_free(((ULMOp_t *)MPI_PROD)->func);
        ((ULMOp_t *)MPI_PROD)->func = 0;
    }
    if (((ULMOp_t *)MPI_MAXLOC)->func) {
        ulm_free(((ULMOp_t *)MPI_MAXLOC)->func);
        ((ULMOp_t *)MPI_MAXLOC)->func = 0;
    }
    if (((ULMOp_t *)MPI_MINLOC)->func) {
        ulm_free(((ULMOp_t *)MPI_MINLOC)->func);
        ((ULMOp_t *)MPI_MINLOC)->func = 0;
    }
    if (((ULMOp_t *)MPI_BAND)->func) {
        ulm_free(((ULMOp_t *)MPI_BAND)->func);
        ((ULMOp_t *)MPI_BAND)->func = 0;
    }
    if (((ULMOp_t *)MPI_BOR)->func) {
        ulm_free(((ULMOp_t *)MPI_BOR)->func);
        ((ULMOp_t *)MPI_BOR)->func = 0;
    }
    if (((ULMOp_t *)MPI_BXOR)->func) {
        ulm_free(((ULMOp_t *)MPI_BXOR)->func);
        ((ULMOp_t *)MPI_BXOR)->func = 0;
    }
    if (((ULMOp_t *)MPI_LAND)->func) {
        ulm_free(((ULMOp_t *)MPI_LAND)->func);
        ((ULMOp_t *)MPI_LAND)->func = 0;
    }
    if (((ULMOp_t *)MPI_LOR)->func) {
        ulm_free(((ULMOp_t *)MPI_LOR)->func);
        ((ULMOp_t *)MPI_LOR)->func = 0;
    }
    if (((ULMOp_t *)MPI_LXOR)->func) {
        ulm_free(((ULMOp_t *)MPI_LXOR)->func);
        ((ULMOp_t *)MPI_LXOR)->func = 0;
    }
    return;
}
