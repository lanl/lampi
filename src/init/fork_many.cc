/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * fork_many:
 *
 * fork_many does multiple forks using either a linear approach, or a
 * binary tree approach which may be advantageous on large SMP systems
 *
 * During the fork, child process data structures are initialized and
 * signal handlers are installed so that if any process exits
 * abnormally, all processes will clean themselves
 *
 * Notes:
 *
 * This does not provide protection against a particular process
 * receiving an uncatchable SIGKILL.
 *
 * In the tree approach if any process (or in the linear approach if
 * the root process) exits normally, then the signals can no longer be
 * propagated.  For this reason, the signaling clean-up mechanism
 * implemented here is only intended to clean up the processes in the
 * case of failure during initialization.  Another more robust method
 * of cleaning up the processes should be set up during initalization.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "init/fork_many.h"
#include "os/atomic.h"
#include "internal/malloc.h"
#include "internal/state.h"
#include "queue/globals.h"

/*
 * Structure describing a child process
 */
struct child_process {
    struct child_process *next;
    pid_t pid;
    int rank;
    volatile int exited;
    int status;
};

/*
 * File scope variables, accessible by signal handlers
 */
static struct child_process *child_list = NULL;
static volatile sig_atomic_t fatal_error_in_progress = 0;
static int maximum_debug_level = 0;
static int process_rank = -1;

/*
 * Debug utility
 */
static void debug(int level, const char *fmt, ...)
{
    if (level <= maximum_debug_level) {
        va_list ap;

        va_start(ap, fmt);
        fprintf(stderr, "fork_many: DEBUG: level %d: ", level);
        vfprintf(stderr, fmt, ap);
        fflush(stderr);
        va_end(ap);
    }
}

/*
 * special client daemon wait for children called in loop
 * code before normal exit...
 */
void daemon_wait_for_children(void)
{
    struct child_process *p;
    bool keepGoing;
    
    do {
        keepGoing = false;
        for (p = child_list; p; p = p->next) {
            if ((p->pid != -1) && (p->exited != 1))
                keepGoing = true;
        }
    } while (keepGoing && (lampiState.AbnormalExit->flag == 0));

    return;
}

/*
 * Exit handler (see atexit()) to wait around for children to 
 * exit
 */
void wait_for_children(void)
{
    struct child_process *p;
    bool keepGoing;
    
    if (!lampiState.useDaemon || 
        (lampiState.useDaemon && lampiState.iAmDaemon)) {
        return;
    }

    do {
        keepGoing = false;
        for (p = child_list; p; p = p->next) {
            if ((p->pid != -1) && (p->exited != 1))
                keepGoing = true;
        }
        /* wait around if children have not exited, or client daemon has not
         * seen abnormal exit yet...
         */
    } while (keepGoing && (lampiState.AbnormalExit->flag < 2));

    return;
}

/*
 * Handler for SIGCHLD. If a child exits abnormally, abort.
 */
