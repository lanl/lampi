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

#include "mpi/constants.h"

/*
 * default attribute functions
 */

/*
 * C
 */

int mpi_dup_fn_c(int oldcomm,
                 int keyval,
                 void *extra_state,
                 void *attribute_val_in,
                 void *attribute_val_out,
                 int *flag)
{
    int *tmp_attribute_val_in = (int *) attribute_val_in;
    int **tmp_attribute_val_out = (int **) attribute_val_out;
    *flag = 1;
    *tmp_attribute_val_out = tmp_attribute_val_in;

    return MPI_SUCCESS;
}


int mpi_null_copy_fn_c(int oldcomm,
                       int keyval,
                       void *extra_state,
                       void *attribute_val_in,
                       void *attribute_val_out,
                       int *flag)
{
    *flag = 0;

    return MPI_SUCCESS;
}


int mpi_null_delete_fn_c(int oldcomm,
                         int keyval,
                         void *attribute_val,
			 void *extra_state)
{
    return MPI_SUCCESS;
}


/*
 * fortran
 */

#include "internal/mpif.h"

void mpi_dup_fn_f(MPI_Comm *oldcomm,
                  MPI_Fint *keyval,
                  MPI_Fint *extra_state,
                  MPI_Fint *attribute_val_in,
                  MPI_Fint *attribute_val_out,
                  MPI_Fint *flag,
                  int *error)
{
    *error = MPI_SUCCESS;
    *flag = 1;
    *attribute_val_out = *attribute_val_in;
}


void mpi_null_copy_fn_f(MPI_Fint *oldcomm,
                        MPI_Fint *keyval,
                        MPI_Fint *extra_state,
                        MPI_Fint *attribute_val_in,
                        MPI_Fint *attribute_val_out,
                        MPI_Fint *flag,
                        MPI_Fint *error)
{
    *error = MPI_SUCCESS;
    *flag = 0;
}


void mpi_null_delete_fn_f(int *oldcomm,
                          int *keyval,
                          int *attribute_all,
                          int *extra_state,
                          int *error)
{
    *error = MPI_SUCCESS;
}


#if defined(HAVE_PRAGMA_WEAK)

#pragma weak MPI_DUP_FN = mpi_dup_fn_f
#pragma weak mpi_dup_fn = mpi_dup_fn_f
#pragma weak mpi_dup_fn_ = mpi_dup_fn_f
#pragma weak mpi_dup_fn__ = mpi_dup_fn_f

#pragma weak MPI_NULL_COPY_FN = mpi_null_copy_fn_f
#pragma weak mpi_null_copy_fn = mpi_null_copy_fn_f
#pragma weak mpi_null_copy_fn_ = mpi_null_copy_fn_f
#pragma weak mpi_null_copy_fn__ = mpi_null_copy_fn_f

#pragma weak MPI_NULL_DELETE_FN = mpi_null_delete_fn_f
#pragma weak mpi_null_delete_fn = mpi_null_delete_fn_f
#pragma weak mpi_null_delete_fn_ = mpi_null_delete_fn_f
#pragma weak mpi_null_delete_fn__ = mpi_null_delete_fn_f

#else

void MPI_DUP_FN(MPI_Comm *oldcomm, MPI_Fint *keyval, MPI_Fint *extra_state,
                  MPI_Fint *attribute_val_in, MPI_Fint *attribute_val_out,
                  MPI_Fint *flag, int *error)
{
    mpi_dup_fn_f(oldcomm, keyval, extra_state, attribute_val_in, attribute_val_out,
                 flag, error);
}

void mpi_dup_fn(MPI_Comm *oldcomm, MPI_Fint *keyval, MPI_Fint *extra_state,
                  MPI_Fint *attribute_val_in, MPI_Fint *attribute_val_out,
                  MPI_Fint *flag, int *error)
{
    mpi_dup_fn_f(oldcomm, keyval, extra_state, attribute_val_in, attribute_val_out,
                 flag, error);
}

void mpi_dup_fn_(MPI_Comm *oldcomm, MPI_Fint *keyval, MPI_Fint *extra_state,
                  MPI_Fint *attribute_val_in, MPI_Fint *attribute_val_out,
                  MPI_Fint *flag, int *error)
{
    mpi_dup_fn_f(oldcomm, keyval, extra_state, attribute_val_in, attribute_val_out,
                 flag, error);
}

void mpi_dup_fn__(MPI_Comm *oldcomm, MPI_Fint *keyval, MPI_Fint *extra_state,
                  MPI_Fint *attribute_val_in, MPI_Fint *attribute_val_out,
                  MPI_Fint *flag, int *error)
{
    mpi_dup_fn_f(oldcomm, keyval, extra_state, attribute_val_in, attribute_val_out,
                 flag, error);
}


void MPI_NULL_COPY_FN(MPI_Fint *oldcomm, MPI_Fint *keyval, MPI_Fint *extra_state,
                        MPI_Fint *attribute_val_in, MPI_Fint *attribute_val_out,
                        MPI_Fint *flag, MPI_Fint *error)
{
    mpi_null_copy_fn_f(oldcomm, keyval, extra_state, attribute_val_in, attribute_val_out,
                       flag, error);
}

void mpi_null_copy_fn(MPI_Fint *oldcomm, MPI_Fint *keyval, MPI_Fint *extra_state,
                      MPI_Fint *attribute_val_in, MPI_Fint *attribute_val_out,
                      MPI_Fint *flag, MPI_Fint *error)
{
    mpi_null_copy_fn_f(oldcomm, keyval, extra_state, attribute_val_in, attribute_val_out,
                       flag, error);
}

void mpi_null_copy_fn_(MPI_Fint *oldcomm, MPI_Fint *keyval, MPI_Fint *extra_state,
                        MPI_Fint *attribute_val_in, MPI_Fint *attribute_val_out,
                        MPI_Fint *flag, MPI_Fint *error)
{
    mpi_null_copy_fn_f(oldcomm, keyval, extra_state, attribute_val_in, attribute_val_out,
                       flag, error);
}

void mpi_null_copy_fn__(MPI_Fint *oldcomm, MPI_Fint *keyval, MPI_Fint *extra_state,
                        MPI_Fint *attribute_val_in, MPI_Fint *attribute_val_out,
                        MPI_Fint *flag, MPI_Fint *error)
{
    mpi_null_copy_fn_f(oldcomm, keyval, extra_state, attribute_val_in, attribute_val_out,
                       flag, error);
}


void MPI_NULL_DELETE_FN(int *oldcomm, int *keyval, int *attribute_all, int *extra_state,
                          int *error)
{
    mpi_null_delete_fn_f(oldcomm, keyval, attribute_all, extra_state, error);
}

void mpi_null_delete_fn(int *oldcomm, int *keyval, int *attribute_all, int *extra_state,
                        int *error)
{
    mpi_null_delete_fn_f(oldcomm, keyval, attribute_all, extra_state, error);
}

void mpi_null_delete_fn_(int *oldcomm, int *keyval, int *attribute_all, int *extra_state,
                         int *error)
{
    mpi_null_delete_fn_f(oldcomm, keyval, attribute_all, extra_state, error);
}

void mpi_null_delete_fn__(int *oldcomm, int *keyval, int *attribute_all, int *extra_state,
                          int *error)
{
    mpi_null_delete_fn_f(oldcomm, keyval, attribute_all, extra_state, error);
}


#endif		/* HAVE_PRAGMA_WEAK */
