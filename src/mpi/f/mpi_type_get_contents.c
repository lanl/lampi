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

#include "internal/malloc.h"
#include "internal/mpif.h"

void mpi_type_get_contents_f(MPI_Fint *datatype,
                             MPI_Fint *max_integers,
                             MPI_Fint *max_addresses,
                             MPI_Fint *max_datatypes,
                             MPI_Fint *array_of_integers,
                             MPI_Fint *array_of_addresses,
                             MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    MPI_Aint *c_address_array = NULL;
    MPI_Datatype *c_datatype_array = NULL;
    MPI_Datatype mtype = MPI_Type_f2c(*datatype);
    int i;

    if (*max_datatypes) {
        c_datatype_array = ulm_malloc(*max_datatypes * sizeof(MPI_Datatype));
        if (c_datatype_array == (MPI_Datatype) NULL) {
            *rc = MPI_ERR_INTERN;
            return;
        }
    }

    if (*max_addresses) {
        c_address_array = ulm_malloc(*max_addresses * sizeof(MPI_Aint));
        if (c_address_array == (MPI_Aint *) NULL) {
            *rc = MPI_ERR_INTERN;
            return;
        }
    }

    *rc = MPI_Type_get_contents(mtype, *max_integers, *max_addresses,
                                *max_datatypes, array_of_integers, c_address_array, c_datatype_array);

    if (*rc == MPI_SUCCESS) {
        for (i = 0; i < *max_addresses; i++) {
            array_of_addresses[i] = (MPI_Fint)c_address_array[i];
        }
        for (i = 0; i < *max_datatypes; i++) {
            if (c_datatype_array[i] != MPI_DATATYPE_NULL) {
                ULMType_t *t = c_datatype_array[i];
                array_of_datatypes[i] = t->fhandle;
            }
            else {
                /* Fortran MPI_DATATYPE_NULL is -1 */
                array_of_datatypes[i] = -1;
            }
        }
    }

    if (c_address_array != NULL)
        ulm_free(c_address_array);
    if (c_datatype_array != NULL)
        ulm_free(c_datatype_array);
}


#ifdef HAVE_PRAGMA_WEAK

#pragma weak PMPI_TYPE_GET_CONTENTS = mpi_type_get_contents_f
#pragma weak pmpi_type_get_contents = mpi_type_get_contents_f
#pragma weak pmpi_type_get_contents_ = mpi_type_get_contents_f
#pragma weak pmpi_type_get_contents__ = mpi_type_get_contents_f

#pragma weak MPI_TYPE_GET_CONTENTS = mpi_type_get_contents_f
#pragma weak mpi_type_get_contents = mpi_type_get_contents_f
#pragma weak mpi_type_get_contents_ = mpi_type_get_contents_f
#pragma weak mpi_type_get_contents__ = mpi_type_get_contents_f

#else

void PMPI_TYPE_GET_CONTENTS(MPI_Fint *datatype, MPI_Fint *max_integers,
                       MPI_Fint *max_addresses, MPI_Fint *max_datatypes,
                       MPI_Fint *array_of_integers, MPI_Fint *array_of_addresses,
                       MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    mpi_type_get_contents_f(datatype, max_integers, max_addresses,
                            max_datatypes, array_of_integers,
                            array_of_addresses, array_of_datatypes, rc);
}

void pmpi_type_get_contents(MPI_Fint *datatype, MPI_Fint *max_integers,
                       MPI_Fint *max_addresses, MPI_Fint *max_datatypes,
                       MPI_Fint *array_of_integers, MPI_Fint *array_of_addresses,
                       MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    mpi_type_get_contents_f(datatype, max_integers, max_addresses,
                            max_datatypes, array_of_integers,
                            array_of_addresses, array_of_datatypes, rc);
}

void pmpi_type_get_contents_(MPI_Fint *datatype, MPI_Fint *max_integers,
                       MPI_Fint *max_addresses, MPI_Fint *max_datatypes,
                       MPI_Fint *array_of_integers, MPI_Fint *array_of_addresses,
                       MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    mpi_type_get_contents_f(datatype, max_integers, max_addresses,
                            max_datatypes, array_of_integers,
                            array_of_addresses, array_of_datatypes, rc);
}

void pmpi_type_get_contents__(MPI_Fint *datatype, MPI_Fint *max_integers,
                       MPI_Fint *max_addresses, MPI_Fint *max_datatypes,
                       MPI_Fint *array_of_integers, MPI_Fint *array_of_addresses,
                       MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    mpi_type_get_contents_f(datatype, max_integers, max_addresses,
                            max_datatypes, array_of_integers,
                            array_of_addresses, array_of_datatypes, rc);
}

void MPI_TYPE_GET_CONTENTS(MPI_Fint *datatype, MPI_Fint *max_integers,
                       MPI_Fint *max_addresses, MPI_Fint *max_datatypes,
                       MPI_Fint *array_of_integers, MPI_Fint *array_of_addresses,
                       MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    mpi_type_get_contents_f(datatype, max_integers, max_addresses,
                            max_datatypes, array_of_integers,
                            array_of_addresses, array_of_datatypes, rc);
}

void mpi_type_get_contents(MPI_Fint *datatype, MPI_Fint *max_integers,
                       MPI_Fint *max_addresses, MPI_Fint *max_datatypes,
                       MPI_Fint *array_of_integers, MPI_Fint *array_of_addresses,
                       MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    mpi_type_get_contents_f(datatype, max_integers, max_addresses,
                            max_datatypes, array_of_integers,
                            array_of_addresses, array_of_datatypes, rc);
}

void mpi_type_get_contents_(MPI_Fint *datatype, MPI_Fint *max_integers,
                       MPI_Fint *max_addresses, MPI_Fint *max_datatypes,
                       MPI_Fint *array_of_integers, MPI_Fint *array_of_addresses,
                       MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    mpi_type_get_contents_f(datatype, max_integers, max_addresses,
                            max_datatypes, array_of_integers,
                            array_of_addresses, array_of_datatypes, rc);
}

void mpi_type_get_contents__(MPI_Fint *datatype, MPI_Fint *max_integers,
                       MPI_Fint *max_addresses, MPI_Fint *max_datatypes,
                       MPI_Fint *array_of_integers, MPI_Fint *array_of_addresses,
                       MPI_Fint *array_of_datatypes, MPI_Fint *rc)
{
    mpi_type_get_contents_f(datatype, max_integers, max_addresses,
                            max_datatypes, array_of_integers,
                            array_of_addresses, array_of_datatypes, rc);
}

#endif
