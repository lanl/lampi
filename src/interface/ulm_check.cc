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
 * Some argument checking functions
 */

#include "ulm/ulm.h"
#include "queue/globals.h"


extern "C" int ulm_invalid_comm(int comm)
{
    if (comm < ULM_COMM_WORLD || communicators[comm] == NULL) {
        return 1;
    }

    return 0;
}


extern "C" int ulm_invalid_request(ULMRequest_t *request)
{
    RequestDesc_t *r = (RequestDesc_t *) (*request);

    if (*request == NULL) {
        /* this is valid ! */
        return 0;
    } else if (*request == _mpi.proc_null_request) {
        return 0;
    } else if (r->requestType != REQUEST_TYPE_RECV &&
               r->requestType != REQUEST_TYPE_SEND) {
        return 1;
    } else {
        return 0;
    }
}

extern "C" int ulm_invalid_source(int comm, int rank)
{
    Group *g = communicators[comm]->remoteGroup;

    if ( (rank < MPI_ROOT || rank >= g->groupSize) && rank !=MPI_PROC_NULL ) {
        return 1;
    } else {
        return 0;
    }
}


extern "C" int ulm_invalid_destination(int comm, int rank)
{
    Group *g = communicators[comm]->remoteGroup;

    if ( (rank < 0 || rank >= g->groupSize) && rank !=MPI_PROC_NULL) {
        return 1;
    } else {
        return 0;
    }
}

extern "C" int ulm_presistant_request(ULMRequest_t *request)
{
    RequestDesc_t *r = (RequestDesc_t *) (*request);

    if (*request == NULL) {
        /* this is valid ! */
        return 0;
    } else if (r->persistent ) {
        return 1;
    } else {
        return 0;
    }
}

extern "C" int ulm_am_i(int comm, int rank)
{
    if (communicators[comm]->localGroup->ProcID == rank) {
        return 1;
    }
    else {
        return 0;
    }
}
