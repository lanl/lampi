/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 * ad_panfs_open.c
 *
 * @PANASAS_COPYRIGHT@
 */

#include "ad_panfs.h"
#include "pan_fs_client_cw_mode.h"

void ADIOI_PANFS_Open(ADIO_File fd, int *error_code)
{
    char* value;
    int perm, old_mask, amode, flag;
#ifndef PRINT_ERR_MSG
    static char myname[] = "ADIOI_PANFS_OPEN";
#endif

    if (fd->perm == ADIO_PERM_NULL) {
	old_mask = umask(022);
	umask(old_mask);
	perm = old_mask ^ 0666;
    }
    else perm = fd->perm;

    amode = 0;
    if (fd->access_mode & ADIO_CREATE)
	amode = amode | O_CREAT;
    if (fd->access_mode & ADIO_RDONLY)
	amode = amode | O_RDONLY;
    if (fd->access_mode & ADIO_WRONLY)
	amode = amode | O_WRONLY;
    if (fd->access_mode & ADIO_RDWR)
	amode = amode | O_RDWR;
    if (fd->access_mode & ADIO_EXCL)
	amode = amode | O_EXCL;

	value = (char *) ADIOI_Malloc((MPI_MAX_INFO_VAL+1)*sizeof(char));
	MPI_Info_get(fd->info, "panfs_concurrent_write", MPI_MAX_INFO_VAL, 
		     value, &flag);
	if (flag) {
        int concurrent_write=atoi(value);
        if(concurrent_write == 1)
        {
            amode = amode | O_CONCURRENT_WRITE;
        }
	}
	ADIOI_Free(value);

    fd->fd_sys = open(fd->filename, amode, perm);

    if ((fd->fd_sys != -1) && (fd->access_mode & ADIO_APPEND))
	fd->fp_ind = fd->fp_sys_posn = lseek(fd->fd_sys, 0, SEEK_END);

#ifdef PRINT_ERR_MSG
    *error_code = (fd->fd_sys == -1) ? MPI_ERR_UNKNOWN : MPI_SUCCESS;
#else
    if (fd->fd_sys == -1) {
	*error_code = MPIR_Err_setmsg(MPI_ERR_IO, MPIR_ADIO_ERROR,
			      myname, "I/O Error", "%s", strerror(errno));
	ADIOI_Error(ADIO_FILE_NULL, *error_code, myname);	    
    }
    else *error_code = MPI_SUCCESS;
#endif
}
