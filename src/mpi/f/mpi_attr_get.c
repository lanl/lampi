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
#include "internal/ftoc.h"

void mpi_attr_get_f(MPI_Comm *comm, MPI_Fint *keyval,
                    MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    int tmpFlag;
    FortranLogical *flag = (FortranLogical *) flagp;

    if (*keyval < 0) {          /* built-in attributes */
        int *addr;

        *rc = PMPI_Attr_get(*comm, *keyval, &addr, &tmpFlag);
        *attribute_val = *addr;
    } else {
        int tmpAttributeVal = 0;

        *rc = MPI_Attr_get(*comm, *keyval, &tmpAttributeVal, &tmpFlag);
        if ((tmpFlag) && (*rc == MPI_SUCCESS)) {
            *((int *) attribute_val) = tmpAttributeVal;
        }
    }

    /* set the fortran logical return value */
    LOGICAL_SET((*flag), &tmpFlag);
}

#if defined(HAVE_PRAGMA_WEAK)

#pragma weak PMPI_ATTR_GET = mpi_attr_get_f
#pragma weak pmpi_attr_get = mpi_attr_get_f
#pragma weak pmpi_attr_get_ = mpi_attr_get_f
#pragma weak pmpi_attr_get__ = mpi_attr_get_f

#pragma weak MPI_ATTR_GET = mpi_attr_get_f
#pragma weak mpi_attr_get = mpi_attr_get_f
#pragma weak mpi_attr_get_ = mpi_attr_get_f
#pragma weak mpi_attr_get__ = mpi_attr_get_f

#else

void PMPI_ATTR_GET(MPI_Comm *comm, MPI_Fint *keyval,
                   MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    mpi_attr_get_f(comm, keyval, attribute_val, flagp, rc);
}

void pmpi_attr_get(MPI_Comm *comm, MPI_Fint *keyval,
                   MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    mpi_attr_get_f(comm, keyval, attribute_val, flagp, rc);
}

void pmpi_attr_get_(MPI_Comm *comm, MPI_Fint *keyval,
                    MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    mpi_attr_get_f(comm, keyval, attribute_val, flagp, rc);
}

void pmpi_attr_get__(MPI_Comm *comm, MPI_Fint *keyval,
                     MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    mpi_attr_get_f(comm, keyval, attribute_val, flagp, rc);
}

void MPI_ATTR_GET(MPI_Comm *comm, MPI_Fint *keyval,
                   MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    mpi_attr_get_f(comm, keyval, attribute_val, flagp, rc);
}

void mpi_attr_get(MPI_Comm *comm, MPI_Fint *keyval,
                   MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    mpi_attr_get_f(comm, keyval, attribute_val, flagp, rc);
}

void mpi_attr_get_(MPI_Comm *comm, MPI_Fint *keyval,
                    MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    mpi_attr_get_f(comm, keyval, attribute_val, flagp, rc);
}

void mpi_attr_get__(MPI_Comm *comm, MPI_Fint *keyval,
                     MPI_Fint *attribute_val, MPI_Fint *flagp, MPI_Fint *rc)
{
    mpi_attr_get_f(comm, keyval, attribute_val, flagp, rc);
}

#endif
