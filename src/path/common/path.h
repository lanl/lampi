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

#ifndef _PATH_HEADER
#define _PATH_HEADER

#include <string.h>

#include "ulm/ulm.h"
#include "path/common/BaseDesc.h"
#include "util/DblLinkList.h"
#include "util/dclock.h"
#include "queue/globals.h"

enum pathType {
    UDPPATH,
    TCPPATH,
    QUADRICSPATH,
    GMPATH,
    SHAREDMEM,
    IBPATH
};

// enumeration used for getInfo/setInfo for paths
enum pathInfoType {
    PATH_TYPE,  /* returns string containing network type: "udp", etc. */
    USES_MULTICAST /* boolean indicating whether this path supports native multicast or not */
};

class pathContainer_t;

// base class for path objects
class BasePath_t {
public:
    // pointer to container
    pathContainer_t *myContainer;
    // valid handles positive integers starting at zero
    int myHandleInContainer;
    // is this path active or inactive?
    bool pathActive;
    // does this path support native multicast?
    bool usesMulticast;
    // type of path
    pathType pathType_m;
    // internal version
    int myVersion;
    // send Descriptor cache
    DoubleLinkList sendDescCache;

    enum recvType {
        POINT_TO_POINT,
        COLLECTIVE,
        ALL
    };

    // default constructor
    BasePath_t()
        {
            myContainer = 0;
            myHandleInContainer= -1;
            pathActive = false;
            usesMulticast = false;
            myVersion = 0;
        }

    // default destructor
    virtual ~BasePath_t();

    // set Handle to this path in the pathContainer
    void bindToContainer(pathContainer_t *container, int pathHandle) {
        myContainer = container;
        myHandleInContainer = pathHandle;
    }

    int getHandle() {
        return myHandleInContainer;
    }

    pathContainer_t *getContainer() {
        return myContainer;
    }

    bool isActive() {
        return pathActive;
    }

    // get information about this path, return success or failure of copying information
    // into addr, errorCode is set to indicate the relevant error
    // globalDestProcessID may or may not be used, depending upon the information
    virtual bool getInfo(pathInfoType info, int globalDestProcessID, void *addr, int addrSize, int *errorCode) {
        bool *boolResult = (bool *)addr;
        int *intResult = (int *)addr;
        *errorCode = ULM_SUCCESS;
        if (!addr) {
            *errorCode = ULM_ERROR;
            return false;
        }
        switch (info) {
        case PATH_TYPE:
            if (addrSize < (int)sizeof(int)) {
                *errorCode = ULM_ERROR;
                return false;
            }
            *intResult = pathType_m;
            break;
        case USES_MULTICAST:
            if (addrSize < (int)sizeof(bool)) {
                *errorCode = ULM_ERROR;
                return false;
            }
            *boolResult = usesMulticast;
            break;
        default:
            *errorCode = ULM_ERROR;
            return false;
        }
        return true;
    }

    // set information about this path, return success or failure of copying information
    // into addr, errorCode is set to indicate the relevant error
    // globalDestProcessID may or may not be used, depending upon the information
    virtual bool setInfo(pathInfoType info, int globalDestProcessID, void *addr, int addrSize, int *errorCode) {
        *errorCode = ULM_SUCCESS;
        return true;
    }

    // used by pathContainer upon activating a path object to configure
    // its internal destination process to list of path mappings
    virtual bool canReach(int globalDestProcessID) {
        return false;
    }

    // bind a message to this path, always return false if pathActive is false.
    // allocate network resources necessary to send this message for 1 or more destinations
    // in globalDestProcessIDArray
    virtual bool bind(SendDesc_t *message, int *globalDestProcessIDArray,
                      int destArrayCount, int *errorCode) {
        if (pathActive) {
            message->path_m = this;
            return true;
        }
        return false;
    }

    // unbind a message frag and release all path and network resources used for this
    // message; if destArray == 0, then unbind this message from all destinations.
    // otherwise, just unbind resources used to go to the specific list of destinations
    virtual void unbind(SendDesc_t *message, int *globalDestProcessIDArray, int destArrayCount) {
        if (message->path_m == this) {
            message->path_m = 0;
        }
    }

    // path-specific initialization
    virtual bool init(SendDesc_t *message) { return true; }

    // actually send this message via this path
    // will return false if the path is not capable of sending the message -- need to rebind;
    // will return true if the path is capable, and will set incomplete to true/false depending
    // upon the state of the message at the end of the call;
    // errorCode is only set if false is returned
    virtual bool send(SendDesc_t *message, bool *incomplete, int *errorCode) {
        *errorCode = ULM_SUCCESS;
        return true;
    }

    // is the send done?
    virtual bool sendDone(SendDesc_t *message, double timeNow, int *errorCode) {
	    unsigned int nAcked;
	    if ( message->path_m->pathType_m == SHAREDMEM ){
		    nAcked = message->pathInfo.sharedmem.sharedData->NumAcked;
	    }
	    else
		    nAcked = message->NumAcked;
        if ( (nAcked >= message->numfrags) && ((unsigned)message->NumSent >= message->numfrags) 
             && (message->NumSent >= 0) ) {
            return true;
        }
        *errorCode = ULM_SUCCESS;
        return false;
    }