static void sigchld_handler(int signo)
{
    struct child_process *p;
    int old_errno = errno;
    int abnormal_exit = 0, abnormal_signal = 0, abnormal_exitstatus = 0;
    int status;
    pid_t abnormal_pid = 0;

    /*
     * Signals can be merged so loop over the children to make sure we
     * identify all of the children who signaled us
     */

    while (1) {

        pid_t pid;
        struct rusage ru;

        /*
         * Ask for status until we get a definite result
         */
        do {
            errno = 0;
#if defined(HAVE_WAIT3) && ! defined(__osf__)
            pid = wait3(&status, WNOHANG, &ru);
#else
            pid = waitpid(-1, &status, WNOHANG);
#endif
        } while (pid <= 0 && errno == EINTR);

        if (pid <= 0) {
            /*
             * No more children reporting
             */
            break;
        }

        /*
         * Find the process(es) that signaled us, and check status
         */

        for (p = child_list; p; p = p->next) {
            if (p->pid == pid) {
                p->status = status;
                p->exited = 1;
                if (WIFEXITED(status)) {
                    if ((abnormal_exitstatus = WEXITSTATUS(status)) != EXIT_SUCCESS) {
                        abnormal_exit++;
                        abnormal_pid = pid;
                        abnormal_signal = 0;
                    }
                } else if (WIFSIGNALED(status)) {
                    abnormal_exit++;
                    abnormal_pid = pid;
                    abnormal_exitstatus = 0;
                    abnormal_signal = WTERMSIG(status);
                }

                /*
                 * Copy rusage info into shared memory
                 */
#if defined(HAVE_WAIT3) && ! defined(__osf__)
                if (lampiState.useDaemon) {
                    /* p->rank is local_rank + 1 (daemon is 0) */
                    memcpy(&(lampiState.rusage[p->rank - 1]),
                           &ru, sizeof(struct rusage));
                }
#endif
            }
        }

        if (abnormal_exit)
            break;

        /*
         * And if the user created children?  Not our problem ...
         */
    }

    if (abnormal_exit) {
        if (lampiState.useDaemon) {
            ATOMIC_LOCK(lampiState.AbnormalExit->lock);
            if (lampiState.AbnormalExit->flag == 0) {
                /* store this information for the client daemon */
                lampiState.AbnormalExit->flag = 1;
                lampiState.AbnormalExit->pid = abnormal_pid;
                lampiState.AbnormalExit->signal = abnormal_signal;
                lampiState.AbnormalExit->status = abnormal_exitstatus;
            }
            ATOMIC_UNLOCK(lampiState.AbnormalExit->lock);
            if (!lampiState.iAmDaemon) {
                ulm_dbg(("Abnormal Exit: (signal = %d).\n", abnormal_signal));
                exit(EXIT_FAILURE);
            }
        }
        else {
            debug(1, "%d(pid:%d) aborting due to abnormal exit of child\n",
                process_rank, getpid());
            for (p = child_list; p; p = p->next) {
                if (p->exited) {
                    if (WIFSIGNALED(status)) {
                        debug(1,"%d(pid:%d) child %d(pid:%d) exited due to signal %d\n",
                            process_rank, getpid(), p->rank, p->pid,
                            WTERMSIG(status));
                    } else {
                        debug(1, "%d(pid:%d) child %d(pid:%d) with status %d\n",
                            process_rank, getpid(), p->rank, p->pid,
                            WEXITSTATUS(status));
                    }
                } else {
                    debug(1, "%d(pid:%d) SIGHUP to child %d(pid:%d)\n",
                        process_rank, getpid(), p->rank, p->pid);
                    kill(p->pid, SIGHUP);
                }
            }
            fflush(stderr);

            // signal my process group
            kill(0, SIGHUP);

            exit(EXIT_FAILURE);
        }
    }

    errno = old_errno;
    return;
}

/*
 * Handler for fatal signals.
 */
static void fatal_signal_handler(int signo)
{
    struct child_process *p;

    if (fatal_error_in_progress) {
        raise(signo);
    }
    fatal_error_in_progress = 1;

    /*
     * clean up:
     * - signal child processes
     * - remove lock files???
     * - send abnormal exit message to daemon???
     */

    debug(1, "%d(pid:%d) received fatal signal (%d)\n",
          process_rank, getpid(), signo);
    if (child_list) {
        for (p = child_list; p; p = p->next) {
            debug(1, "%d(pid:%d) sending signal (%d) to child %d(pid:%d)\n",
                  process_rank, getpid(), signo, p->rank, p->pid);
            kill(p->pid, signo);
        }
    }

    /*
     * Restore default handler, and raise the signal again
     */
    signal(signo, SIG_DFL);
    raise(signo);
}


/*
 * Fork children using a straightforward linear approach (0th process
 * forks all the others).  After the call there are "nprocs"
 * processes, each distinguished by the returned process rank in the
 * range [0,nprocs-1].
 *
 * Also install signal handlers to clean up in case of abnormal
 * terminations.
 *
 * \param nprocs        the number of processes after the fork
 * \return              the process rank in [0,nprocs-1], or -1
 *                      on error
 */
