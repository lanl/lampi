#include "mpi.h"
#include "mpio.h"  /* not necessary with MPICH 1.1.1 or HPMPI 1.4 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* prints out all the default info keys and values used by ROMIO */

int main(int argc, char **argv)
{
    int i, len, nkeys, flag, mynod, default_striping_factor=0, nprocs;
    MPI_File fh;
    MPI_Info info, info_used;
    char *filename, key[MPI_MAX_INFO_KEY], value[MPI_MAX_INFO_VAL];

    MPI_Init(&argc,&argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &mynod);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

/* process 0 takes the file name as a command-line argument and 
   broadcasts it to other processes */
    if (!mynod) {
	i = 1;
	while ((i < argc) && strcmp("-fname", *argv)) {
	    i++;
	    argv++;
	}
	if (i >= argc) {
	    fprintf(stderr, "\n*#  Usage: file_info -fname filename\n\n");
	    MPI_Abort(MPI_COMM_WORLD, 1);
	}
	argv++;
	len = strlen(*argv);
	filename = (char *) malloc(len+1);
	strcpy(filename, *argv);
	MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(filename, len+1, MPI_CHAR, 0, MPI_COMM_WORLD);
    }
    else {
	MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
	filename = (char *) malloc(len+1);
	MPI_Bcast(filename, len+1, MPI_CHAR, 0, MPI_COMM_WORLD);
    }

/* open the file with MPI_INFO_NULL */
    MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_CREATE | MPI_MODE_RDWR, 
                  MPI_INFO_NULL, &fh);

/* check the default values set by ROMIO */
    MPI_File_get_info(fh, &info_used);
    MPI_Info_get_nkeys(info_used, &nkeys);

    for (i=0; i<nkeys; i++) {
	MPI_Info_get_nthkey(info_used, i, key);
	MPI_Info_get(info_used, key, MPI_MAX_INFO_VAL-1, value, &flag);
	if (!mynod) 
	    fprintf(stderr, "Process %d, Default:  key = %s, value = %s\n", mynod, 
                key, value);
	if (!strcmp("striping_factor", key))
	  default_striping_factor = atoi(value);
    }

    MPI_File_close(&fh);

/* delete the file */
    if (!mynod) MPI_File_delete(filename, MPI_INFO_NULL);
    MPI_Barrier(MPI_COMM_WORLD);

/* set new info values. */

    MPI_Info_create(&info);

/* The following four hints are accepted on all machines. They can
   be specified at file-open time or later (any number of times). */

    /* buffer size for collective I/O */
    MPI_Info_set(info, "cb_buffer_size", "8388608");

    /* number of processes that actually perform I/O in collective I/O */
    sprintf(value, "%d", nprocs/2);
    MPI_Info_set(info, "cb_nodes", value);

    /* buffer size for data sieving in independent reads */
    MPI_Info_set(info, "ind_rd_buffer_size", "2097152");

    /* buffer size for data sieving in independent writes */
    MPI_Info_set(info, "ind_wr_buffer_size", "1048576");


/* The following three hints related to file striping are accepted only 
   on Intel PFS and IBM PIOFS file systems and are ignored elsewhere. 
   They can be specified only at file-creation time; if specified later 
   they will be ignored. */

    /* number of I/O devices across which the file will be striped.
       accepted only if 0 < value < default_striping_factor; 
       ignored otherwise */
    sprintf(value, "%d", default_striping_factor-1);
    MPI_Info_set(info, "striping_factor", value);

    /* the striping unit in bytes */
    MPI_Info_set(info, "striping_unit", "131072");

    /* the I/O device number from which to start striping the file.
       accepted only if 0 <= value < default_striping_factor; 
       ignored otherwise */
    sprintf(value, "%d", default_striping_factor-2);
    MPI_Info_set(info, "start_iodevice", value);


/* The following hint about PFS server buffering is accepted only on 
   Intel PFS. It can be specified anytime. */ 
    MPI_Info_set(info, "pfs_svr_buf", "true");

/* open the file and set new info */
    MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_CREATE | MPI_MODE_RDWR, 
                  info, &fh);

/* check the values set */
    MPI_File_get_info(fh, &info_used);
    MPI_Info_get_nkeys(info_used, &nkeys);

    if (!mynod) fprintf(stderr, "\n New values\n\n");
    for (i=0; i<nkeys; i++) {
	MPI_Info_get_nthkey(info_used, i, key);
	MPI_Info_get(info_used, key, MPI_MAX_INFO_VAL-1, value, &flag);
	if (!mynod) fprintf(stderr, "Process %d, key = %s, value = %s\n", mynod, 
                key, value);
    }
    
    MPI_File_close(&fh);
    free(filename);
    MPI_Info_free(&info_used);
    MPI_Info_free(&info);
    MPI_Finalize();
    return 0;
}
