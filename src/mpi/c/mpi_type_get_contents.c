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

#include "internal/mpi.h"
#include "os/atomic.h"

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Type_get_contents = PMPI_Type_get_contents
#endif

int PMPI_Type_get_contents(MPI_Datatype mtype, int max_integers, int max_addresses,
    int max_datatypes, int *array_of_integers, MPI_Aint *array_of_addresses,
    MPI_Datatype *array_of_datatypes)
{
    ULMType_t *type = mtype;
    int i;

    /* can't process a predefined named datatype */
    if ((mtype == MPI_DATATYPE_NULL) || (type->envelope.combiner == MPI_COMBINER_NAMED)) {
        return MPI_ERR_ARG;
    }

    /* arrays must be at least as big as values returned by MPI_Type_get_envelope() */
    if ((max_integers < type->envelope.nints) || (max_addresses < type->envelope.naddrs) ||
        (max_datatypes < type->envelope.ndatatypes)) {
        return MPI_ERR_ARG;
    }
    
    for (i = 0; i < max_integers; i++) {
        if (i < type->envelope.nints) { 
            array_of_integers[i] = type->envelope.iarray[i];
        }
        else {
            array_of_integers[i] = 0;
        }
    }
    for (i = 0; i < max_addresses; i++) {
        if (i < type->envelope.naddrs) {
            array_of_addresses[i] = type->envelope.aarray[i];
        }
        else {
            array_of_addresses[i] = (MPI_Aint)NULL;
        }
    }
    for (i = 0; i < type->envelope.ndatatypes; i++) {
        if (i < type->envelope.ndatatypes) {
            array_of_datatypes[i] = type->envelope.darray[i];
            if (!(((ULMType_t *)(type->envelope.darray[i]))->isbasic)) {
                fetchNadd(&(((ULMType_t *)(type->envelope.darray[i]))->ref_count), 1);
            }
        }
        else {
            array_of_datatypes[i] = MPI_DATATYPE_NULL;
        }
    }

    return MPI_SUCCESS;
}

