/*
 *  MPIIncludes.h
 *  ScalableStartup
 */

#ifndef _MPI_INCLUDES_H_
#define _MPI_INCLUDES_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if defined(__APPLE__)
#include <netinet/in.h>
#endif

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>

#endif
