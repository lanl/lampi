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

#ifndef _UDP_EARLYSEND_H_
#define _UDP_EARLYSEND_H_

#include <stdio.h>
#include <sys/socket.h>		        // needed for msghdr
#include <netinet/in.h>		        // needed for struct sockaddr_in
#include <sys/uio.h>		        // needed for iovecs

#include "queue/globals.h"	        // needed for local_myproc()
#include "util/DblLinkList.h"	// needed for DoubleLinkList
#include "path/common/BaseDesc.h"	// needed for BaseSendDesc_t
#include "path/udp/state.h"
#include "path/udp/header.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

#define EARLY_SEND_SMALL 256
#define EARLY_SEND_MED   4096
#define EARLY_SEND_LARGE 8192 

class UDPEarlySend_large:public Links_t {
  public:
    // Constructors
    UDPEarlySend_large() {} 
    UDPEarlySend_large(int a) {} 
    char data[EARLY_SEND_LARGE] ;
};

class UDPEarlySend_med:public Links_t {
    public:
    UDPEarlySend_med() {}
    UDPEarlySend_med(int a) {}
    char data[EARLY_SEND_MED];
};


class UDPEarlySend_small:public Links_t {
  public:
    // Constructors
    UDPEarlySend_small() {}
    UDPEarlySend_small(int a) {}
    char data[EARLY_SEND_SMALL];
};

#endif				// _UDP_EARLYSEND_H_
