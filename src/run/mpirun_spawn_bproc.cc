/*
 * Copyright 2002-2003. The Regents of the University of California. This material 
 * was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
 * National Laboratory, which is operated by the University of California for 
 * the U.S. Department of Energy. The Government is granted for itself and 
 * others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
 * license in this material to reproduce, prepare derivative works, and 
 * perform publicly and display publicly. Beginning five (5) years after 
 * October 10,2002 subject to additional five-year worldwide renewals, the 
 * Government is granted for itself and others acting on its behalf a paid-up, 
 * nonexclusive, irrevocable worldwide license in this material to reproduce, 
 * prepare derivative works, distribute copies to the public, perform publicly 
 * and display publicly, and to permit others to do so. NEITHER THE UNITED 
 * STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
 * CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
 * IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
 * PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
 * OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "internal/constants.h"
#include "internal/malloc.h"
#include "internal/new.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "run/Run.h"
#include "internal/new.h"
#include "run/JobParams.h"
#include "internal/log.h"

#include "init/environ.h"

#ifdef BPROC
#include <sys/un.h>
#include <sys/bproc.h>
#endif
#define MAXHOSTNAMELEN 256
#define MAXBUFFERLEN 256
#define OVERWRITE 2

#ifdef __linux__
#define SETENV(NAME,VALUE)  setenv(NAME,VALUE,OVERWRITE);
#endif

/// nehal's copy
#ifdef BPROC
extern char **environ;
static int ERROR_LINE = 0;
static char *ERROR_FILE = NULL;
enum {
    EXEC_NAME,
    EXEC_ARGS,
    END
};
#define CHECK_FOR_ERROR(RC)   if ((RC) == NULL) { \
                                        ERROR_FILE = __FILE__;\
                                        ERROR_LINE = __LINE__;\
                                        goto CLEANUP_ABNORMAL; \
                                }

#endif

#ifdef BPROC

static void set_non_block(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
	perror("fcntl");
	exit(1);
    }
    flags |= O_NONBLOCK;
    flags = fcntl(fd, F_SETFL, flags);
    if (flags == -1) {
	perror("fcntl");
	exit(1);
    }
}

static void set_close_exec(int fd)
{
    int flags;
    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
	perror("fcntl");
	exit(1);
    }
    flags |= FD_CLOEXEC;
    flags = fcntl(fd, F_SETFL, flags);
    if (flags == -1) {
	perror("fcntl");
	exit(1);
    }
}

static int setup_socket(struct sockaddr_in *listenaddr)
{
    int fd;
    socklen_t addrsize;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
	perror("socket");
	exit(1);
    }
    listenaddr->sin_family = AF_INET;
    listenaddr->sin_addr.s_addr = INADDR_ANY;
    listenaddr->sin_port = 0;
    if (bind
	(fd, (struct sockaddr *) listenaddr, sizeof(*listenaddr)) == -1) {
	perror("bind");
	exit(1);
    }

    if (listen(fd, 1024) == -1) {
	perror("listen");
	exit(1);
    }
    addrsize = sizeof(struct sockaddr_in);
    getsockname(fd, (struct sockaddr *) listenaddr, &addrsize);
    set_close_exec(fd);
    set_non_block(fd);
    return fd;
}

#endif

#ifdef BPROC

static int *pids = 0;
static int iosock_fd[2];
static int max_sock_fd = -1;

void *accept_thread(void *arg) {
    ULMRunParams_t *RunParameters = (ULMRunParams_t *)arg;
	int nHosts = RunParameters->NHosts;
    int connectCount = 0, i, hostID = 0;
    fd_set rset;
    struct timeval tmo = { 300, 0 };

    /* enable asynchronous cancel mode for this thread */
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, (int *)NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, (int *)NULL);

    /*   the front end sits in an select loop
     *   when the remote side connects. the front end accepts.
     *   the remote end sends two
     *   things back. the pid and rfd (the process id
     *   and what type of file descriptor (stderr,stdout,stdin)
     */
    while (connectCount < 2 * nHosts) {
	FD_ZERO(&rset);
	/* set the sockets to listen on */
	for (int fdIndex = 0; fdIndex < 2; fdIndex++) {
	    FD_SET(iosock_fd[fdIndex], &rset);
	}

	/* see if any sockets to accept */
	int nRead = select(max_sock_fd + 1, &rset, NULL, NULL, &tmo);
	if (nRead > 0) {
	    for (int sock = 0; sock < 2; sock++) {
		if (FD_ISSET(iosock_fd[sock], &rset)) {
		    int fd = -1, pid = -1, rfd = -1;
		    socklen_t sa_size;
		    struct sockaddr sa;
		    sa_size = sizeof(sa);
		    fd = accept(iosock_fd[sock], &sa, &sa_size);
		    if (fd == -1 && errno != EAGAIN) {
			perror("accept");
			close(iosock_fd[sock]);
            pthread_exit((void *)0);
		    }
		    if (read(fd, &pid, sizeof(pid)) != sizeof(pid) ||
			read(fd, &rfd, sizeof(rfd)) != sizeof(rfd)) {
			fprintf(stderr,
				"mpirun_spawn_bproc: failed to read pid or fd"
				" from IO connection.\n");
			close(fd);
		    }
		    /* find host index that corresponds to this pid */
		    for (i = 0; i < nHosts; i++)
			if (pid == pids[i]) {
			    hostID = i;
			}
		    switch (rfd) {
		    case STDOUT_FILENO:
			if (RunParameters->STDOUTfds[hostID] != -1)
			    break;
			RunParameters->STDOUTfds[hostID] = fd;
			connectCount++;
			break;
		    case STDERR_FILENO:
			if (RunParameters->STDERRfds[hostID] != -1)
			    break;
			RunParameters->STDERRfds[hostID] = fd;
			connectCount++;
			break;
		    }
		}
	    }			/* end sock loop */
	}			/* end nread > 0 */
    }				/* end while (connectCount < 2 * nHosts) */
    pthread_exit((void *)1);
}
#endif

