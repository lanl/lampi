/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 * ad_panfs_rdcoll.c
 *
 * @PANASAS_COPYRIGHT@
 */

#include "ad_panfs.h"

void ADIOI_PANFS_ReadStridedColl(ADIO_File fd, void *buf, int count,
                       MPI_Datatype datatype, int file_ptr_type,
                       ADIO_Offset offset, ADIO_Status *status, int
                       *error_code)
{
    ADIOI_GEN_ReadStridedColl(fd, buf, count, datatype, file_ptr_type,
			      offset, status, error_code);
}
