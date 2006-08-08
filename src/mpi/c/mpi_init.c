/*
 * Copyright 2002-2006. The Regents of the University of
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

#include "internal/mpi.h"
#include "internal/mpif.h"


#if defined(__linux__) && defined(HAVE_EXECINFO_H)

#include <signal.h>
#include <execinfo.h>

static void show_stackframe(int signo, siginfo_t * info, void *p)
{
    char print_buffer[1024];
    char *tmp = print_buffer;
    int size = sizeof(print_buffer);
    int ret;
    char *str = "";
    char eof_msg[] = "*** End of error message ***\n";
    int i;
    int trace_size;
    void *trace[32];
    char **messages = (char **) NULL;

    /*
     * Yes, we are doing printf inside a signal-handler.
     * However, backtrace itself calls malloc (which may not be signal-safe,
     * under linux, printf and malloc are)
     *
     * We could use backtrace_symbols_fd and write directly into an
     * filedescriptor, however, without formatting -- also this fd 
     * should be opened in a sensible way...
     */
    memset(print_buffer, 0, sizeof(print_buffer));

    switch (signo) {
    case SIGILL:
        switch (info->si_code) {
#ifdef ILL_ILLOPC
        case ILL_ILLOPC:
            str = "ILL_ILLOPC";
            break;
#endif
#ifdef ILL_ILLOPN
        case ILL_ILLOPN:
            str = "ILL_ILLOPN";
            break;
#endif
#ifdef ILL_ILLADR
        case ILL_ILLADR:
            str = "ILL_ILLADR";
            break;
#endif
#ifdef ILL_ILLTRP
        case ILL_ILLTRP:
            str = "ILL_ILLTRP";
            break;
#endif
#ifdef ILL_PRVOPC
        case ILL_PRVOPC:
            str = "ILL_PRVOPC";
            break;
#endif
#ifdef ILL_PRVREG
        case ILL_PRVREG:
            str = "ILL_PRVREG";
            break;
#endif
#ifdef ILL_COPROC
        case ILL_COPROC:
            str = "ILL_COPROC";
            break;
#endif
#ifdef ILL_BADSTK
        case ILL_BADSTK:
            str = "ILL_BADSTK";
            break;
#endif
        }
        break;
    case SIGFPE:
        switch (info->si_code) {
#ifdef FPE_INTDIV
        case FPE_INTDIV:
            str = "FPE_INTDIV";
            break;
#endif
#ifdef FPE_INTOVF
        case FPE_INTOVF:
            str = "FPE_INTOVF";
            break;
#endif
        case FPE_FLTDIV:
            str = "FPE_FLTDIV";
            break;
        case FPE_FLTOVF:
            str = "FPE_FLTOVF";
            break;
        case FPE_FLTUND:
            str = "FPE_FLTUND";
            break;
        case FPE_FLTRES:
            str = "FPE_FLTRES";
            break;
        case FPE_FLTINV:
            str = "FPE_FLTINV";
            break;
#ifdef FPE_FLTSUB
        case FPE_FLTSUB:
            str = "FPE_FLTSUB";
            break;
#endif
        }
        break;
    case SIGSEGV:
        switch (info->si_code) {
#ifdef SEGV_MAPERR
        case SEGV_MAPERR:
            str = "SEGV_MAPERR";
            break;
#endif
#ifdef SEGV_ACCERR
        case SEGV_ACCERR:
            str = "SEGV_ACCERR";
            break;
#endif
        }
        break;
    case SIGBUS:
        switch (info->si_code) {
#ifdef BUS_ADRALN
        case BUS_ADRALN:
            str = "BUS_ADRALN";
            break;
#endif
#ifdef BUSADRERR
        case BUS_ADRERR:
            str = "BUS_ADRERR";
            break;
#endif
#ifdef BUS_OBJERR
        case BUS_OBJERR:
            str = "BUS_OBJERR";
            break;
#endif
        }
        break;
    case SIGTRAP:
        switch (info->si_code) {
#ifdef TRAP_BRKPT
        case TRAP_BRKPT:
            str = "TRAP_BRKPT";
            break;
#endif
#ifdef TRAP_TRACE
        case TRAP_TRACE:
            str = "TRAP_TRACE";
            break;
#endif
        }
        break;
    case SIGCHLD:
        switch (info->si_code) {
#ifdef CLD_EXITED
        case CLD_EXITED:
            str = "CLD_EXITED";
            break;
#endif
#ifdef CLD_KILLED
        case CLD_KILLED:
            str = "CLD_KILLED";
            break;
#endif
#ifdef CLD_DUMPED
        case CLD_DUMPED:
            str = "CLD_DUMPED";
            break;
#endif
#ifdef CLD_WTRAPPED
        case CLD_TRAPPED:
            str = "CLD_TRAPPED";
            break;
#endif
#ifdef CLD_STOPPED
        case CLD_STOPPED:
            str = "CLD_STOPPED";
            break;
#endif
#ifdef CLD_CONTINUED
        case CLD_CONTINUED:
            str = "CLD_CONTINUED";
            break;
#endif
        }
        break;
#ifdef SIGPOLL
    case SIGPOLL:
        switch (info->si_code) {
#ifdef POLL_IN
        case POLL_IN:
            str = "POLL_IN";
            break;
#endif
#ifdef POLL_OUT
        case POLL_OUT:
            str = "POLL_OUT";
            break;
#endif
#ifdef POLL_MSG
        case POLL_MSG:
            str = "POLL_MSG";
            break;
#endif
#ifdef POLL_ERR
        case POLL_ERR:
            str = "POLL_ERR";
            break;
#endif
#ifdef POLL_PRI
        case POLL_PRI:
            str = "POLL_PRI";
            break;
#endif
#ifdef POLL_HUP
        case POLL_HUP:
            str = "POLL_HUP";
            break;
#endif
        }
        break;
#endif                          /* SIGPOLL */
    default:
        switch (info->si_code) {
#ifdef SI_ASYNCNL
        case SI_ASYNCNL:
            str = "SI_ASYNCNL";
            break;
#endif
#ifdef SI_SIGIO
        case SI_SIGIO:
            str = "SI_SIGIO";
            break;
#endif
        case SI_ASYNCIO:
            str = "SI_ASYNCIO";
            break;
        case SI_MESGQ:
            str = "SI_MESGQ";
            break;
        case SI_TIMER:
            str = "SI_TIMER";
            break;
        case SI_QUEUE:
            str = "SI_QUEUE";
            break;
        case SI_USER:
            str = "SI_USER";
            break;
#ifdef SI_KERNEL
        case SI_KERNEL:
            str = "SI_KERNEL";
            break;
#endif
#ifdef SI_UNDEFINED
        case SI_UNDEFINED:
            str = "SI_UNDEFINED";
            break;
#endif
        }
    }

    ret =
        snprintf(tmp, size,
                 "Signal:%d info.si_errno:%d(%s) si_code:%d(%s)\n", signo,
                 info->si_errno, strerror(info->si_errno), info->si_code,
                 str);
    size -= ret;
    tmp += ret;

    switch (signo) {
    case SIGILL:
    case SIGFPE:
    case SIGSEGV:
    case SIGBUS:
        {
            ret = snprintf(tmp, size, "Failing at addr:%p\n",
                           info->si_addr);
            size -= ret;
            tmp += ret;
            break;
        }
    case SIGCHLD:{
            ret = snprintf(tmp, size, "si_pid:%d si_uid:%d si_status:%d\n",
                           info->si_pid, info->si_uid, info->si_status);
            size -= ret;
            tmp += ret;
            break;
        }
#ifdef SIGPOLL
    case SIGPOLL:{
#ifdef HAVE_SIGINFO_T_SI_FD
            ret = snprintf(tmp, size, "si_band:%ld si_fd:%d\n",
                           info->si_band, info->si_fd);
#elif HAVE_SIGINFO_T_SI_BAND
            ret = snprintf(tmp, size, "si_band:%ld\n", info->si_band);
#else
            size = 0;
#endif
            size -= ret;
            tmp += ret;
            break;
        }
#endif
    }

    write(fileno(stderr), print_buffer, sizeof(print_buffer) - size);
    fflush(stderr);

    trace_size = backtrace(trace, 32);
    messages = backtrace_symbols(trace, trace_size);

    for (i = 0; i < trace_size; i++) {
        fprintf(stderr, "[%d] func:%s\n", i, messages[i]);
        fflush(stderr);
    }

    free(messages);

    write(fileno(stderr), eof_msg, sizeof(eof_msg));
    fflush(stderr);
}