int mpirun_spawn_bproc(unsigned int *AuthData, int ReceivingSocket,
		       int **ListHostsStarted, ULMRunParams_t * RunParameters,
		       int FirstAppArgument, int argc, char **argv)
{
#ifdef BPROC
    /*     Strings with pre-assigned values  */
    char *execName = NULL;
    char *exec_args[END];
    char *tmp_args = NULL;
    char hostList[MAXHOSTNAMELEN];
    int *nodes = 0;
    int argsUsed = 0;
    int i = 0;
    int nHosts = 0;
    int ret_status = 0;
    size_t len = 0;
    char *auth0_str = "LAMPI_ADMIN_AUTH0";
    char *auth1_str = "LAMPI_ADMIN_AUTH1";
    char *auth2_str = "LAMPI_ADMIN_AUTH2";
    char *server_str = "LAMPI_ADMIN_IP";
    char *socket_str = "LAMPI_ADMIN_PORT";
    char *space_str = " ";
    /*   constructed pre-assigned string */
    char LAMPI_SOCKET[MAXBUFFERLEN];
    char LAMPI_AUTH[MAXBUFFERLEN];
    char LAMPI_SERVER[MAXBUFFERLEN];

#ifndef USE_CT
    struct bproc_io_t io[2];
    struct sockaddr_in addr;
    pthread_t a_thread;
    void *a_thread_return;
#endif

    /* initialize some values */
    memset(LAMPI_SOCKET, 0, MAXBUFFERLEN);
    memset(LAMPI_SERVER, 0, MAXBUFFERLEN);
    memset(LAMPI_AUTH, 0, MAXBUFFERLEN);
    memset(hostList, 0, MAXBUFFERLEN);

    exec_args[EXEC_NAME] = NULL;
    exec_args[EXEC_ARGS] = NULL;
    exec_args[END] = NULL;

	nHosts = RunParameters->NHosts;

	CHECK_FOR_ERROR(nodes =
			(int *) ulm_malloc(sizeof(int) * nHosts));
	CHECK_FOR_ERROR(pids =
			(int *) ulm_malloc(sizeof(int) * nHosts));

    for (i = 0; i < nHosts; i++) {
        nodes[i] = bproc_getnodebyname(RunParameters->HostList[i]);
    }

#ifndef USE_CT
    // allocate space for stdio file handles
    RunParameters->STDERRfds = ulm_new(int, nHosts);
    RunParameters->STDOUTfds = ulm_new(int, nHosts);
    for (i = 0; i < nHosts; i++) {
	RunParameters->STDERRfds[i] = -1;
	RunParameters->STDOUTfds[i] = -1;
    }
#endif

    // executable  for each of the hosts
    execName = RunParameters->ExeList[0];

    sprintf(LAMPI_SOCKET, "%d", ReceivingSocket);
    sprintf(LAMPI_SERVER, "%s", RunParameters->mpirunName);

    /* SERVER ENV */
    SETENV(server_str, LAMPI_SERVER);

    /* AUTH ENV */
    sprintf(LAMPI_AUTH, "%u", AuthData[0]);
    SETENV(auth0_str, LAMPI_AUTH);
    sprintf(LAMPI_AUTH, "%u", AuthData[1]);
    SETENV(auth1_str, LAMPI_AUTH);
    sprintf(LAMPI_AUTH, "%u", AuthData[2]);
    SETENV(auth2_str, LAMPI_AUTH);

    /* SOCKET ENV */
    SETENV(socket_str, LAMPI_SOCKET);

    /* adding executable name */
    CHECK_FOR_ERROR(exec_args[EXEC_NAME] =
		    (char *) ulm_malloc(sizeof(char) * strlen(execName)));
    sprintf(exec_args[EXEC_NAME], execName);

    /* program arguments */
    if ((argc - FirstAppArgument) > 0) {
	len = 0;
	argsUsed = 1;
	for (i = FirstAppArgument; i < argc; i++) {
	    len += strlen(argv[i]);
	    len += 1;		// for the space after the arguments

	}
	CHECK_FOR_ERROR(tmp_args =
			(char *) ulm_malloc(sizeof(char) * len));
	for (i = FirstAppArgument; i < argc; i++) {
	    strcat(tmp_args, argv[i]);
	    strcat(tmp_args, space_str);
	}
	CHECK_FOR_ERROR(exec_args[EXEC_ARGS] =
			(char *) ulm_malloc(sizeof(char) *
					    strlen(tmp_args)));
	sprintf(exec_args[EXEC_ARGS], tmp_args);
    }
    /*
     * finish filling in exec_args
     */
    exec_args[END] = NULL;

#ifdef USE_CT
    if (bproc_vexecmove
	(nHosts, nodes, pids, exec_args[EXEC_NAME],
	 argv + (FirstAppArgument - 1), environ) < 0) {
	fprintf(stderr, "bproc error  %s at line = %i in file= %s\n",
		strerror(errno), __LINE__, __FILE__);
	ret_status = errno;
	goto CLEANUP_ABNORMAL;
    }
#else
    /*
     * setup stdio sockets for the exec'ed processes to connect back
     *  to
     */
    for (int iofd = 0; iofd < 2; iofd++) {
	io[iofd].fd = iofd + 1;
	io[iofd].type = BPROC_IO_SOCKET;
	io[iofd].send_info = 1;
	((struct sockaddr_in *) &io[iofd].d.addr)->sin_family = AF_INET;
	((struct sockaddr_in *) &io[iofd].d.addr)->sin_addr.s_addr = 0;
	iosock_fd[iofd] = setup_socket(&addr);
	if (iosock_fd[iofd] < 0) {
	    perror("setup_socket");
	    goto CLEANUP_ABNORMAL;
	}

	if (iosock_fd[iofd] > max_sock_fd)
	    max_sock_fd = iosock_fd[iofd];
	((struct sockaddr_in *) &io[iofd].d.addr)->sin_port =
	    addr.sin_port;

    }

    /* spawn accept() processing thread */
    if (pthread_create(&a_thread, (pthread_attr_t *)NULL, accept_thread, RunParameters) != 0) {
        ulm_err(("Error: unable to create accept thread!\n"));
        goto CLEANUP_ABNORMAL;
    }

    if (bproc_vexecmove_io
	(nHosts, nodes, pids, io, 2, exec_args[EXEC_NAME],
	 argv + (FirstAppArgument - 1), environ) < 0) {
	fprintf(stderr, "bproc error  %s at line = %i in file= %s\n",
		strerror(errno), __LINE__, __FILE__);
	ret_status = errno;
    /* cancel accept() processing thread */
    pthread_cancel(a_thread);
	goto CLEANUP_ABNORMAL;
    }

    /* join accept() processing thread */
    if (pthread_join(a_thread, &a_thread_return) == 0) {
        int return_value = (int)a_thread_return;
        if (return_value == 0) {
            ulm_err(("Error: accept thread error!\n"));
            goto CLEANUP_ABNORMAL;
        }
    }
    else {
        ulm_err(("Error: unable to join accept thread!\n"));
        goto CLEANUP_ABNORMAL;
    }
#endif

    // clean up the memory allocations
    ulm_free(exec_args[EXEC_NAME]);
    if (argsUsed == 1) {
	free(tmp_args);
	free(exec_args[EXEC_ARGS]);
	free(nodes);
	free(pids);
    }
    return 0;

  CLEANUP_ABNORMAL:
    free(exec_args[EXEC_NAME]);
    if (argsUsed == 1) {
	free(tmp_args);
	free(exec_args[EXEC_ARGS]);
	free(nodes);
	free(pids);

    }
    if ((ERROR_LINE != 0) && (ERROR_FILE != NULL))
	fprintf(stderr, "Abnormal termination at line %i in file %s\n",
		ERROR_LINE, ERROR_FILE);
    return -1;

#endif
    return 0;
}
