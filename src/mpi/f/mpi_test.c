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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "internal/mpif.h"

void mpi_test_f(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    MPI_Request c_req = MPI_Request_f2c(*req);

    *rc = MPI_Test(&c_req, flag, status);
    if (*rc == MPI_SUCCESS && (*req >= 0) && *flag
        && status->_persistent == 0) {
        _mpi_ptr_table_free(_mpif.request_table, *req);
        /* set request to MPI_REQUEST_NULL */
        *req = -1;
    }
}

#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_TEST = mpi_test_f
#pragma weak pmpi_test = mpi_test_f
#pragma weak pmpi_test_ = mpi_test_f
#pragma weak pmpi_test__ = mpi_test_f

#pragma weak MPI_TEST = mpi_test_f
#pragma weak mpi_test = mpi_test_f
#pragma weak mpi_test_ = mpi_test_f
#pragma weak mpi_test__ = mpi_test_f

#else

void PMPI_TEST(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    mpi_test_f(req, flag, status, rc);
}

void pmpi_test(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    mpi_test_f(req, flag, status, rc);
}

void pmpi_test_(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    mpi_test_f(req, flag, status, rc);
}

void pmpi_test__(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    mpi_test_f(req, flag, status, rc);
}

void MPI_TEST(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    mpi_test_f(req, flag, status, rc);
}

void mpi_test(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    mpi_test_f(req, flag, status, rc);
}

void mpi_test_(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    mpi_test_f(req, flag, status, rc);
}

void mpi_test__(MPI_Fint *req, MPI_Fint *flag,
                MPI_Status *status, MPI_Fint *rc)
{
    mpi_test_f(req, flag, status, rc);
}

#endif
