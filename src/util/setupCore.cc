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

#ifdef __mips

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#elif __osf__

#include <sys/uswitch.h>

#endif

#include "ulm/errors.h"

/*
 *  Set-up unique core file names (if possible)
 */
int setupCore()
{
#ifdef __mips

    ptrdiff_t ret = prctl(PR_COREPID, 0, 1);
    if ((int) ret == -1) {
        return ULM_ERROR;
    }

#elif __osf__

    long uswitch_val;

    uswitch_val = uswitch(USC_GET, 0);
    if (uswitch_val < 0) {
        return ULM_ERROR;
    }
    if (uswitch(USC_SET, uswitch_val | USW_CORE) < 0) {
        return ULM_ERROR;
    }

#elif __linux__

    /*
     * On GNU/Linux, unique corefile names can be set using sysctl:
     *
     * /sbin/sysctl -w kernel.core_uses_pid=1
     *
     * which unfortunately must be run as root, or by adding
     *
     * kernel.core_uses_pid=1
     *
     * to /etc/sysctl.conf
     *
     * The claim in the glibc manual that the environment variable
     * COREFILE can be used to control the core file name is
     * apparently not true.
     *
     */

#endif

    return ULM_SUCCESS;
}


#ifdef TEST_PROGRAM

#include <stdlib.h>

int main(int argc, char *argv[])
{
    setupCore();
    abort();
    return EXIT_SUCCESS;
}

#endif
