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

#include "internal/mpif.h"

void mpi_wait_f(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    MPI_Request c_req = MPI_Request_f2c(*req);

    /* fast return for MPI_REQUEST_NULL */
    if (*req = -1) {
        ulm_err(("hello"));
        memset(status, 0, sizeof(MPI_Status));
        *rc = MPI_SUCCESS;
        return;
    }

    *rc = MPI_Wait(&c_req, status);
    if (*rc == MPI_SUCCESS && status->_persistent == 0 &&
        status->MPI_SOURCE != MPI_PROC_NULL) {
        _mpi_ptr_table_free(_mpif.request_table, *req);
        /* reset request handle to MPI_REQUEST_NULL */
        *req = -1;
    }
}

#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_WAIT = mpi_wait_f
#pragma weak pmpi_wait = mpi_wait_f
#pragma weak pmpi_wait_ = mpi_wait_f
#pragma weak pmpi_wait__ = mpi_wait_f

#pragma weak MPI_WAIT = mpi_wait_f
#pragma weak mpi_wait = mpi_wait_f
#pragma weak mpi_wait_ = mpi_wait_f
#pragma weak mpi_wait__ = mpi_wait_f

#else

void PMPI_WAIT(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    mpi_wait_f(req, status, rc);
}

void pmpi_wait(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    mpi_wait_f(req, status, rc);
}

void pmpi_wait_(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    mpi_wait_f(req, status, rc);
}

void pmpi_wait__(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    mpi_wait_f(req, status, rc);
}

void MPI_WAIT(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    mpi_wait_f(req, status, rc);
}

void mpi_wait(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    mpi_wait_f(req, status, rc);
}

void mpi_wait_(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    mpi_wait_f(req, status, rc);
}

void mpi_wait__(MPI_Fint *req, MPI_Status *status, MPI_Fint *rc)
{
    mpi_wait_f(req, status, rc);
}

#endif