static void register_stacktrace_printer(void)
{
    struct sigaction act;
    int sigs[] = { SIGABRT, SIGSEGV, SIGBUS, 0 };
    int i;

    memset(&act, 0, sizeof(act));
    act.sa_sigaction = show_stackframe;
    act.sa_flags = SA_SIGINFO;
#ifdef SA_ONESHOT
    act.sa_flags |= SA_ONESHOT;
#else
    act.sa_flags |= SA_RESETHAND;
#endif

    for (i = 0; sigs[i] != 0; ++i) {
        sigaction(sigs[i], &act, NULL);
    }
}

#else

static void register_stacktrace_printer(void)
{
    return;
}

#endif                          /* defined(__linux__) && defined(HAVE_EXECINFO_H) */



#ifdef HAVE_PRAGMA_WEAK
#pragma weak MPI_Init = PMPI_Init
#endif

int PMPI_Init(int *argc, char ***argv)
{
    int rc;

    if (_mpi.finalized) {
        rc = MPI_ERR_OTHER;
        return rc;
    }

    /*
     * set MPI thread usage flag - MPI defines more parameters than
     * ulm uses internally
     */
    _mpi.thread_usage = MPI_THREAD_SINGLE;

    register_stacktrace_printer();

    rc = ulm_init();
    if (rc != ULM_SUCCESS) {
        return MPI_ERR_INTERN;
    }

    ulm_barrier(ULM_COMM_WORLD);
    rc = _mpi_init();
    if (rc) {
        return MPI_ERR_INTERN;
    }

    if (_mpi.fortran_layer_enabled) {
        rc = _mpif_init();
        if (rc) {
            return MPI_ERR_INTERN;
        }
    }

    return MPI_SUCCESS;
}
