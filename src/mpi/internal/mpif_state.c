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

/*
 * MPI Fortran: state initialization
 */

#include "internal/mpif.h"

mpif_state_t _mpif;
static int called = 0;

int _mpif_init(void)
{
    if (_MPI_DEBUG) {
        _mpi_dbg("_mpif_init:\n");
    }

    /*
     * initialize _mpif lock - upper layer is responsible to ensure that
     * only one thread at a time makes a call
     */
    if (!called) {
        called = 1;
        ATOMIC_LOCK_INIT(_mpif.lock);
    }

    ATOMIC_LOCK(_mpif.lock);

    if (_mpif.initialized == 0) {

        if (_MPI_DEBUG) {
            _mpi_dbg("_mpif_init: initializing\n");
        }

        _mpif.initialized = 1;

        _mpif.op_table = _mpif_create_op_table();
        if (_mpif.op_table == NULL) {
            return -1;
        }

        _mpif.request_table = _mpif_create_request_table();
        if (_mpif.request_table == NULL) {
            return -1;
        }

        _mpif.type_table = _mpif_create_type_table();
        if (_mpif.type_table == NULL) {
            return -1;
        }
    }

    ATOMIC_UNLOCK(_mpif.lock);

    return 0;
}


/*
 * MPI fortran layer clean-up.  By this point, everything is complete,
 * so unconditionally free everything we can.
 */
int _mpif_finalize(void)
{
    int i;
    int rc;

    rc = ulm_barrier(ULM_COMM_WORLD);
    if (rc != ULM_SUCCESS) {
        return _mpi_error(rc);
    }

    ATOMIC_LOCK(_mpif.lock);
    if (_mpif.finalized == 0) {

        _mpif.finalized = 1;

        /* empty the fortran-layer tables */
        
        for (i = 0; i < _mpif.op_table->size; i++) {
            if (_mpif.op_table->addr[i]) {
                MPI_Op_free((MPI_Op *) &(_mpif.op_table->addr[i]));
            }
        }

        for (i = 0; i < _mpif.request_table->size; i++) {
            if (_mpif.request_table->addr[i]) {
                MPI_Request_free((MPI_Request *) &(_mpif.request_table->addr[i]));
            }
        }

        for (i = 0; i < _mpif.type_table->size; i++) {
            if (_mpif.type_table->addr[i]) {
                MPI_Type_free((MPI_Datatype *) &(_mpif.type_table->addr[i]));
            }
        }

        /* free the fortran-layer tables */

        if (_mpif.op_table->addr) {
            ulm_free(_mpif.op_table->addr);
        }
        ulm_free(_mpif.op_table);
        if (_mpif.request_table->addr) {
            ulm_free(_mpif.request_table->addr);
        }
        ulm_free(_mpif.request_table);
        if (_mpif.type_table->addr) {
            ulm_free(_mpif.type_table->addr);
        }
        ulm_free(_mpif.type_table);
    }
    ATOMIC_UNLOCK(_mpif.lock);

    return MPI_SUCCESS;
}
