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

#include <vapi_common.h>
#include "path/ib/state.h"

ib_state_t ib_state;

// chunk should be page-aligned or else...ZeroAlloc() should take care of this...
bool ib_register_chunk(int hca_index, void *addr, size_t size)
{
    bool returnValue = true;
    VAPI_ret_t vapi_result;
    VAPI_mr_t mr;

    // we need to stash this info for lkey retrieval, etc.
    VAPI_mr_hndl_t tmp_mr_handle;
    VAPI_mr_t tmp_mr;

    ib_hca_state_t *h = &(ib_state.hca[hca_index]);

    mr.type = VAPI_MR;
    mr.start = (VAPI_virt_addr_t)((unsigned long)addr);
    mr.size = size;
    mr.pd_hndl = h->pd;
    mr.acl = VAPI_EN_LOCAL_WRITE | VAPI_EN_REMOTE_WRITE;

    vapi_result = VAPI_register_mr(h->handle, &mr, &(tmp_mr_handle), 
        &(tmp_mr));
    if (vapi_result != VAPI_OK) {
        ulm_err(("ib_register_chunk: VAPI_register_mr() for HCA %d (addr %p size %ld) returned %s\n",
            hca_index, addr, (long)size, VAPI_strerror(vapi_result)));
        returnValue = false;
    }
    return returnValue;
}
