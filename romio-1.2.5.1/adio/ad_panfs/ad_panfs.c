/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 * ad_panfs.c
 *
 * @PANASAS_COPYRIGHT@
 */

#include "ad_panfs.h"

/* adioi.h has the ADIOI_Fns_struct define */
#include "adioi.h"

struct ADIOI_Fns_struct ADIO_PANFS_operations = {
    ADIOI_PANFS_Open, /* Open */
    ADIOI_PANFS_ReadContig, /* ReadContig */
    ADIOI_PANFS_WriteContig, /* WriteContig */
    ADIOI_GEN_ReadStridedColl, /* ReadStridedColl */
    ADIOI_GEN_WriteStridedColl, /* WriteStridedColl */
    ADIOI_GEN_SeekIndividual, /* SeekIndividual */
    ADIOI_PANFS_Fcntl, /* Fcntl */
    ADIOI_PANFS_SetInfo, /* SetInfo */
    ADIOI_GEN_ReadStrided, /* ReadStrided */
    ADIOI_GEN_WriteStrided, /* WriteStrided */
    ADIOI_PANFS_Close, /* Close */
    ADIOI_PANFS_IreadContig, /* IreadContig */
    ADIOI_PANFS_IwriteContig, /* IwriteContig */
    ADIOI_PANFS_ReadDone, /* ReadDone */
    ADIOI_PANFS_WriteDone, /* WriteDone */
    ADIOI_PANFS_ReadComplete, /* ReadComplete */
    ADIOI_PANFS_WriteComplete, /* WriteComplete */
    ADIOI_PANFS_IreadStrided, /* IreadStrided */
    ADIOI_PANFS_IwriteStrided, /* IwriteStrided */
    ADIOI_GEN_Flush, /* Flush */
    ADIOI_PANFS_Resize, /* Resize */
    ADIOI_GEN_Delete, /* Delete */
};