static int fork_many_linear(int nprocs, volatile pid_t *local_pids)
{
    struct child_process *p = 0;
    struct child_process *ptmp;
    struct sigaction action;
    sigset_t signals;
    int child_rank;

    process_rank = 0; /* file scope for handlers */

    if (nprocs < 2) {
        return process_rank;
    }

    /*
     * I am the rank 0 process, and have children, so:
     *
     * 1) Allocate and initialize child_list for keeping track of my
     * children
     *
     * 2) Install a SIGCHLD handler.
     * Set the mask to block all signals while the
     * handler runs.
     *
     * 3) Install a fatal signal handler, so that I can kill my
     * children when my parent exits.
     *
     * 4) Fork off my children and store there pids in the global
     * child_list (accessible to the signal handlers)
     */

    /*
     * allocate and initialize child_list
     */

    child_list = NULL;

    while (--nprocs) {
        ptmp = (struct child_process *) ulm_malloc(sizeof(struct child_process));
        if (ptmp == NULL) {
            return -1;
        }
        if (child_list) {
            p->next = ptmp;
        }  else {
            child_list = ptmp;
        }
        p = ptmp;
        p->next = NULL;
        p->pid = -1;
        p->rank = -1;
        p->exited = 0;
        p->status = 0;
    }

     /*
      * clear process mask...except for __pthread_sig_restart
      * used by glibc-LinuxThreads pthreads library
      */
     sigemptyset(&signals);
     sigaddset(&signals, SIGCHLD);
     if (sigprocmask(SIG_UNBLOCK, &signals, NULL) < 0) {
         perror("sigprocmask failed");
         abort();
     }

    /*
     * Install SIGCHLD handler
     */

    action.sa_handler = sigchld_handler;
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &action, NULL) < 0) {
        perror("sigaction failed");
        abort();
    }

    /*
     * Install fatal signal handler
     */

    if (sigaction(SIGHUP, NULL, &action) < 0) {
        perror("sigaction failed");
        abort();
    }
    if (action.sa_handler != SIG_IGN) {
        action.sa_handler = fatal_signal_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        if (sigaction(SIGHUP, &action, NULL) < 0) {
            perror("sigaction failed");
            abort();
        }
    }

    if (sigaction(SIGINT, NULL, &action) < 0) {
        perror("sigaction failed");
        abort();
    }
    if (action.sa_handler != SIG_IGN) {
        action.sa_handler = fatal_signal_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        if (sigaction(SIGINT, &action, NULL) < 0) {
            perror("sigaction failed");
            abort();
        }
    }

    if (sigaction(SIGTERM, NULL, &action) < 0) {
        perror("sigaction failed");
        abort();
    }
    if (action.sa_handler != SIG_IGN) {
        action.sa_handler = fatal_signal_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        if (sigaction(SIGTERM, &action, NULL) < 0) {
            perror("sigaction failed");
            abort();
        }
    }

    /*
     * fork children
     */

    child_rank = 1;
    for (p = child_list; p; p = p->next) {
        p->pid = fork();
        if (p->pid < 0) {
            /* error */
            return -1;
        } else if (p->pid == 0) {
            /* child */
            for (p = child_list; p; p = ptmp) {
                ptmp = p->next;
                ulm_free(p);
            }
            child_list = NULL;
            process_rank = child_rank; /* file scope for handlers */
	    mb();
            break;
        } else {
            /* parent */
            p->rank = child_rank;
	    local_pids[child_rank] = p->pid;
            child_rank++;
        }
    }

    return process_rank;
}


/*
 * Fork children using a recursive binary tree algorithm.  If called
 * with parent_rank = 0, then after the call there are "nprocs"
 * processes, each distinguished by the returned process rank in the
 * range [0,nprocs-1].
 *
 * Also install signal handlers to clean up in case of abnormal
 * terminations.
 *
 * \param rank          the input process rank, 0 on initial call
 * \param nprocs        the number of processes after the fork
 * \return              the process rank in [0,nprocs-1], or -1
 *                      on error
 */
