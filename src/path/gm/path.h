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

#ifndef GM_PATH_H
#define GM_PATH_H

#include <stdio.h>

#include "path/common/path.h"
#include "path/gm/state.h"
#include "path/gm/recvFrag.h"
#include "path/gm/sendFrag.h"
#include "ulm/ulm.h"

#ifdef ENABLE_RELIABILITY
#include "internal/constants.h"
#include "util/dclock.h"
#endif


class gmPath : public BasePath_t {

    // data members
 protected:

    // methods
public:

    gmPath() { pathType_m = GMPATH; }
    ~gmPath() {}

    bool canReach(int globalDestProcessID);
    bool send(SendDesc_t *message, bool *incomplete, int *errorCode);
    bool receive(double timeNow, int *errorCode, recvType recvTypeArg);

    bool init(SendDesc_t *message)
        {
            int nfrags;

            // initialize numfrags
            nfrags = (message->posted_m.length_m + gmState.bufSize - 1) / gmState.bufSize;
            if (nfrags == 0) {
                nfrags = 1;
            }
            message->numfrags = nfrags;

            return true;
        }

#ifdef ENABLE_RELIABILITY
    
    bool doAck() { return gmState.doAck; }
    
    int fragSendQueue() {return GMFRAGSTOSEND;}
    
    int toAckQueue() {return GMFRAGSTOACK;}
    
#endif

    static void callback(struct gm_port *p, void *context, enum gm_status status);
};

#endif /* GM_PATH_H */
