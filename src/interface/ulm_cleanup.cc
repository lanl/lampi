#include "ulm/ulm.h"
#include "queue/ReliabilityInfo.h"
#include "queue/globals.h"
#include "queue/barrier.h"
#include "path/udp/UDPNetwork.h"
#include "path/common/pathContainer.h"
#include "mem/MemoryPool.h"

void ulm_cleanup()
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

    // clean up lampiState struct
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