    // called via progress call to service receive queues and other resources used by this path
    // recvTypeArg is a "hint" at best...
    virtual bool receive(double timeNow, int *errorCode, recvType recvTypeArg = ALL) {
        *errorCode = ULM_SUCCESS; return true; }

    // return the message descritor to the free list
    virtual void ReturnDesc(SendDesc_t *message, int poolIndex=-1);

    // called via progress call to send (or push) miscellaneous non-message related data, such as
    // control messages, etc.
    virtual bool push(double timeNow, int *errorCode) {
        *errorCode = ULM_SUCCESS;
        return true;
    }

    // called at ulm_finalize to see if there is any traffic that still needs to be pushed out
    virtual bool needsPush(void) { 
        return false;
    }

    // do any cleanup required before exiting
    virtual void finalize(void) { }
    
    // activate this path
    virtual void activate() { pathActive = true; }

    // deactivate this path
    virtual void deactivate() { pathActive = false; }

    virtual bool doAck() { return false; }
    
    virtual int fragSendQueue() {
        // return the queue identifier for the fragsToSend queue, e.g. GMFRAGSTOSEND
        return ONNOLIST;
    }
    
    virtual int toAckQueue() {
        // return the queue identifier for the fragsToAck queue, e.g. GMFRAGSTOACK
        return ONNOLIST;
    }
    
#if ENABLE_RELIABILITY    
    
    virtual void initFragSeq(BaseSendFragDesc_t *frag)
    {
        /* the first frag of a synchronous send needs a valid frag_seq
        * if ENABLE_RELIABILITY is defined -- since it is recorded and ACK'ed...
        */
        int     globalDestProc;
        
        if ( !doAck() ) {
            if ((frag->parentSendDesc_m->sendType != ULM_SEND_SYNCHRONOUS) ||
                (frag->seqOffset_m != 0)) {
                frag->setFragSequence(0);
                return;
            }
        }
        
        // thread-safe allocation of frag sequence number in header
        globalDestProc = frag->globalDestProc();
        if (usethreads())
            reliabilityInfo->next_frag_seqsLock[globalDestProc].lock();
        frag->setFragSequence(reliabilityInfo->next_frag_seqs[globalDestProc]++);
        if (usethreads())
            reliabilityInfo->next_frag_seqsLock[globalDestProc].unlock();
    }
    
    
    
    static unsigned int headerChecksum(const void *header, unsigned long crclen,
                                        int word_count)
    {
        unsigned int csum;
        int         i;
        ulm_uint32_t    *ptr;
        
#if BYTE_ORDER == LITTLE_ENDIAN
        unsigned char *tmpp, *csump;
        unsigned int tmp;
#endif
        
        if ( usecrc() )
        {
            csum = uicrc(header, crclen);
#if BYTE_ORDER == LITTLE_ENDIAN
            /* byte swap if little endian - so CRC of header + CRC yields 0 */
            csump = (unsigned char *)&csum;
            tmpp = (unsigned char *)&tmp;
            tmpp[0] = csump[3];
            tmpp[1] = csump[2];
            tmpp[2] = csump[1];
            tmpp[3] = csump[0];
            csum = tmp;
#endif
        }
        else {
            csum = 0;
            ptr = (ulm_uint32_t *)header;
            for (i = 0; i < word_count; i++) {
                csum += ptr[i];
            }
        }
        return csum;
    }
    
    
    virtual void freeResources(SendDesc_t *message, BaseSendFragDesc_t *frag)
    {
        /*
         This should only be called from the resend() logic for the situation
         where the msg was delivered, but the ACK was dropped.  So increment
         the ack counter.
         */
        message->clearToSend_m = true;
        (message->NumAcked)++;
        
        // This must be done before setting fragSeq_m and parentSendDesc_m to 0
        if ( frag->sendDidComplete() )
        {
            frag->freeResources(dclock(), frag->parentSendDesc_m);
            
            // set fragSeq_m value to 0/null/invalid to detect duplicate ACKs
            frag->setFragSequence(0);
            
            // reset send descriptor pointer
            frag->parentSendDesc_m = 0;            
        }
    }
    
    
    virtual bool retransmitP(SendDesc_t *message)
    {
        if (!doAck() || (RETRANS_TIME == -1) || (message->earliestTimeToResend == -1)
            || (message->FragsToAck.size() == 0))
            return false;
        else if (dclock() >= message->earliestTimeToResend) {
            return true;
        } else {
            return false;
        }
        
    }

    // returns true if message needs to be moved to a an incomplete list for further
    // send processing; returns false with errorCode = ULM_SUCCESS, if there is
    // no need to move the descriptor -- and with errorCode = ULM_ERR_BAD_PATH, if
    // the message must be rebound to a new path
    virtual bool resend(SendDesc_t *message, int *errorCode);
#endif      /* ENABLE_RELIABILITY */
};

#endif
