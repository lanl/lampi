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



#ifndef _UDPNETWORK_H_
#define _UDPNETWORK_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>		// for bzero

#include "util/Lock.h"

class UDPNetwork;
class adminMessage;


class UDPGlobals {
public:

    static const int NPortsPerProc = 2;	// one for short messages and one for long.
    static UDPNetwork* UDPNet;
    static bool checkLongMessageSocket;
    static Locks longMessageLock;

private:

    friend class UDPNetwork;
};


class UDPNetwork {
public:

    static int beginInitLocal(int num_hosts, int num_procs, int len_name, const char* names);

    int getLocalSocket(bool shortMsg)
        {
            return sockfd[(shortMsg ? 0 : 1)];
        }

    // info in network byte order
    struct sockaddr_in getHostAddr(int hostRank)
        {
            return hostAddrs[hostRank];
        }

    // info in network byte order
    unsigned short getHostPort(int procRank, bool shortMsg)
        {
            int basePort = procRank * UDPGlobals::NPortsPerProc;
            if (!shortMsg)
                basePort++;
            return procPorts[basePort];
        }

    // Default ctor, does not initialize socket fd's nor bind them
    UDPNetwork();

    UDPNetwork(int num_hosts, int num_procs, int len_name, const char* names);

    // Initialize sockets and bind them to addresses.
    int initialize(int ProcID);

    int nHosts;
    int nProcs;
    int sockfd[UDPGlobals::NPortsPerProc];
    unsigned short portID[UDPGlobals::NPortsPerProc];
    struct sockaddr_in addr;				// my local network address (recv on)
    volatile int *localProcessesDone;			// (shared memory) local process count initialization flag
    volatile int *hostsDone;					// (shared memory) local host count initialization flag
    struct sockaddr_in *hostAddrs;	// (shared memory) global list of network addresses (send to)
    unsigned short *procPorts;		// global list of process UDP ports (send to)
};


#endif // _UDPNETWORK_H_
