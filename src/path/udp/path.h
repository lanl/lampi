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



#ifndef _UDPPATH
#define _UDPPATH

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "ulm/ulm.h"
#include "queue/globals.h" /* for getMemPoolIndex() */
#include "path/common/path.h"
#include "path/udp/state.h"
#include "path/udp/sendFrag.h"
#include "path/udp/recvFrag.h"
#include "path/udp/UDPEarlySend.h"
#include "path/udp/UDPNetwork.h"

#ifdef ENABLE_RELIABILITY
#include "internal/constants.h"
#include "util/dclock.h"
#endif

class udpPath : public BasePath_t {
public:

    udpPath() {
        pathType_m = UDPPATH;
    }

    ~udpPath() {}

    bool packData(SendDesc_t *message, udpSendFragDesc *frag);

    virtual bool receive(double timeNow, int *errorCode, recvType recvTypeArg = ALL) {
        int error = ULM_SUCCESS;
        udpRecvFragDesc::pullFrags(error);
        if (error == ULM_SUCCESS) {
            *errorCode = ULM_SUCCESS;
            return true;
        }
        else {
            *errorCode = error;
            return false;
        }
    }

    virtual bool canReach(int globalDestProcessID) {
        struct sockaddr_in sockAddr = UDPGlobals::UDPNet->getProcAddr(globalDestProcessID);
        if (sockAddr.sin_addr.s_addr == 0)
            return false;
        else
            return true;
    }

    virtual bool bind(SendDesc_t *message, int *globalDestProcessIDArray, int destArrayCount, int *errorCode) {
        if (isActive()) {
            *errorCode = ULM_SUCCESS;
            message->path_m = this;
            return true;
        } else {
            *errorCode = ULM_ERR_BAD_PATH;
            return false;
        }
    }

    virtual void unbind(SendDesc_t *message, int *globalDestProcessIDArray, int destArrayCount) {
        ulm_err(("udpPath::unbind - called, not implemented yet...\n"));
    }

    virtual bool init(SendDesc_t *message) {
        message->numfrags = 1;
       	message->pathInfo.udp.numFragsCopied = 0; 
	if (message->posted_m.length_m > maxShortPayloadSize_g) {
            size_t remaining = message->posted_m.length_m-maxShortPayloadSize_g;
            message->numfrags += (remaining + maxPayloadSize_g - 1) / maxPayloadSize_g;
        }
        return true;
    }

    virtual bool send(SendDesc_t *message, bool *incomplete, int *errorCode);

#ifdef ENABLE_RELIABILITY
    
    bool doAck() { return true; }
    
    int fragSendQueue() {return UDPFRAGSTOSEND;}
    
    int toAckQueue() {return UDPFRAGSTOACK;}
    
#endif
    
};

#endif
