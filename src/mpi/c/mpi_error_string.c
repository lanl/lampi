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



#include <string.h>

#include "internal/mpi.h"

static struct {
    int error_code;
    char *error_string;
} table[]  = {
    {MPI_SUCCESS, "No error"},
    {MPI_ERR_BUFFER, "Invalid buffer pointer"},
    {MPI_ERR_COUNT, "Invalid count"},
    {MPI_ERR_TYPE, "Invalid dataype"},
    {MPI_ERR_TAG, "Invalid tag"},
    {MPI_ERR_COMM, "Invalid communicator"},
    {MPI_ERR_RANK, "Invalid rank"},
    {MPI_ERR_REQUEST, "Invalid request"},
    {MPI_ERR_ROOT, "Invalid root"},
    {MPI_ERR_GROUP, "Invalid group"},
    {MPI_ERR_OP, "Invalid operation"},
    {MPI_ERR_TOPOLOGY, "Invalid topology"},
    {MPI_ERR_DIMS, "Invalid dimension"},
    {MPI_ERR_ARG, "Invalid argument"},
    {MPI_ERR_UNKNOWN, "Unknown error"},
    {MPI_ERR_TRUNCATE, "Message truncated on receive"},
    {MPI_ERR_OTHER, "Known error not in this list"},
    {MPI_ERR_INTERN, "Internal MPI error"},
    {MPI_ERR_PENDING, "Pending request"},
    {MPI_ERR_IN_STATUS, "Error code is in status"},
    {MPI_ERR_ACCESS, "MPI2 error -- ACCESS"},
    {MPI_ERR_AMODE, "MPI2 error -- AMODE"},
    {MPI_ERR_ASSERT, "MPI2 error -- ASSERT"},
    {MPI_ERR_BAD_FILE, "MPI2 error -- BAD_FILE"},
    {MPI_ERR_BASE, "MPI2 error -- BASE"},
    {MPI_ERR_CONVERSION, "MPI2 error -- CONVERSION"},
    {MPI_ERR_DISP, "MPI2 error -- DISP"},
    {MPI_ERR_DUP_DATAREP, "MPI2 error -- DUP_DATAREP"},
    {MPI_ERR_FILE_EXISTS, "MPI2 error -- FILE_EXISTS"},
    {MPI_ERR_FILE_IN_USE, "MPI2 error -- FILE_IN_USE"},
    {MPI_ERR_FILE, "MPI2 error -- FILE"},
    {MPI_ERR_INFO_KEY, "MPI2 error -- INFO_KEY"},
    {MPI_ERR_INFO_NOKEY, "MPI2 error -- INFO_NOKEY"},
    {MPI_ERR_INFO_VALUE, "MPI2 error -- INFO_VALUE"},
    {MPI_ERR_INFO, "MPI2 error -- INFO"},
    {MPI_ERR_IO, "MPI2 error -- IO"},
    {MPI_ERR_KEYVAL, "MPI2 error -- KEYVAL"},
    {MPI_ERR_LOCKTYPE, "MPI2 error -- LOCKTYPE"},
    {MPI_ERR_NAME, "MPI2 error -- NAME"},
    {MPI_ERR_NO_MEM, "MPI2 error -- NO_MEM"},
    {MPI_ERR_NOT_SAME, "MPI2 error -- NOT_SAME"},
    {MPI_ERR_NO_SPACE, "MPI2 error -- NO_SPACE"},
    {MPI_ERR_NO_SUCH_FILE, "MPI2 error -- NO_SUCH_FILE"},
    {MPI_ERR_PORT, "MPI2 error -- PORT"},
    {MPI_ERR_QUOTA, "MPI2 error -- QUOTA"},
    {MPI_ERR_READ_ONLY, "MPI2 error -- READ_ONLY"},
    {MPI_ERR_RMA_CONFLICT, "MPI2 error -- RMA_CONFLICT"},
    {MPI_ERR_RMA_SYNC, "MPI2 error -- RMA_SYNC"},
    {MPI_ERR_SERVICE, "MPI2 error -- SERVICE"},
    {MPI_ERR_SIZE, "MPI2 error -- SIZE"},
    {MPI_ERR_SPAWN, "MPI2 error -- SPAWN"},
    {MPI_ERR_UNSUPPORTED_DATAREP, "MPI2 error -- UNSUPPORTED_DATAREP"},
    {MPI_ERR_UNSUPPORTED_OPERATION, "MPI2 error -- UNSUPPORTED_OPERATION"},
    {MPI_ERR_WIN, "MPI2 error -- WIN"},
    {MPI_ERR_LASTCODE, ""}
};

#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Error_string = PMPI_Error_string
#endif

int PMPI_Error_string(int code, char *string, int *len)
{
    int i, not_found = 1;

    for (i = 0; i < MPI_ERR_LASTCODE; i++) {
	if (code == table[i].error_code) {
	    *len = strlen(table[i].error_string);
	    strcpy(string, table[i].error_string);
	    not_found = 0;
	    break;
	}
    }
    if (not_found) {
	strcpy(string, "Unknown error");
	*len = strlen(string);
    }

    return MPI_SUCCESS;
}
