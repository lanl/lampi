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

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/state.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "queue/globals.h"
#include "os/IRIX/acquire.h"

// structure that holds available resrouces
request *ULMreq;
// structure of allocated resources
acquire *ULMai;

//
//  This routine is used for determining which processors will be used.
//
int getCPUSet ()
{
    constraint_info ci;
    int NChildren = lampiState.map_host_to_local_size[lampiState.hostid];

    // allocate request structure
    ULMreq = ulm_new(request, 1);
    if( !ULMreq ) {
	fprintf(stderr, " Unable to allocate memory for ULMreq\n");
	ulm_exit((-1, "Aborting \n"));
    }

    // add constraints - no op
    ULMreq->add_constraint(R_CPU, NChildren);
    // one node per host
    if( nCpPNode == 1 ) {
	ci.set_type(R_CPU);
	ci.set_num(1);
	ci.reset_mask(C_CPU_PER_NODE);
	ULMreq->add_constraint(R_CPU, 1, &ci);
    }

    // obtain all resources - at this stage hippi info is also obtained,
    //   but need to change this so that only cpu data is obtained
    ULMreq->map_resources();

    // assign cpu resources
    ULMai = new acquire(NChildren, ULMreq->get_num_resource(R_CPU),
			ULMreq->get_resource(R_CPU));
    if( !ULMai ) {
	fprintf(stderr, " Unable to allocate ULMai data structure\n");
	fflush(stderr);
	ulm_exit((-1, "Aborting \n"));
    }

    // allows for scheduling hints
    if (!(ULMai->init_parent())) {
	fprintf(stderr, " init_parent call failed\n");
	fflush(stderr);
	ulm_exit((-1, "Aborting \n"));
    }

    return ULM_SUCCESS;
}
