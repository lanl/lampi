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



#include "internal/mpi.h"

int ulm_type_get_elements(ULMType_t *dtype, int ntype, ssize_t offset, ssize_t lb, ssize_t ub, int *packed_bytes, int *count)
{
    int keep_going = 1;
    int i;

    switch (dtype->envelope.combiner) {
        case MPI_COMBINER_NAMED:
            if ((dtype->packed_size > 0) && (ntype > 0)) {
                int bytes = ntype * dtype->packed_size;
                if (!_MPI_DTYPE_TRIM 
                        || (((offset + bytes) > lb) 
                        && (offset < ub))) {
                    if (_MPI_DTYPE_TRIM && offset < lb) {
                        bytes -= (lb - offset);
                    }
                    if (_MPI_DTYPE_TRIM && offset + (ntype * dtype->packed_size) > ub) {
                        bytes -= (ub - (offset + (ntype * dtype->packed_size)));
                    }
                    if (bytes <= *packed_bytes) {
                        *packed_bytes -= bytes;
                        *count += bytes / dtype->packed_size;
                        keep_going = (*packed_bytes) ? 1 : 0;
                    }
                    else {
                        ntype = *packed_bytes / dtype->packed_size;
                        *count += ntype;
                        *packed_bytes -= ntype * dtype->packed_size;
                        keep_going = 0;
                    }
                }
            }
            break;
        case MPI_COMBINER_CONTIGUOUS:
            keep_going = ulm_type_get_elements(dtype->envelope.darray[0], 
                dtype->envelope.iarray[0], offset, dtype->lower_bound, 
                (ssize_t)(dtype->lower_bound + dtype->extent), packed_bytes, count);
            break;
        case MPI_COMBINER_VECTOR:
            for (i = 0; i < dtype->envelope.iarray[0]; i++) {
                keep_going = ulm_type_get_elements(dtype->envelope.darray[0],
                    dtype->envelope.iarray[1], (ssize_t)((i * dtype->envelope.iarray[2] * dtype->extent) + offset),
                    dtype->lower_bound, (ssize_t)(dtype->lower_bound + dtype->extent), packed_bytes, count);
                if (!keep_going) {
                    break;
                }
            }
            break;
        case MPI_COMBINER_HVECTOR_INTEGER:
        case MPI_COMBINER_HVECTOR:
            for (i = 0; i < dtype->envelope.iarray[0]; i++) {
                keep_going = ulm_type_get_elements(dtype->envelope.darray[0],
                    dtype->envelope.iarray[1], (ssize_t)((i * dtype->envelope.aarray[0]) + offset),
                    dtype->lower_bound, (ssize_t)(dtype->lower_bound + dtype->extent), packed_bytes, count);
                if (!keep_going) {
                    break;
                }
            }
            break;
        case MPI_COMBINER_INDEXED:
            for (i = 0; i < dtype->envelope.iarray[0]; i++) {
                keep_going = ulm_type_get_elements(dtype->envelope.darray[0],
                    dtype->envelope.iarray[i+1], (ssize_t)((dtype->envelope.iarray[i + dtype->envelope.iarray[0] + 1] * 
                    ((ULMType_t *)(dtype->envelope.darray[0]))->extent) + offset), 
                    dtype->lower_bound, (ssize_t)(dtype->lower_bound + dtype->extent), packed_bytes, count);
                if (!keep_going) {
                    break;
                }
            }
            break;
        case MPI_COMBINER_HINDEXED_INTEGER:
        case MPI_COMBINER_HINDEXED:
            for (i = 0; i < dtype->envelope.iarray[0]; i++) {
                keep_going = ulm_type_get_elements(dtype->envelope.darray[0],
                    dtype->envelope.iarray[i+1], (ssize_t)(dtype->envelope.aarray[i] + offset),
                    dtype->lower_bound, (ssize_t)(dtype->lower_bound + dtype->extent), packed_bytes, count);
                if (!keep_going) {
                    break;
                }
            }
            break;
        case MPI_COMBINER_STRUCT_INTEGER:
        case MPI_COMBINER_STRUCT:
            for (i = 0; i < dtype->envelope.iarray[0]; i++) {
	            if (((ULMType_t *)(dtype->envelope.darray[i]))->extent > 0) {
                    keep_going = ulm_type_get_elements(dtype->envelope.darray[i], 
                        dtype->envelope.iarray[i+1], (ssize_t)(dtype->envelope.aarray[i] + offset), dtype->lower_bound, 
                            (ssize_t)(dtype->lower_bound + dtype->extent), packed_bytes, count);
                    if (!keep_going) {
                        break;
                    }
	            }
            }
            break;
        default:
            keep_going = 0;
            break;
    }

    return keep_going;
}

#pragma weak MPI_Get_elements = PMPI_Get_elements
int PMPI_Get_elements(MPI_Status *status, MPI_Datatype mtype, int *count)
{
    int rc = MPI_SUCCESS;

    /* Check arguments */
    if (status == NULL || status == 0) {
	ulm_err(("Error: MPI_Get_Elements: Null status passed in\n"));
	rc = MPI_ERR_IN_STATUS;
    }

    if (count == NULL || count == 0) {
	ulm_err(("Error: MPI_Get_Elements: Null count passed in\n"));
	rc = MPI_ERR_COUNT;
    }

    if (rc == MPI_SUCCESS) {
	/* Calculate the number of typemap pairs received */
	ULMType_t *dtype = (ULMType_t *) mtype;
	int dtype_cnt = status->_count / dtype->packed_size;
    int total_count = 0, partial_count = 0;
    int packed_bytes;

    if (dtype_cnt == 0) {
        packed_bytes = status->_count;
        if (packed_bytes > 0) {
            ulm_type_get_elements(dtype, 1, (ssize_t)0, dtype->lower_bound, 
                (ssize_t)(dtype->lower_bound + dtype->extent), &packed_bytes, &partial_count);
        }
        *count = partial_count;
    }
    else {
        packed_bytes = dtype->packed_size;
        if (packed_bytes > 0) {
            ulm_type_get_elements(dtype, 1, (ssize_t)0, dtype->lower_bound,
                (ssize_t)(dtype->lower_bound + dtype->extent), &packed_bytes, &total_count);
        }
        packed_bytes = status->_count - (dtype_cnt * dtype->packed_size);
        if (packed_bytes > 0) {
            ulm_type_get_elements(dtype, 1, (ssize_t)0, dtype->lower_bound,
                (ssize_t)(dtype->lower_bound + dtype->extent), &packed_bytes, &partial_count);
        }
        *count = (total_count * dtype_cnt) + partial_count;
    }

    }
    else {
        _mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
    }

    return rc;
}
