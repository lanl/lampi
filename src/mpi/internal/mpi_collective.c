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



#include "internal/mpi.h"
#include "internal/collective.h"

int _mpi_init_collectives(void)
{
    _mpi.collective.allgather = ulm_allgather;
    _mpi.collective.allgatherv = ulm_allgatherv;
    _mpi.collective.allreduce = ulm_allreduce;
    _mpi.collective.alltoall = ulm_alltoall;
    _mpi.collective.alltoallv = ulm_alltoallv;
    _mpi.collective.alltoallw = NULL; /* ulm_alltoallw; */
    _mpi.collective.barrier = ulm_barrier;
    _mpi.collective.bcast = ulm_bcast;
    _mpi.collective.gather = ulm_gather;
    _mpi.collective.gatherv = ulm_gatherv;
    _mpi.collective.reduce = ulm_reduce;
    _mpi.collective.reduce_scatter = NULL; /*ulm_reduce_scatter; */
    _mpi.collective.scan = ulm_scan;
    _mpi.collective.scatter = ulm_scatter;
    _mpi.collective.scatterv = ulm_scatterv;

    return 0;
}

int ulm_get_collective_function(int key, void **func)
{
    switch (key) {
    case ULM_COLLECTIVE_ALLGATHER:
        *func = (void *) _mpi.collective.allgather;
        break;
    case ULM_COLLECTIVE_ALLGATHERV:
        *func = (void *) _mpi.collective.allgatherv;
        break;
    case ULM_COLLECTIVE_ALLREDUCE:
        *func = (void *) _mpi.collective.allreduce;
        break;
    case ULM_COLLECTIVE_ALLTOALL:
        *func = (void *) _mpi.collective.alltoall;
        break;
    case ULM_COLLECTIVE_ALLTOALLV:
        *func = (void *) _mpi.collective.alltoallv;
        break;
    case ULM_COLLECTIVE_ALLTOALLW:
        *func = (void *) _mpi.collective.alltoallw;
        break;
    case ULM_COLLECTIVE_BARRIER:
        *func = (void *) _mpi.collective.barrier;
        break;
    case ULM_COLLECTIVE_BCAST:
        *func = (void *) _mpi.collective.bcast;
        break;
    case ULM_COLLECTIVE_GATHER:
        *func = (void *) _mpi.collective.gather;
        break;
    case ULM_COLLECTIVE_GATHERV:
        *func = (void *) _mpi.collective.gatherv;
        break;
    case ULM_COLLECTIVE_REDUCE:
        *func = (void *) _mpi.collective.reduce;
        break;
    case ULM_COLLECTIVE_REDUCE_SCATTER:
        *func = (void *) _mpi.collective.reduce_scatter;
        break;
    case ULM_COLLECTIVE_SCAN:
        *func = (void *) _mpi.collective.scan;
        break;
    case ULM_COLLECTIVE_SCATTER:
        *func = (void *) _mpi.collective.scatter;
        break;
    case ULM_COLLECTIVE_SCATTERV:
        *func = (void *) _mpi.collective.scatterv;
        break;
    default:
        return ULM_ERR_BAD_PARAM;
    }

    return ULM_SUCCESS;
}


int ulm_set_collective_function(int key, void *func)
{
    switch (key) {
    case ULM_COLLECTIVE_ALLGATHER:
        _mpi.collective.allgather = (ulm_allgather_t *) func;
        break;
    case ULM_COLLECTIVE_ALLGATHERV:
        _mpi.collective.allgatherv = (ulm_allgatherv_t *) func;
        break;
    case ULM_COLLECTIVE_ALLREDUCE:
        _mpi.collective.allreduce = (ulm_allreduce_t *) func;
        break;
    case ULM_COLLECTIVE_ALLTOALL:
        _mpi.collective.alltoall = (ulm_alltoall_t *) func;
        break;
    case ULM_COLLECTIVE_ALLTOALLV:
        _mpi.collective.alltoallv = (ulm_alltoallv_t *) func;
        break;
    case ULM_COLLECTIVE_ALLTOALLW:
        _mpi.collective.alltoallw = (ulm_alltoallw_t *) func;
        break;
    case ULM_COLLECTIVE_BARRIER:
        _mpi.collective.barrier = (ulm_barrier_t *) func;
        break;
    case ULM_COLLECTIVE_BCAST:
        _mpi.collective.bcast = (ulm_bcast_t *) func;
        break;
    case ULM_COLLECTIVE_GATHER:
        _mpi.collective.gather = (ulm_gather_t *) func;
        break;
    case ULM_COLLECTIVE_GATHERV:
        _mpi.collective.gatherv = (ulm_gatherv_t *) func;
        break;
    case ULM_COLLECTIVE_REDUCE:
        _mpi.collective.reduce = (ulm_reduce_t *) func;
        break;
    case ULM_COLLECTIVE_REDUCE_SCATTER:
        _mpi.collective.reduce_scatter = (ulm_reduce_scatter_t *) func;
        break;
    case ULM_COLLECTIVE_SCAN:
        _mpi.collective.scan = (ulm_scan_t *) func;
        break;
    case ULM_COLLECTIVE_SCATTER:
        _mpi.collective.scatter = (ulm_scatter_t *) func;
        break;
    case ULM_COLLECTIVE_SCATTERV:
        _mpi.collective.scatterv = (ulm_scatterv_t *) func;
        break;
    default:
        return ULM_ERR_BAD_PARAM;
    }

    return ULM_SUCCESS;
}
