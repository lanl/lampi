/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 * ad_panfs_hints.c
 *
 * @PANASAS_COPYRIGHT@
 */

#include "ad_panfs.h"
#include "pan_fs_client_cw_mode.h"

void ADIOI_PANFS_SetInfo(ADIO_File fd, MPI_Info users_info, int *error_code)
{
    static char myname[] = "ADIOI_PANFS_SETINFO";
    char *value, *path, *file_name_ptr;
    int flag, tmp_val = -1;
    int concurrent_write = 0; 
    int raid_group_enable = 0;
    int raid_group_stripe_unit, raid_group_stripe_width, raid_group_stripe_depth, raid_group_total_blades  = -1;
    int gen_error_code;
    int err;

    *error_code = MPI_SUCCESS;

    if (fd->info == MPI_INFO_NULL) {
	    /* This must be part of the open call. can set striping parameters 
         * if necessary. 
         */ 
	    MPI_Info_create(&(fd->info));

        /* has user specified striping parameters 
               and do they have the same value on all processes? */
        if (users_info != MPI_INFO_NULL) {
	        value = (char *) ADIOI_Malloc((MPI_MAX_INFO_VAL+1)*sizeof(char));

            MPI_Info_get(users_info, "panfs_concurrent_write", MPI_MAX_INFO_VAL, 
                 value, &flag);
            if (flag) {
                concurrent_write=atoi(value);
                tmp_val = concurrent_write;
                MPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
                if (tmp_val != concurrent_write) {
                    FPRINTF(stderr, "ADIOI_PANFS_SetInfo: the value for key \"panfs_concurrent_write\" must be the same on all processes\n");
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
	            MPI_Info_set(fd->info, "panfs_concurrent_write", value); 
            }

            MPI_Info_get(users_info, "panfs_raid_group_enable", MPI_MAX_INFO_VAL, 
                 value, &flag);
            if (flag) {
                raid_group_enable=atoi(value);
                tmp_val = raid_group_enable;
                MPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
                if (tmp_val != raid_group_enable) {
                    FPRINTF(stderr, "ADIOI_PANFS_SetInfo: the value for key \"panfs_raid_group_enable\" must be the same on all processes\n");
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
	            MPI_Info_set(fd->info, "panfs_raid_group_enable", value); 
            }

            MPI_Info_get(users_info, "panfs_raid_group_stripe_unit", MPI_MAX_INFO_VAL, 
                 value, &flag);
            if (flag) {
                raid_group_stripe_unit=atoi(value);
                tmp_val = raid_group_stripe_unit;
                MPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
                if (tmp_val != raid_group_stripe_unit) {
                    FPRINTF(stderr, "ADIOI_PANFS_SetInfo: the value for key \"panfs_raid_group_stripe_unit\" must be the same on all processes\n");
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
	            MPI_Info_set(fd->info, "panfs_raid_group_stripe_unit", value); 
            }

            MPI_Info_get(users_info, "panfs_raid_group_stripe_width", MPI_MAX_INFO_VAL, 
                 value, &flag);
            if (flag) {
                raid_group_stripe_width=atoi(value);
                tmp_val = raid_group_stripe_width;
                MPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
                if (tmp_val != raid_group_stripe_width) {
                    FPRINTF(stderr, "ADIOI_PANFS_SetInfo: the value for key \"panfs_raid_group_stripe_width\" must be the same on all processes\n");
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
	            MPI_Info_set(fd->info, "panfs_raid_group_stripe_width", value); 
            }

            MPI_Info_get(users_info, "panfs_raid_group_stripe_depth", MPI_MAX_INFO_VAL, 
                 value, &flag);
            if (flag) {
                raid_group_stripe_depth=atoi(value);
                tmp_val = raid_group_stripe_depth;
                MPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
                if (tmp_val != raid_group_stripe_depth) {
                    FPRINTF(stderr, "ADIOI_PANFS_SetInfo: the value for key \"panfs_raid_group_stripe_depth\" must be the same on all processes\n");
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
	            MPI_Info_set(fd->info, "panfs_raid_group_stripe_depth", value); 
            }

            MPI_Info_get(users_info, "panfs_raid_group_total_blades", MPI_MAX_INFO_VAL, 
                 value, &flag);
            if (flag) {
                raid_group_total_blades=atoi(value);
                tmp_val = raid_group_total_blades;
                MPI_Bcast(&tmp_val, 1, MPI_INT, 0, fd->comm);
                if (tmp_val != raid_group_total_blades) {
                    FPRINTF(stderr, "ADIOI_PANFS_SetInfo: the value for key \"panfs_raid_group_total_blades\" must be the same on all processes\n");
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }
	            MPI_Info_set(fd->info, "panfs_raid_group_total_blades", value); 
            }
	        ADIOI_Free(value);

            if ((raid_group_enable > 0) && (raid_group_stripe_unit > 0) 
                    && (raid_group_stripe_width > 0) && (raid_group_stripe_depth > 0) 
                    && (raid_group_total_blades >0)) {
                int myrank;

                MPI_Comm_rank(fd->comm, &myrank);
                if (!myrank) {
                    unsigned int num_comps, rg_width, rg_depth;
                    pan_fs_client_layout_create_args_t file_create_args;    
                    int fd_dir;
                    int perm, old_mask;
                    char* slash;
                    struct stat stat_buf;

                    /* Check that the file does not exist before
                     * trying to create it
                     */
                    err = stat(fd->filename,&stat_buf);
                    if((err == -1) && (errno == ENOENT))
                    {
                        /* File does not exist */
                        if (fd->perm == ADIO_PERM_NULL) {
                            old_mask = umask(022);
                            umask(old_mask);
                            perm = old_mask ^ 0666;
                        }
                        else 
                        {
                            perm = fd->perm;
                        }

                        path = strdup(fd->filename);
                        slash = strrchr(path, '/');
                        if (!slash)
                            strcpy(path, ".");
                        else {
                            if (slash == path) 
                                *(path + 1) = '\0';
                            else *slash = '\0';
                        }

                        /* create raid group object */
                        bzero(&file_create_args,sizeof(pan_fs_client_layout_create_args_t)); 
                        /* open directory */
                        fd_dir = open(path, O_RDONLY);
                        if (fd_dir < 0) {
#ifdef PRINT_ERR_MSG
                            FPRINTF(stderr,"I/O Error opening parent directory to create PanFS file using raid groups. %s\n", strerror(errno));
                            *error_code = MPI_ERR_UNKNOWN;
#else
                            *error_code = MPIR_Err_setmsg(MPI_ERR_IO, MPIR_ADIO_ERROR,
                                          myname, "I/O Error opening parent directory to create PanFS file using raid groups", "%s", strerror(errno));
                            ADIOI_Error(ADIO_FILE_NULL, *error_code, myname);	    
#endif
                        }
                        else
                        {
                            char *file_name_ptr = fd->filename;
                            slash = strrchr(fd->filename, '/');
                            if (slash)
                            {
                                file_name_ptr = slash + 1;
                            }
                            /* create file in the directory */
                            file_create_args.mode = perm;
                            file_create_args.version = PAN_FS_CLIENT_LAYOUT_VERSION;
                            strcpy(file_create_args.filename, file_name_ptr); 
                            file_create_args.layout.agg_type = PAN_FS_CLIENT_LAYOUT_TYPE__RAIDGRP1_5; 
                            file_create_args.version = PAN_FS_CLIENT_LAYOUT_VERSION;
                            file_create_args.layout.layout_is_valid = 1;
                            file_create_args.layout.u.raidgrp1_5.total_num_comps = raid_group_total_blades;
                            file_create_args.layout.u.raidgrp1_5.raidgrp_width   = raid_group_stripe_width;
                            file_create_args.layout.u.raidgrp1_5.raidgrp_depth   = raid_group_stripe_depth;
                            file_create_args.layout.u.raidgrp1_5.stripe_unit     = raid_group_stripe_unit;
                            file_create_args.layout.u.raidgrp1_5.raidgrp_layout_policy   = PAN_FS_CLIENT_LAYOUT_RAIDGRP__ROUND_ROBIN;
                            err = ioctl(fd_dir, PAN_FS_CLIENT_LAYOUT_CREATE_FILE, &file_create_args);
                            if (err < 0) {
#ifdef PRINT_ERR_MSG
                                FPRINTF("I/O Error doing ioctl on parent directory to create PanFS file using raid groups %s\n", strerror(errno));
                                *error_code = MPI_ERR_UNKNOWN;
#else
                                *error_code = MPIR_Err_setmsg(MPI_ERR_IO, MPIR_ADIO_ERROR,
                                              myname, "I/O Error doing ioctl on parent directory to create PanFS file using raid groups", "%s", strerror(errno));
                                ADIOI_Error(ADIO_FILE_NULL, *error_code, myname);	    
#endif
                            }
                            err = close(fd_dir);
                        }
                        free(path);
                    }
                }
                MPI_Barrier(fd->comm);
            }
        }
        else
        {
	        MPI_Info_set(fd->info, "panfs_concurrent_write", "0");
	        MPI_Info_set(fd->info, "panfs_raid_group_enable", "0");
        }
    }

    ADIOI_GEN_SetInfo(fd, users_info, &gen_error_code); 
    /* If this function is successful, use the error code returned from ADIOI_GEN_SetInfo
     * otherwise use the error_code generated by this function
     */
    if(*error_code == MPI_SUCCESS)
    {
        *error_code = gen_error_code;
    }
}
