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



#include <elan/elan.h>
#include <rms/rmscall.h>
#include <stdlib.h>

#include "init/environ.h"

#include "init/init.h"
#include "internal/options.h"
#include "queue/globals.h"
#include "internal/malloc.h"


void lampi_init_prefork_rms(lampiState_t *s)
{
    ELAN_CAPABILITY cap;
    ELAN3_DEVINFO devinfo;
    int elan_nodeId;
    int *local2global;
    int host;
    int rank;

    if (s->error) {
	return;
    }
    if (s->verbose) {
	lampi_init_print("lampi_init_prefork_rms");
    }

    if (rms_getcap(0, &cap) < 0) {
	s->error = ERROR_LAMPI_INIT_PREFORK_RESOURCE_MANAGEMENT;
	return;
    }

    if (elan3_devinfo(0, &devinfo) != 0) {
	s->error = ERROR_LAMPI_INIT_PREFORK_RESOURCE_MANAGEMENT;
	return;
    }

    /*
     * Set up various structural info
     * - zero-based host ID
     * - total number of processes
     * - number of processes on this host
     * - maximum number of local processes on any host
     * - global to local rank offset
     * - global rank to host
     */

    lampi_environ_find_integer("RMS_NNODES", &(s->nhosts));
    elan_nodeId = devinfo.NodeId - cap.LowNode;
    s->local_size = elan3_nlocal(elan_nodeId, &cap);

    /*
     * RMS_NODEIDs are allocated contiguously from zero;
     * ELAN3_DEVINFO.NodeIds are not.
     */
    lampi_environ_find_integer("RMS_NODEID", &(s->hostid));

}


void lampi_init_postfork_rms(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_rms");
    }

    if (s->local_rank > 0) {
        if (rms_setcap(0, s->local_rank) < 0) {
            s->error = ERROR_LAMPI_INIT_POSTFORK_RESOURCE_MANAGEMENT;
            return;
        }
    }

    if (s->debug) {
	printf("hostid\t%d\n"
	       "global_size\t%d\n"
	       "global_to_local_offset\t%d\n"
	       "local_rank\t%d\n"
	       "local_size\t%d\n"
	       "max_local_size\t%d\n",
	       s->hostid,
	       s->global_size,
	       s->global_to_local_offset,
	       s->local_rank,
	       s->local_size,
	       s->max_local_size);
	fflush(stdout);
    }
}


void lampi_init_allforked_rms(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_allforked_rms");
    }
}