static int fork_many_tree(int rank, int nprocs, volatile pid_t *local_pids)
{
    struct child_process *p;
    struct child_process *ptmp;
    int child_rank;

    /*
     * Construct a linked list of structs describing the child
     * processes (if there are to be any).
     *
     * The child process ranks are calcluated from the parent rank by
     * a standard bit-shift algorithm.
     */

    child_list = NULL;

    /*
     * left child
     */

    child_rank = ((rank + 1) << 1) - 1;
    if (child_rank < nprocs) {
        p = (struct child_process *) ulm_malloc(sizeof(struct child_process));
        if (p == NULL) {
            return -1;
        }
        p->next = NULL;
        p->pid = -1;
        p->rank = child_rank;
        p->exited = 0;
        p->status = 0;

        child_list = p;
    }

    /*
     * right child
     */

    child_rank = child_rank + 1;
    if (child_rank < nprocs) {
        p = (struct child_process *) ulm_malloc(sizeof(struct child_process));
        if (p == NULL) {
            return -1;
        }
        p->next = NULL;
        p->pid = -1;
        p->rank = child_rank;
        p->exited = 0;
        p->status = 0;

        child_list->next = p;
    }

    if (child_list) {

        /*
         * I have children, so:
         *
         * 1) Install a SIGCHLD handler.
         * Set the mask to block all signals while the
         * handler runs.
         *
         * 2) Install a fatal signal handler, so that I can kill my
         * children when my parent exits.
         *
         * 3) Fork off my children and store there pids in the global
         * child_list (accessible to the signal handlers
         */

        struct sigaction action;
        sigset_t signals;

        /*
         * clear process mask...except for __pthread_sig_restart
         * used by glibc-LinuxThreads pthreads library
         */
        sigemptyset(&signals);
        sigaddset(&signals, SIGCHLD);
        if (sigprocmask(SIG_UNBLOCK, &signals, NULL) < 0) {
            perror("sigprocmask failed");
            abort();
        }

        /*
         * Install SIGCHLD handler
         */

        if (sigaction(SIGCHLD, NULL, &action) < 0) {
            perror("sigaction failed");
            abort();
        }
        action.sa_handler = sigchld_handler;
        action.sa_flags |= SA_NOCLDSTOP;
        if (sigaction(SIGCHLD, &action, NULL) < 0) {
            perror("sigaction failed");
            abort();
        }

        /*
         * Install fatal signal handler
         */

        if (sigaction(SIGHUP, NULL, &action) < 0) {
            perror("sigaction failed");
            abort();
        }
        if (action.sa_handler != SIG_IGN) {
            action.sa_handler = fatal_signal_handler;
            sigemptyset(&action.sa_mask);
            action.sa_flags = 0;
            if (sigaction(SIGHUP, &action, NULL) < 0) {
                perror("sigaction failed");
                abort();
            }
        }

        if (sigaction(SIGINT, NULL, &action) < 0) {
            perror("sigaction failed");
            abort();
        }
        if (action.sa_handler != SIG_IGN) {
            action.sa_handler = fatal_signal_handler;
            sigemptyset(&action.sa_mask);
            action.sa_flags = 0;
            if (sigaction(SIGINT, &action, NULL) < 0) {
                perror("sigaction failed");
                abort();
            }
        }

        if (sigaction(SIGTERM, NULL, &action) < 0) {
            perror("sigaction failed");
            abort();
        }
        if (action.sa_handler != SIG_IGN) {
            action.sa_handler = fatal_signal_handler;
            sigemptyset(&action.sa_mask);
            action.sa_flags = 0;
            if (sigaction(SIGTERM, &action, NULL) < 0) {
                perror("sigaction failed");
                abort();
            }
        }

        /*
         * fork children
         */

        for (p = child_list; p; p = p->next) {
            p->pid = fork();
            if (p->pid < 0) {
                /* error */
                return -1;
            } else if (p->pid == 0) {
                /* child */
                rank = p->rank;
                for (p = child_list; p; p = ptmp) {
                    ptmp = p->next;
                    ulm_free(p);
                }
                child_list = NULL;
                rank = fork_many_tree(rank, nprocs, local_pids);
                process_rank = rank; /* file scope for handlers */
                break;
            }
	    else {
		local_pids[p->rank] = p->pid;
	    }
        }
    }

    return rank;
}


