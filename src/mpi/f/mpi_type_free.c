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

#include "internal/mpif.h"

void mpi_type_free_f(MPI_Fint *f_type, MPI_Fint *rc)
{
    MPI_Datatype c_type;

    c_type = MPI_Type_f2c(*f_type);
    *rc = MPI_Type_free(&c_type);
    if (*rc == MPI_SUCCESS) {
        /*
         * if the datatype is not MPI_DATATYPE_NULL, then free it, and
         * reset the handle
         */
        if (!c_type && (*f_type != -1)) {
            _mpi_ptr_table_free(_mpif.type_table, *f_type);
        }
        /*
         * reset the handle even if we didn't really free the datatype -
         * like if it is a basic datatype....
         */
        *f_type = -1;
    }
}

#if defined(HAVE_PRAGMA_WEAK)

#pragma weak PMPI_TYPE_FREE = mpi_type_free_f
#pragma weak pmpi_type_free = mpi_type_free_f
#pragma weak pmpi_type_free_ = mpi_type_free_f
#pragma weak pmpi_type_free__ = mpi_type_free_f

#pragma weak MPI_TYPE_FREE = mpi_type_free_f
#pragma weak mpi_type_free = mpi_type_free_f
#pragma weak mpi_type_free_ = mpi_type_free_f
#pragma weak mpi_type_free__ = mpi_type_free_f

#else

void PMPI_TYPE_FREE(MPI_Fint *f_type, MPI_Fint *rc)
{
    mpi_type_free_f(f_type, rc);
}

void pmpi_type_free(MPI_Fint *f_type, MPI_Fint *rc)
{
    mpi_type_free_f(f_type, rc);
}

void pmpi_type_free_(MPI_Fint *f_type, MPI_Fint *rc)
{
    mpi_type_free_f(f_type, rc);
}

void pmpi_type_free__(MPI_Fint *f_type, MPI_Fint *rc)
{
    mpi_type_free_f(f_type, rc);
}

void MPI_TYPE_FREE(MPI_Fint *f_type, MPI_Fint *rc)
{
    mpi_type_free_f(f_type, rc);
}

void mpi_type_free(MPI_Fint *f_type, MPI_Fint *rc)
{
    mpi_type_free_f(f_type, rc);
}

void mpi_type_free_(MPI_Fint *f_type, MPI_Fint *rc)
{
    mpi_type_free_f(f_type, rc);
}

void mpi_type_free__(MPI_Fint *f_type, MPI_Fint *rc)
{
    mpi_type_free_f(f_type, rc);
}

#endif
