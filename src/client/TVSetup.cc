/*
 * Copyright 2002-2004. The Regents of the University of
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
 * Setup work needed by TotalView on the remote hosts.
 */

#include <stdio.h>

#include "internal/log.h"
#include "internal/profiler.h"

volatile int MPIR_acquired_pre_main = 1;
volatile int MPIR_debug_gate = 0;
volatile int *PtrToGate;
volatile int TVDummy = 0;       /* NEVER change this - this is a variable the
                                   MPIR_Breakpoint uses to trick the compiler
                                   thinking that this might be modified, and
                                   therefore not optimize MPIR_Breakpoint away */

/*
 * dummy subroutine - need to make sure that compiler will not
 * optimize this routine away, so that TotalView can set a breakpoint,
 * and do what it needs to.
 */
extern "C" void MPIR_Breakpoint(void)
{
    while (TVDummy == 1) {
        /* spin */;
    }
}


void ClientTVSetup(void)
{
    MPIR_acquired_pre_main = 1;
    PtrToGate = &MPIR_debug_gate;
    while (*PtrToGate == 0) {
        /* spin */;
    }
    MPIR_Breakpoint();
}

