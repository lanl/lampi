/*
 * Copyright 2002-2003.  The Regents of the University of
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

#include "path/ib/init.h"
#include "path/ib/state.h"
#include "internal/malloc.h"

void ibSetup(lampiState_t *s)
{
    VAPI_ret_t vapi_result;
    int i;

    // get the number of available InfiniBand HCAs
    ib_state.num_hcas = 0;
    vapi_result = EVAPI_list_hcas(0, &ib_state.num_hcas, (VAPI_hca_id_t *)NULL);
    
    // no available HCAs...active_port is zero, exchange info now
    if (!ib_state.num_hcas) {
        goto exchange_info;
    }

    // allocate array large enough to hold ids
    ib_state.hca_ids = (VAPI_hca_id_t *)ulm_malloc(sizeof(VAPI_hca_id_t) * ib_state.num_hcas);
    
    // get ids
    vapi_result = EVAPI_list_hcas(
        sizeof(VAPI_hca_id_t) * ib_state.num_hcas,
        &ib_state.num_hcas,
        ib_state.hca_ids);
    if (vapi_result != VAPI_OK) {
        ulm_err(("ibSetup: EVAPI_list_hcas() returned %s\n", 
            VAPI_strerror(vapi_result)));
        exit(1);
    }

    // allocate array large enough to hold all of the handles
    ib_state.hca_handles = (VAPI_hca_hndl_t *)ulm_malloc(sizeof(VAPI_hca_hndl_t) * ib_state.num_hcas);

    // get handles
    for (i = 0; i < (int)ib_state.num_hcas; i++) {
        vapi_result = EVAPI_get_hca_hndl(ib_state.hca_ids[i], &ib_state.hca_handles[i]);
        if (vapi_result != VAPI_OK) {
            ulm_err(("ibSetup: EVAPI_get_hca_hndl() for HCA %d returned %s\n",
                i, VAPI_strerror(vapi_result)));
            exit(1);
        }
    }

exchange_info:

    return;
}

