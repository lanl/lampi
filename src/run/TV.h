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



/*
 *  This header file contains data needed for TotalView to startup
 *    its debugging session.  This is what ULMRun needs for the
 *    setup.  The application processes also need some data.  The
 *    source for MPICH was looked at to see how to do the TotalView
 *    integration.
 *  ** Names are names TotalView is expecting, so DO NOT change them. **
 */
#include "client/adminMessage.h"

#ifndef _MPIRUNTV
#define _MPIRUNTV
extern "C" {
    typedef struct {
        char *host_name;  /* something that can be passed to inet_addr */
        char *executable_name;  /* name of binary */
        int pid;          /* process pid */
    } MPIR_PROCDESC;
};

extern "C" MPIR_PROCDESC *MPIR_proctable;
extern int MPIR_proctable_size;
extern int MPIR_being_debugged;
extern volatile int MPIR_debug_state;
extern volatile int MPIR_i_am_starter;

#define MPIR_DEBUG_SPAWNED  1
#define MPIR_DEBUG_ABORTING 2

extern volatile int TVDummy;
/* function prototype */
extern "C" void MPIR_Breakpoint();  /* dummy function used to let TotalView set a breakpoint */
void MPIrunTVSetUp(ULMRunParams_t *RunParameters, adminMessage *server); /* setup function */
void MPIrunTVSetUpApp(pid_t ** PIDsOfAppProcs);

#endif /* _MPIRUNTV */
