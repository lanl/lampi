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

#include "init/init.h"
#include "internal/options.h"
#include "path/common/pathContainer.h"
#include "path/ib/init.h"
#include "path/ib/path.h"


void lampi_init_postfork_ib(lampiState_t * s)
{
    if ((s->nhosts == 1) || (s->error))
        return;

    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_ib");
    }

    /* return if InfiniBand is not being used */
    if (!s->ib) {
        return;
    }

    /* set up IB communications */
    ibSetup(s);
    if ((s->error) || (!s->ib)) {
        return;
    }

    if (!s->iAmDaemon) {
        int pathHandle;
        ibPath *pathAddr;

        /*
         * Add InfiniBand path to global pathContainer
         */
        pathAddr = (ibPath *) (pathContainer()->add(&pathHandle));
        if (!pathAddr) {
            s->error = ERROR_LAMPI_INIT_POSTFORK_PATHS;
            return;
        }
        new(pathAddr) ibPath;
        pathContainer()->activate(pathHandle);
    }
}
