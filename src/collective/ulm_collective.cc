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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "internal/mpi.h"
#include "queue/Communicator.h"
#include "queue/globals.h"
#include "internal/collective.h"

int ulm_comm_get_collective(MPI_Comm comm, int key, void **func)
{
    Communicator    *communicator = communicators[comm];

    switch (key) {
    case ULM_COLLECTIVE_ALLGATHER:
        *func = (void *) communicator->collective.allgather;
        break;
    case ULM_COLLECTIVE_ALLGATHERV:
        *func = (void *) communicator->collective.allgatherv;
        break;
    case ULM_COLLECTIVE_ALLREDUCE:
        *func = (void *) communicator->collective.allreduce;
        break;
    case ULM_COLLECTIVE_ALLTOALL:
        *func = (void *) communicator->collective.alltoall;
        break;
    case ULM_COLLECTIVE_ALLTOALLV:
        *func = (void *) communicator->collective.alltoallv;
        break;
    case ULM_COLLECTIVE_ALLTOALLW:
        *func = (void *) communicator->collective.alltoallw;
        break;
    case ULM_COLLECTIVE_BARRIER:
        *func = (void *) communicator->collective.barrier;
        break;
    case ULM_COLLECTIVE_BCAST:
        *func = (void *) communicator->collective.bcast;
        break;
    case ULM_COLLECTIVE_GATHER:
        *func = (void *) communicator->collective.gather;
        break;
    case ULM_COLLECTIVE_GATHERV:
        *func = (void *) communicator->collective.gatherv;
        break;
    case ULM_COLLECTIVE_REDUCE:
        *func = (void *) communicator->collective.reduce;
        break;
    case ULM_COLLECTIVE_REDUCE_SCATTER:
        *func = (void *) communicator->collective.reduce_scatter;
        break;
    case ULM_COLLECTIVE_SCAN:
        *func = (void *) communicator->collective.scan;
        break;
    case ULM_COLLECTIVE_SCATTER:
        *func = (void *) communicator->collective.scatter;
        break;
    case ULM_COLLECTIVE_SCATTERV:
        *func = (void *) communicator->collective.scatterv;
        break;
    default:
        return ULM_ERR_BAD_PARAM;
    }

    return ULM_SUCCESS;
}

