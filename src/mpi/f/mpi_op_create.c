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

#include "internal/mpif.h"

void mpi_op_create_f(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    MPI_Op c_op;

    *rc = MPI_Op_create(func, *commute, &c_op);

    if (*rc == MPI_SUCCESS) {
        ULMOp_t *op = c_op;

        op->fortran = 1;
        op->fhandle = _mpi_ptr_table_add(_mpif.op_table, c_op);
        *f_op = op->fhandle;
    }
}

#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_OP_CREATE = mpi_op_create_f
#pragma weak pmpi_op_create = mpi_op_create_f
#pragma weak pmpi_op_create_ = mpi_op_create_f
#pragma weak pmpi_op_create__ = mpi_op_create_f

#pragma weak MPI_OP_CREATE = mpi_op_create_f
#pragma weak mpi_op_create = mpi_op_create_f
#pragma weak mpi_op_create_ = mpi_op_create_f
#pragma weak mpi_op_create__ = mpi_op_create_f

#else

void PMPI_OP_CREATE(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    mpi_op_create_f(func, commute, f_op, rc);
}

void pmpi_op_create(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    mpi_op_create_f(func, commute, f_op, rc);
}

void pmpi_op_create_(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    mpi_op_create_f(func, commute, f_op, rc);
}

void pmpi_op_create__(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    mpi_op_create_f(func, commute, f_op, rc);
}

void MPI_OP_CREATE(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    mpi_op_create_f(func, commute, f_op, rc);
}

void mpi_op_create(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    mpi_op_create_f(func, commute, f_op, rc);
}

void mpi_op_create_(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    mpi_op_create_f(func, commute, f_op, rc);
}

void mpi_op_create__(MPI_User_function *func,
                     MPI_Fint *commute,
                     MPI_Fint *f_op,
                     MPI_Fint *rc)
{
    mpi_op_create_f(func, commute, f_op, rc);
}

#endif
