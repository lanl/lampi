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

#ifndef GM_SENDFRAG_H
#define GM_SENDFRAG_H

#include "queue/globals.h"	        // needed for reliabilityInfo
#include "util/DblLinkList.h"	// needed for DoubleLinkList
#include "path/common/BaseDesc.h"	// needed for BaseSendDesc_t
#include "path/common/path.h"
#include "path/gm/state.h"               // GM runtime state

// process-private send frag descriptor

class gmSendFragDesc : public BaseSendFragDesc_t {

public:

    // data members

    int globalDestProc_m;
    int dev_m;
    long long fragSeq_m;
    bool initialized_m;

    // methods

    gmSendFragDesc() {}
    gmSendFragDesc(int poolIndex = 0) {}
    virtual ~gmSendFragDesc() {}

    // initialize fragment

    bool init(int globalDestProc = -1,
              int tmapIndex = -1,
              BaseSendDesc_t *parentSendDesc = NULL,
              size_t seqOffset = 0,
              size_t length = 0,
              BasePath_t *failedPath = NULL,
              packType packed = PACKED,
              int numTransmits = 0);

    bool initialized() { return currentSrcAddr_m ? true : false; }

    struct gm_port *gmPort()
        {
            return gmState.localDevList[dev_m].gmPort;
        }
        

    unsigned int gmDestNodeID()
        {
            return gmState.localDevList[dev_m].remoteDevList[globalDestProc_m].node_id;
        }
            
    unsigned int gmDestPortID()
        {
            return gmState.localDevList[dev_m].remoteDevList[globalDestProc_m].port_id;
        }
            
};

#endif // GM_SENDFRAG_H
