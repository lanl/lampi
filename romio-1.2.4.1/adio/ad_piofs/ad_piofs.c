/* 
 *   $Id$
 *
 *   Copyright (C) 2001 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "ad_piofs.h"

/* adioi.h has the ADIOI_Fns_struct define */
#include "adioi.h"

struct ADIOI_Fns_struct ADIO_PIOFS_operations = {
    ADIOI_PIOFS_Open, /* Open */
    ADIOI_PIOFS_ReadContig, /* ReadContig */
    ADIOI_PIOFS_WriteContig, /* WriteContig */
    ADIOI_PIOFS_ReadStridedColl, /* ReadStridedColl */
    ADIOI_PIOFS_WriteStridedColl, /* WriteStridedColl */
    ADIOI_PIOFS_SeekIndividual, /* SeekIndividual */
    ADIOI_PIOFS_Fcntl, /* Fcntl */
    ADIOI_PIOFS_SetInfo, /* SetInfo */
    ADIOI_PIOFS_ReadStrided, /* ReadStrided */
    ADIOI_PIOFS_WriteStrided, /* WriteStrided */
    ADIOI_PIOFS_Close, /* Close */
    ADIOI_PIOFS_IreadContig, /* IreadContig */
    ADIOI_PIOFS_IwriteContig, /* IwriteContig */
    ADIOI_PIOFS_ReadDone, /* ReadDone */
    ADIOI_PIOFS_WriteDone, /* WriteDone */
    ADIOI_PIOFS_ReadComplete, /* ReadComplete */
    ADIOI_PIOFS_WriteComplete, /* WriteComplete */
    ADIOI_PIOFS_IreadStrided, /* IreadStrided */
    ADIOI_PIOFS_IwriteStrided, /* IwriteStrided */
    ADIOI_PIOFS_Flush, /* Flush */
    ADIOI_PIOFS_Resize, /* Resize */
    ADIOI_GEN_Delete, /* Delete */
};
