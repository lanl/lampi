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



#include "internal/malloc.h"
#include "internal/mpif.h"

#pragma weak PMPI_TESTANY = mpi_testany_f
#pragma weak pmpi_testany = mpi_testany_f
#pragma weak pmpi_testany_ = mpi_testany_f
#pragma weak pmpi_testany__ = mpi_testany_f

#pragma weak MPI_TESTANY = mpi_testany_f
#pragma weak mpi_testany = mpi_testany_f
#pragma weak mpi_testany_ = mpi_testany_f
#pragma weak mpi_testany__ = mpi_testany_f

void mpi_testany_f(MPI_Fint *count, MPI_Fint *request_array,
                   MPI_Fint *index, MPI_Fint *flag,
                   MPI_Status *status, MPI_Fint *rc)
{
    MPI_Request *c_req;
    int i;

    c_req = ulm_malloc(*count * sizeof(MPI_Request));
    if (c_req == NULL) {
        *rc = MPI_ERR_INTERN;
        _mpi_errhandler(MPI_COMM_WORLD, *rc, __FILE__, __LINE__);
        return;
    }

    for (i = 0; i < *count; i++) {
        c_req[i] = MPI_Request_f2c(request_array[i]);
    }

    *rc = MPI_Testany(*count, c_req, index, flag, status);

    if (*rc == MPI_SUCCESS) {
        /*
         * 1. Remove request table entry for a non-persistent request
         * 2. Increment index by one for fortran conventions
         */
        if (*index != MPI_UNDEFINED) {
            if (status->_persistent == 0) {
                _mpi_ptr_table_free(_mpif.request_table,
                                    request_array[*index]);
                /* set to MPI_REQUEST_NULL */
                request_array[*index] = -1;
            }
            *index += 1;
        }
    }
    /* end if (*rc == MPI_SUCCESS) */
    if (c_req) {
        ulm_free(c_req);
    }
}
