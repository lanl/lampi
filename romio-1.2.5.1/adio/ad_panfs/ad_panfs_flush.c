/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 * ad_panfs_flush.c
 *
 * @PANASAS_COPYRIGHT@
 */

#include "ad_panfs.h"

void ADIOI_PANFS_Flush(ADIO_File fd, int *error_code)
{
    ADIOI_GEN_Flush(fd, error_code);
}
