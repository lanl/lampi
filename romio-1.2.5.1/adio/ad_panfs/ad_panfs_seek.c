/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 * ad_panfs_seek.c
 *
 * @PANASAS_COPYRIGHT@
 */

#include "ad_panfs.h"

ADIO_Offset ADIOI_PANFS_SeekIndividual(ADIO_File fd, ADIO_Offset offset, 
		      int whence, int *error_code)
{
    return ADIOI_GEN_SeekIndividual(fd, offset, whence, error_code);
}
