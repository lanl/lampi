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

#include <stdio.h>
#include <unistd.h>
#include "internal/profiler.h"

#include "ulm/ulm.h"
#include "internal/log.h"
#include "queue/globals.h"

#include "path/common/path.h"
#include "path/common/pathContainer.h"
#include "path/common/InitSendDescriptors.h"
#include "path/common/pathGlobals.h"
#ifdef ENABLE_SHARED_MEMORY
#include "path/sharedmem/SMPSharedMemGlobals.h"
#endif /* SHARED_MEMORY */

/*!
 * default - bind a point-to-point send message descriptor to a path object
 *
 * \param message   send message descriptor
 * \return          ULM return code
 */
extern "C" int ulm_bind_pt2pt_message(ULMRequest_t *request, int ctx,int destination)
{
    SendDesc_t *SendDesc;
    int errorCode = ULM_SUCCESS;
    int globalDestID;

    globalDestID =
        communicators[ctx]->remoteGroup->
        mapGroupProcIDToGlobalProcID[destination];

    // use local shared memory first, if at all possible
    if (pathList[globalDestID].useSharedMemory_m >= 0) {
        int useLocal = pathList[globalDestID].useSharedMemory_m;

        /* get element from cache, is available */
        SendDesc =
            (SendDesc_t *) pathArray[useLocal]->sendDescCache.
            GetLastElement();
        if (!SendDesc) {
            /* if not available, get from free list */
            SendDesc = _ulm_SendDescriptors.getElement(0, errorCode);
            if (!SendDesc) {
                return errorCode;
            }
            /* allocate sharedmemSendInfo data */
            SendDesc->pathInfo.sharedmem.sharedData =
                (sharedMemData_t *) SMPSendDescs.
                getElement(getMemPoolIndex(), errorCode);
            if (!(SendDesc->pathInfo.sharedmem.sharedData)) {
                return errorCode;
            }
            /* compute address of first frag */
            size_t offsetToFirstFrag = sizeof(sharedMemData_t);
            SendDesc->pathInfo.sharedmem.firstFrag =
                (SMPFragDesc_t *) ((char *) SendDesc->pathInfo.sharedmem.
                                   sharedData + offsetToFirstFrag);
            /* placement new */
            new(SendDesc->pathInfo.sharedmem.firstFrag)
                SMPFragDesc_t(getMemPoolIndex());

            /* compute address of first frag payload -
             * will never change */
            size_t offsetToPayload =
                (((sizeof(sharedMemData_t) + sizeof(SMPFragDesc_t) -
                   1) / CACHE_ALIGNMENT) + 1) * CACHE_ALIGNMENT;
            void *firstFragPayloadAddr =
                (void *) ((char *) SendDesc->pathInfo.sharedmem.
                          sharedData + offsetToPayload);

            /* set the payload address in the first frag */
            SendDesc->pathInfo.sharedmem.firstFrag->addr_m =
                firstFragPayloadAddr;

            // fill in pointer to send descriptor
            SendDesc->pathInfo.sharedmem.firstFrag->SendingHeader_m.SMP =
                (sharedMemData_t *) SendDesc->pathInfo.sharedmem.
                sharedData;
        }
        pathArray[useLocal]->bind(SendDesc, &globalDestID, 1, &errorCode);
    }
    // use Quadrics as option #2a
    else if (pathList[globalDestID].useQuadrics_m >= 0) {
        int useQuadrics = pathList[globalDestID].useQuadrics_m;
        /* if not available, get from free list */
        SendDesc = _ulm_SendDescriptors.getElement(0, errorCode);
        if (!SendDesc) {
            return errorCode;
        }
        pathArray[useQuadrics]->bind(SendDesc, &globalDestID, 1,
                                     &errorCode);
    }
    // use Myrinet/GM as option #2b
    else if (pathList[globalDestID].useGM_m >= 0) {
        int useGM = pathList[globalDestID].useGM_m;
        /* if not available, get from free list */
        SendDesc = _ulm_SendDescriptors.getElement(0, errorCode);
        if (!SendDesc) {
            return errorCode;
        }
        pathArray[useGM]->bind(SendDesc, &globalDestID, 1, &errorCode);
    }
    // use UDP connectivity as option #3
    else if (pathList[globalDestID].useUDP_m >= 0) {
        int useUDP = pathList[globalDestID].useUDP_m;
        /* if not available, get from free list */
        SendDesc = _ulm_SendDescriptors.getElement(0, errorCode);
        if (!SendDesc) {
            return errorCode;
        }
        pathArray[useUDP]->bind(SendDesc, &globalDestID, 1, &errorCode);
    } else {
        /* not path - error */
        errorCode = ULM_ERR_FATAL;
        ulm_err(("ulm_bind_pt2pt_message error: "
                 "unable to find path to global process ID %d\n",
                 globalDestID));
    }

    *request = (ULMRequest_t) SendDesc;

    return errorCode;
}
