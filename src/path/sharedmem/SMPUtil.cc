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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ulm/ulm.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "os/numa.h"

// allocate payload buffer
void *allocPayloadBuffer(SMPSharedMem_logical_dev_t *dev,
                         unsigned long length, int *errorCode,
                         int memPoolIndex)
{
    *errorCode = ULM_SUCCESS;

    void *payloadBuffer = dev->MemoryBuckets.ULMMalloc(length);
    if (payloadBuffer == ((void *) -1L)) {

        // get bucket index
        int BucketIndex = dev->MemoryBuckets.GetBucketIndex(length);

        // try and lock the device
        if (dev->lock_m.trylock() == 1) {
            size_t lenData;
            // request chunk from pool
            void *tmpPtr =
                dev->GetChunkOfMemory(BucketIndex, &lenData, errorCode);

            // no chunk available
            if (tmpPtr == (void *) (-1L)) {
#ifdef _DEBUGQUEUES
                ulm_err(("Error: Can't add memory to SMP data pool\n"));
#endif                          /* _DEBUGQUEUES */
                if ((*errorCode) == ULM_ERR_OUT_OF_RESOURCE) {
                    dev->lock_m.unlock();
                    return payloadBuffer;
                }
            } else {
                //  memory locality
                if (!setAffinity(tmpPtr, lenData, memPoolIndex)) {
                    *errorCode = ULM_ERROR;
                    dev->lock_m.unlock();
                    return payloadBuffer;
                }
                // if chunk is available - push onto stack
                dev->PushChunkOnStack(BucketIndex, (char *) tmpPtr);
            }
            // unlock device
            dev->lock_m.unlock();
        }                       // end lock region
    }                           // end  payloadBuffer == ((void *)-1L)

    return payloadBuffer;
}
