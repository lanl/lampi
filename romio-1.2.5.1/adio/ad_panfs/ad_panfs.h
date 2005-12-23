/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 * ad_panfs.h
 *
 * @PANASAS_COPYRIGHT@
 */

#ifndef AD_UNIX_INCLUDE
#define AD_UNIX_INCLUDE

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "adio.h"

#ifndef NO_AIO
#ifdef AIO_SUN
#include <sys/asynch.h>
#else
#include <aio.h>
#ifdef NEEDS_AIOCB_T
typedef struct aiocb aiocb_t;
#endif
#endif
#endif

int ADIOI_PANFS_aio(ADIO_File fd, void *buf, int len, ADIO_Offset offset,
		  int wr, void *handle);

#endif
