/*
 * Copyright 2002-2005. The Regents of the University of
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

#include "internal/malloc.h"
#include "internal/mpif.h"

void mpi_type_create_hindexed_f(MPI_Fint *count,
                                MPI_Fint *blklen_array,
                                MPI_Aint *disp_array,
                                MPI_Fint *f_type_old,
                                MPI_Fint *f_type_new,
                                MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;
    ULMType_t *type;
    int i;

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_hindexed(*count, blklen_array, disp_array,
                            c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        /* save true combiner value */
        type = c_type_new;
        type->envelope.combiner = MPI_COMBINER_HINDEXED_INTEGER;
        /* save pointer to index translation */
        *f_type_new = MPI_Type_c2f(c_type_new);
    }
}

void mpi_type_hindexed_f(MPI_Fint *count,
                         MPI_Fint *blklen_array,
                         MPI_Fint *f_disp_array,
                         MPI_Fint *f_type_old,
                         MPI_Fint *f_type_new, MPI_Fint *rc)
{
    MPI_Datatype c_type_old;
    MPI_Datatype c_type_new;
    MPI_Aint *c_disp_array;
    ULMType_t *type;
    int i;

    c_disp_array = ulm_malloc(*count * sizeof(MPI_Aint));
    if (c_disp_array == (MPI_Aint *) NULL) {
        *rc = MPI_ERR_INTERN;
        return;
    }

    for (i = 0; i < *count; i++) {
        c_disp_array[i] = (MPI_Aint) f_disp_array[i];
    }

    c_type_old = MPI_Type_f2c(*f_type_old);

    *rc = MPI_Type_hindexed(*count, blklen_array,
                            c_disp_array, c_type_old, &c_type_new);

    if (*rc == MPI_SUCCESS) {
        /* save true combiner value */
        type = c_type_new;
        type->envelope.combiner = MPI_COMBINER_HINDEXED_INTEGER;
        /* save pointer to index translation */
        *f_type_new = MPI_Type_c2f(c_type_new);
    }

    ulm_free(c_disp_array);
}

#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_TYPE_CREATE_HINDEXED = mpi_type_create_hindexed_f
#pragma weak pmpi_type_create_hindexed = mpi_type_create_hindexed_f
#pragma weak pmpi_type_create_hindexed_ = mpi_type_create_hindexed_f
#pragma weak pmpi_type_create_hindexed__ = mpi_type_create_hindexed_f

#pragma weak MPI_TYPE_CREATE_HINDEXED = mpi_type_create_hindexed_f
#pragma weak mpi_type_create_hindexed = mpi_type_create_hindexed_f
#pragma weak mpi_type_create_hindexed_ = mpi_type_create_hindexed_f
#pragma weak mpi_type_create_hindexed__ = mpi_type_create_hindexed_f

#pragma weak PMPI_TYPE_HINDEXED = mpi_type_hindexed_f
#pragma weak pmpi_type_hindexed = mpi_type_hindexed_f
#pragma weak pmpi_type_hindexed_ = mpi_type_hindexed_f
#pragma weak pmpi_type_hindexed__ = mpi_type_hindexed_f

#pragma weak MPI_TYPE_HINDEXED = mpi_type_hindexed_f
#pragma weak mpi_type_hindexed = mpi_type_hindexed_f
#pragma weak mpi_type_hindexed_ = mpi_type_hindexed_f
#pragma weak mpi_type_hindexed__ = mpi_type_hindexed_f

#else

void PMPI_TYPE_HINDEXED(MPI_Fint *count,
                        MPI_Fint *blklen_array,
                        MPI_Fint *f_disp_array,
                        MPI_Fint *f_type_old,
                        MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_hindexed_f(count, blklen_array, f_disp_array,
                        f_type_old, f_type_new, rc);
}

void pmpi_type_hindexed(MPI_Fint *count,
                        MPI_Fint *blklen_array,
                        MPI_Fint *f_disp_array,
                        MPI_Fint *f_type_old,
                        MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_hindexed_f(count, blklen_array, f_disp_array,
                        f_type_old, f_type_new, rc);
}

void pmpi_type_hindexed_(MPI_Fint *count,
                         MPI_Fint *blklen_array,
                         MPI_Fint *f_disp_array,
                         MPI_Fint *f_type_old,
                         MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_hindexed_f(count, blklen_array, f_disp_array,
                        f_type_old, f_type_new, rc);
}

