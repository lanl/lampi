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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "run/coprocess.h"

/*
 * fork/exec a process and get stdio descriptors
 */
int coprocess_open(char *const argv[], coprocess_t *cp)
{
    int pfdin[2];
    int pfdout[2];
    int pfderr[2];

    if (pipe(pfdin) < 0) {
        return -1;              /* errno set by pipe() */
    }
    if (pipe(pfdout) < 0) {
        return -1;              /* errno set by pipe() */
    }
    if (pipe(pfderr) < 0) {
        return -1;              /* errno set by pipe() */
    }

    if ((cp->pid = fork()) < 0) {

        return -1;              /* errno set by fork() */

    } else if (cp->pid == 0) {

        /*
         * child
         */

        close(pfdin[1]);
        if (pfdin[0] != STDIN_FILENO) {
            if (dup2(pfdin[0], STDIN_FILENO) != STDIN_FILENO) {
                return -1;      /* errno set by dup2() */
            }
            close(pfdin[0]);
        }
        close(pfdout[0]);
        if (pfdout[1] != STDOUT_FILENO) {
            if (dup2(pfdout[1], STDOUT_FILENO) != STDOUT_FILENO) {
                return -1;      /* errno set by dup2() */
            }
            close(pfdout[1]);
        }
        close(pfderr[0]);
        if (pfderr[1] != STDERR_FILENO) {
            if (dup2(pfderr[1], STDERR_FILENO) != STDERR_FILENO) {
                return -1;      /* errno set by dup2() */
            }
            close(pfderr[1]);
        }
        if (execvp(argv[0], argv) < 0) {
            perror("exec");
            exit(EXIT_FAILURE);
        }

    } else {

        /*
         * parent
         */

        close(pfdin[0]);
        close(pfdout[1]);
        close(pfderr[1]);

        cp->fdin = pfdin[1];
        cp->fdout = pfdout[0];
        cp->fderr = pfderr[0];
        if ((cp->fpin = fdopen(cp->fdin, "w")) == NULL) {
            return -1;
        }
        if ((cp->fpout = fdopen(cp->fdout, "r")) == NULL) {
            return -1;
        }
        if ((cp->fperr = fdopen(cp->fderr, "r")) == NULL) {
            return -1;
        }
    }

    return 0;
}


int coprocess_close(coprocess_t *cp)
{
    int status;

    fclose(cp->fpin);
    fclose(cp->fpout);
    fclose(cp->fperr);

    while (waitpid(cp->pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;          /* error other than EINTR from waitpid() */
        }
    }

    return status;
}


#ifdef TEST

int main(int argc, char *argv[])
{
    enum { MAXLINE = 1024 };
    char line[MAXLINE];
    coprocess_t cp[1];
    fd_set fdset[3];
    int maxfdp1;
    int rc;

    char *exec_argv[] = { "cat", "nonexistentfile", "-", NULL };

    rc = coprocess_open(exec_argv, cp);
    if (rc < 0) {
        perror("coprocess_open");
        return EXIT_FAILURE;
    }

    printf("cp->pid = %d\n", (int) cp->pid);
    printf("cp->fdin = %d\n", cp->fdin);
    printf("cp->fdout = %d\n", cp->fdout);
    printf("cp->fderr = %d\n", cp->fderr);
    printf("cp->fpin = %p\n", cp->fpin);
    printf("cp->fpout = %p\n", cp->fpout);
    printf("cp->fperr = %p\n", cp->fperr);

    fputs("here comes the data\n", cp->fpin);
    fputs("and more data\n", cp->fpin);
    fputs("and more\n", cp->fpin);
    fclose(cp->fpin);

    FD_ZERO(&fdset);
    FD_SET(cp->fdout, &fdset);
    FD_SET(cp->fderr, &fdset);
    maxfdp1 = cp->fdout > cp->fderr ? cp->fdout : cp->fderr;

    while (1) {
        rc = select(maxfdp1, &fdset, NULL, NULL, NULL);

        if (FD_ISSET(cp->fdout, &fdset)) {
            if ((fgets(line, sizeof(line), cp->fpout)) == NULL) {
                break;
            }
            fputs("stdout> ", stdout);
            fputs(line, stdout);
            fflush(stdout);
            memset(line, 0, sizeof(line));
        }
        if (FD_ISSET(cp->fderr, &fdset)) {
            if ((fgets(line, sizeof(line), cp->fperr)) == NULL) {
                break;
            }
            fputs("stderr> ", stderr);
            fputs(line, stderr);
            fflush(stderr);
            memset(line, 0, sizeof(line));
        }
    }

    rc = coprocess_close(cp);
    if (rc < 0) {
        perror("coprocess_close");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#endif
