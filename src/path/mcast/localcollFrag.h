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

#ifndef _COLLFRAGDESC
#define _COLLFRAGDESC

#include "path/mcast/utsendDesc.h"

class LCFD:public BaseRecvFragDesc_t {
public:
    // default constructor
    LCFD() {
        WhichQueue = SMPFREELIST;
    }

    LCFD(int plIndex) {
        WhichQueue = SMPFREELIST;
        lock_m.init();
        poolIndex_m = plIndex;
    }

    // needed so that ack can be placed directly
    BaseSendDesc_t *SendingHeader;

    // flags that indicate state of frag
    int flags;

    // copy function - specific to communications layer
    virtual unsigned int CopyFunction(void *appAddr, void *fragAddr,
                                      ssize_t length);

    // check data - no-op in this case
    virtual bool CheckData(unsigned int checkSum, ssize_t length) {
        return true;
    }

    // function to send ack
    virtual bool AckData(double timeNow = -1.0);

    // return descriptor to descriptor pool
    virtual void ReturnDescToPool(int LocalRank);

    int tmap_index;
};

template < long SizeOfClass, long SizeWithNoPadding >
class LCFD_Padded:public LCFD {
public:
    LCFD_Padded() {
    }

    LCFD_Padded(int plIndex):LCFD(plIndex) {
    }

private:
    char padding[SizeOfClass - SizeWithNoPadding];
};

typedef LCFD_Padded < 256, sizeof(LCFD) > LocalCollFragDesc;

#endif /* !_COLLFRAGDESC */