void pmpi_type_hindexed__(MPI_Fint *count,
                          MPI_Fint *blklen_array,
                          MPI_Fint *f_disp_array,
                          MPI_Fint *f_type_old,
                          MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_hindexed_f(count, blklen_array, f_disp_array,
                        f_type_old, f_type_new, rc);
}

void MPI_TYPE_HINDEXED(MPI_Fint *count,
                       MPI_Fint *blklen_array,
                       MPI_Fint *f_disp_array,
                       MPI_Fint *f_type_old,
                       MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_hindexed_f(count, blklen_array, f_disp_array,
                        f_type_old, f_type_new, rc);
}

void mpi_type_hindexed(MPI_Fint *count,
                       MPI_Fint *blklen_array,
                       MPI_Fint *f_disp_array,
                       MPI_Fint *f_type_old,
                       MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_hindexed_f(count, blklen_array, f_disp_array,
                        f_type_old, f_type_new, rc);
}

void mpi_type_hindexed_(MPI_Fint *count,
                        MPI_Fint *blklen_array,
                        MPI_Fint *f_disp_array,
                        MPI_Fint *f_type_old,
                        MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_hindexed_f(count, blklen_array, f_disp_array,
                        f_type_old, f_type_new, rc);
}

void mpi_type_hindexed__(MPI_Fint *count,
                         MPI_Fint *blklen_array,
                         MPI_Fint *f_disp_array,
                         MPI_Fint *f_type_old,
                         MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_hindexed_f(count, blklen_array, f_disp_array,
                        f_type_old, f_type_new, rc);
}

void PMPI_TYPE_CREATE_HINDEXED(MPI_Fint *count,
                               MPI_Fint *blklen_array,
                               MPI_Aint *disp_array,
                               MPI_Fint *f_type_old,
                               MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_create_hindexed_f(count, blklen_array, f_disp_array,
                               f_type_old, f_type_new, rc);
}

void pmpi_type_create_hindexed(MPI_Fint *count,
                               MPI_Fint *blklen_array,
                               MPI_Aint *disp_array,
                               MPI_Fint *f_type_old,
                               MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_create_hindexed_f(count, blklen_array, f_disp_array,
                               f_type_old, f_type_new, rc);
}

void pmpi_type_create_hindexed_(MPI_Fint *count,
                                MPI_Fint *blklen_array,
                                MPI_Aint *disp_array,
                                MPI_Fint *f_type_old,
                                MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_create_hindexed_f(count, blklen_array, f_disp_array,
                               f_type_old, f_type_new, rc);
}

void pmpi_type_create_hindexed__(MPI_Fint *count,
                                 MPI_Fint *blklen_array,
                                 MPI_Aint *disp_array,
                                 MPI_Fint *f_type_old,
                                 MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_create_hindexed_f(count, blklen_array, f_disp_array,
                               f_type_old, f_type_new, rc);
}

void MPI_TYPE_CREATE_HINDEXED(MPI_Fint *count,
                              MPI_Fint *blklen_array,
                              MPI_Aint *disp_array,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_create_hindexed_f(count, blklen_array, f_disp_array,
                               f_type_old, f_type_new, rc);
}

void mpi_type_create_hindexed(MPI_Fint *count,
                              MPI_Fint *blklen_array,
                              MPI_Aint *disp_array,
                              MPI_Fint *f_type_old,
                              MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_create_hindexed_f(count, blklen_array, f_disp_array,
                               f_type_old, f_type_new, rc);
}

void mpi_type_create_hindexed_(MPI_Fint *count,
                               MPI_Fint *blklen_array,
                               MPI_Aint *disp_array,
                               MPI_Fint *f_type_old,
                               MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_create_hindexed_f(count, blklen_array, f_disp_array,
                               f_type_old, f_type_new, rc);
}

void mpi_type_create_hindexed__(MPI_Fint *count,
                                MPI_Fint *blklen_array,
                                MPI_Aint *disp_array,
                                MPI_Fint *f_type_old,
                                MPI_Fint *f_type_new, MPI_Fint *rc)
{
    mpi_type_create_hindexed_f(count, blklen_array, f_disp_array,
                               f_type_old, f_type_new, rc);
}

#endif
