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

#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_TYPE_CREATE_DARRAY = mpi_type_create_darray_f
#pragma weak pmpi_type_create_darray = mpi_type_create_darray_f
#pragma weak pmpi_type_create_darray_ = mpi_type_create_darray_f
#pragma weak pmpi_type_create_darray__ = mpi_type_create_darray_f

#pragma weak MPI_TYPE_CREATE_DARRAY = mpi_type_create_darray_f
#pragma weak mpi_type_create_darray = mpi_type_create_darray_f
#pragma weak mpi_type_create_darray_ = mpi_type_create_darray_f
#pragma weak mpi_type_create_darray__ = mpi_type_create_darray_f

void mpi_type_create_darray_f(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

#else

void PMPI_TYPE_CREATE_DARRAY(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

void pmpi_type_create_darray(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

void pmpi_type_create_darray_(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

void pmpi_type_create_darray__(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

void MPI_TYPE_CREATE_DARRAY(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

void mpi_type_create_darray(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

void mpi_type_create_darray_(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

void mpi_type_create_darray__(MPI_Fint *size, MPI_Fint *rank,
                              MPI_Fint *ndims,
                              MPI_Fint *gsize_array,
                              MPI_Fint *distrib_array,
                              MPI_Fint *darg_array,
                              MPI_Fint *psize_array,
                              MPI_Fint *order,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_create_darray(*size, *rank, *ndims, gsize_array,
                                 distrib_array, darg_array,
                                 psize_array, *order,
                                 c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

#endif