/*
 * Fork many processes.
 * \param nprocs        the number of processes after the fork
 * \param type          the algorithm used
 * \return              the process rank in [0,nprocs-1], or -1
 *                      on error
 */
int fork_many(int nprocs, enum fork_many_type type, volatile pid_t *local_pids)
{
	int rank;
    if (type == FORK_MANY_TYPE_TREE) {
        rank=fork_many_tree(0, nprocs, local_pids);
    } else {
        rank=fork_many_linear(nprocs, local_pids);
    }

    /* return local rank */
    return rank;
}


#ifdef TEST_PROGRAM

#include <ctype.h>
#include <getopt.h>

int main(int argc, char *argv[])
{
    struct child_process *p;
    int c;
    int nprocs = 0;
    int timeout = 5;
    int proc_rank_to_kill = 0;
    int proc;
    enum fork_many_type fork_many_type = FORK_MANY_TYPE_TREE;

    maximum_debug_level = 1;

    while ((c = getopt (argc, argv, "k:n:t:LT")) != -1) {
        switch (c) {
        case 'n':
            nprocs = atoi(optarg);
            break;
        case 't':
            timeout = atoi(optarg);
            break;
        case 'k':
            proc_rank_to_kill = atoi(optarg);
            break;
        case 'L':
            fork_many_type = FORK_MANY_TYPE_LINEAR;
            break;
        case 'T':
            fork_many_type = FORK_MANY_TYPE_TREE;
            break;
        case '?':
            if (isprint(optopt)) {
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            } else {
                fprintf(stderr,
                        "Unknown option character `\\x%x'.\n",
                        optopt);
            }
            /* fall through */
        default:
            fprintf(stderr,
                    "Usage: %s [-n <nprocs>] [-k <process to kill>] [-t timeout]\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (proc_rank_to_kill < 0 || proc_rank_to_kill >= nprocs) {
        fprintf(stderr, "process to kill is out of range\n");
        exit(EXIT_FAILURE);
    }

    /*
     * here there is one process
     */

    printf("about to fork: 1 -> %d processes\n", nprocs);

    proc = fork_many(nprocs, fork_many_type);
    if (proc < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    /*
     * here, we are process "proc" of "nprocs"
     *
     * everyone print out info
     */

    printf("rank(self=%d", proc);
    if (proc == 0) {
        printf(" parent=none");
    } else {
        if (fork_many_type == FORK_MANY_TYPE_TREE) {
            printf(" parent=%d", (proc - 1) >> 1);
        } else {
            printf(" parent=0");
        }
    }
    for (p = child_list; p; p = p->next) {
        printf(" child=%d", p->rank);
    }
    printf(") pid(self=%d parent=%d",
           getpid(), getppid());
    for (p = child_list; p; p = p->next) {
        printf(" child=%d", p->pid);
    }
    printf(")\n");
    fflush(stdout);

    /*
     * wait for timeout, and then kill one process
     */

    sleep(1);
    if (proc == 0) {
        printf("sleeping: seconds remaining %3d", timeout);
        fflush(stdout);
        while (timeout--) {
            sleep(1);
            printf("\b\b\b%3d", timeout);
            fflush(stdout);
        }
        printf("\n");
        fflush(stdout);
    } else {
        sleep(timeout);
    }
    sleep(1);

    if (proc_rank_to_kill == 0) {
        if (proc == 0) {
            printf("0(pid:%d) sending SIGTERM to 0(pid:%d)\n",
                   getpid(), getpid());
            fflush(stdout);
        }
        sleep(1);
        kill(getpid(), SIGTERM);

    } else if (child_list) {
        for (p = child_list; p; p = p->next) {
            if (p->rank == proc_rank_to_kill) {
                printf("%d(pid:%d) sending SIGTERM to %d(pid:%d)\n",
                       proc, getpid(), p->rank, p->pid);
                fflush(stdout);
                sleep(1);
                kill(p->pid, SIGTERM);
            }
        }
    }

    while (1) {
        sleep(1);
    }

    return EXIT_SUCCESS;
}

#endif /* TEST_PROGRAM */
