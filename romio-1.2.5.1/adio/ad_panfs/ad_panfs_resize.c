/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 * ad_panfs_resize.c
 *
 * @PANASAS_COPYRIGHT@
 */

#include "ad_panfs.h"

void ADIOI_PANFS_Resize(ADIO_File fd, ADIO_Offset size, int *error_code)
{
    int err;
#ifndef PRINT_ERR_MSG
    static char myname[] = "ADIOI_PANFS_RESIZE";
#endif
    
    err = ftruncate(fd->fd_sys, size);
#ifdef PRINT_ERR_MSG
    *error_code = (err == 0) ? MPI_SUCCESS : MPI_ERR_UNKNOWN;
#else
    if (err == -1) {
	*error_code = MPIR_Err_setmsg(MPI_ERR_IO, MPIR_ADIO_ERROR,
			      myname, "I/O Error", "%s", strerror(errno));
	ADIOI_Error(fd, *error_code, myname);	    
    }
    else *error_code = MPI_SUCCESS;
#endif
}
