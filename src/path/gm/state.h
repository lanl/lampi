/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef NET_GM_STATE
#define NET_GM_STATE

#include <gm.h>

#include "queue/globals.h"
#include "util/DblLinkList.h"
#include "util/Lock.h"
#include "mem/MemoryPool.h"
#include "mem/FreeLists.h"
#include "internal/mmap_params.h"
#include "path/gm/header.h"
#include "path/gm/base_state.h"

class gmFragBuffer : public Links_t {
 public:
    gmFragBuffer *me;
    gmHeader header;
    char payload[GMFRAG_DEFAULT_FRAGMENT_SIZE - sizeof(gmHeader)];

    gmFragBuffer(int poolIndex) {}
};

struct remoteDevInfo_t {
    unsigned int node_id;
    unsigned int port_id;
};

class localDevInfo_t {
public:
    Locks Lock;
    unsigned int node_id;
    unsigned int port_id;
    unsigned int sendTokens;
    unsigned int recvTokens;
    int numBufsProvided[1];
    gm_port *gmPort;
    char macAddress[LENMACADDR];
    remoteDevInfo_t *remoteDevList;
    MemoryPoolPrivate_t memPool;
    FreeListPrivate_t <gmFragBuffer> bufList;
    bool devOK;

    localDevInfo_t() { devOK = true; }
};

class gmRecvFragDesc;
class gmSendFragDesc;

class gmState_t {

public:

    /*
     * GM does not provide a way to query the OS for the number of
     * devices configured.  Therefor we set an upper limit on devices
     * to try and open
     */
    enum { MAX_GM_DEVICES = 10 };

    /* GM does not provide a way to query the OS for the number of
     *   available ports on a device, w/o first opening it */
    enum { MAX_PORTS_PER_DEVICE = 32 };

    /* number of reserved GM ports */
    enum { NUM_RESERVED_PORTS = 3 };

    /* list of reserved GM ports */
    int reservedPorts[NUM_RESERVED_PORTS];

    /* upper limit on number of devices to open
     *   -1 == as many as*/
    int maxGMDevs;

    /* list of devices actually opened */
    localDevInfo_t localDevList[MAX_GM_DEVICES];

    /* actual number of ports opened */
    int nDevsAllocated;

    /* last GM device used */
    int lastDev;

    /* fragment size */
    size_t fragSize;

    /* payload buffer size (i.e. fragment size - header size) */
    size_t bufSize;

    /* GM "size" of a fragment (i.e. log2 size) */
    unsigned int log2Size;

    /* flag on whether to do ACK protocol */
    bool doAck;

    /* flag on whether to do checksumming */
    bool doChecksum;

    /* free list of GM send fragments */
    FreeListPrivate_t <gmSendFragDesc> sendFragList;

    /* free list of GM receive fragments */
    FreeListPrivate_t <gmRecvFragDesc> recvFragList;

    gmState_t()
	{
            reservedPorts[0] = 0;
            reservedPorts[1] = 1;
            reservedPorts[2] = 3;

            fragSize = GMFRAG_DEFAULT_FRAGMENT_SIZE;
            bufSize = fragSize - sizeof(gmHeader);
            log2Size = gm_min_size_for_length((unsigned long) fragSize);
            maxGMDevs = MAX_GM_DEVICES;
            doAck = false;
            doChecksum = false;
        }
};

// Global object to hold LA-MPI's gm run-time state

extern gmState_t gmState;

#endif
