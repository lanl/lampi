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

#include <stdio.h>

#include "queue/globals.h"
#include "internal/constants.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "internal/log.h"
#include "path/mcast/utsendInit.h"

#ifdef UDP
# include "path/udp/UDPNetwork.h"
#endif // UDP

#ifdef SHARED_MEMORY
#include "path/sharedmem/SMPfns.h"
#endif // SHARED_MEMORY

#ifdef RELIABILITY_ON
#include "util/DblLinkList.h"
void CheckForCollRetransmits(long mp);
#endif

/* debug */
#include "util/dclock.h"
//#include <pthread.h>
//extern pthread_key_t ptPrivate;
//#define dbgFile (FILE *)(pthread_getspecific(ptPrivate))
/* end debug */

#ifdef RELIABILITY_ON
void CheckForCollRetransmits(long mp)
{
    UtsendDesc_t *desc = 0;
    UtsendDesc_t *tmpDesc = 0;
    // stack list -- no locks inited, no locks used!
    DoubleLinkList tmpList;

    IncompleteUtsendDescriptors[mp]->Lock.lock();
    for (desc = (UtsendDesc_t *) IncompleteUtsendDescriptors[mp]->begin();
	 desc != (UtsendDesc_t *)
	     IncompleteUtsendDescriptors[mp]->end();
	 desc = (UtsendDesc_t *) desc->next) {
	if (desc->retransmitP() && (desc->Lock.trylock() == 1)) {
	    desc->reSend();
	    desc->Lock.unlock();
	}
    }
    IncompleteUtsendDescriptors[mp]->Lock.unlock();

    UnackedUtsendDescriptors[mp]->Lock.lock();
    for (desc = (UtsendDesc_t *) UnackedUtsendDescriptors[mp]->begin();
	 desc != (UtsendDesc_t *) UnackedUtsendDescriptors[mp]->end();
	 desc = (UtsendDesc_t *) desc->next) {
	if (desc->retransmitP() && (desc->Lock.trylock() == 1)) {
	    desc->reSend();
	    if (desc->incompletePt2PtMessages.size()) {
		tmpDesc = (UtsendDesc_t *)UnackedUtsendDescriptors[mp]->RemoveLinkNoLock(desc);
		// save the element on a temporary list for the moment to avoid
		// potential thread locking deadlock issues
		desc->WhichQueue = INCOMUTSENDDESC;
		tmpList.AppendNoLock(desc);
		desc->Lock.unlock();
		desc = tmpDesc;
	    }
	    else {
		desc->Lock.unlock();
	    }
	}
    }
    UnackedUtsendDescriptors[mp]->Lock.unlock();

    if (tmpList.size() > 0) {
	// save descriptors on the IncompleteList
	IncompleteUtsendDescriptors[mp]->Lock.lock();
	tmpList.MoveListNoLock(*IncompleteUtsendDescriptors[mp]);
	IncompleteUtsendDescriptors[mp]->Lock.unlock();
    }
}
#endif
