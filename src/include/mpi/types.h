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

#ifndef _ULM_MPI_TYPES_H_
#define _ULM_MPI_TYPES_H_

#include <sys/types.h>

#include "ulm/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Types ***********************************************************/

/*
 * Data types
 */
typedef int MPI_Comm;
typedef int MPI_Group;
typedef void *MPI_Datatype;
typedef void *MPI_Op;
typedef void *MPI_Request;
typedef struct MPI_Status MPI_Status;
typedef int MPI_Fint;
typedef ssize_t MPI_Aint;
/* typedef ssize_t MPI_Offset; */
typedef int MPI_Errhandler;
/* typedef void *MPI_File;		 ??? */
/* typedef void *MPI_Info;		 ??? */
typedef void *MPI_Win;		 /* ??? */

/*
 * !!!!
 * Make sure this coincides with the layout of ULMStatus_t in
 * ulm/types.h and is the same size as MPI_STATUS_SIZE in mpi/mpif.h
 * !!!!
 */
struct MPI_Status {
    int MPI_SOURCE;
    int MPI_TAG;
    int MPI_ERROR;
    int _count;
    int _persistent;
    int _state;
};

/*
 * Function types
 */
typedef int (MPI_Copy_function) (MPI_Comm, int, void *, void *, void *, int *);
typedef int (MPI_Delete_function) (MPI_Comm, int, void *, void *);
typedef void (MPI_Comm_errhandler_fn) (MPI_Comm *, int *,...);
typedef void (MPI_File_errhandler_fn) (void *, void *,...);
typedef void (MPI_Handler_function) (MPI_Comm *, int *,...);
typedef void (MPI_User_function) (void *, void *, int *, MPI_Datatype *);
typedef void (MPI_Win_errhandler_fn) (MPI_Win *, int *,...);

#ifdef __cplusplus
}
#endif

#endif /* _ULM_MPI_TYPES_H_ */
