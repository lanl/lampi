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

#include "queue/Communicator.h"
#include "internal/profiler.h"
#include "internal/state.h"
#include "ulm/ulm.h"

//
// Check to see if a frag has arrived that matches a given tag and
// source.
//
int Communicator::iprobe(int sourceProc, int tag,
                         int *found, ULMStatus_t *status)
{
    int errorCode = ULM_SUCCESS;

    *found = 0;

    //
    // Pull in any outstanding messages before we look through things to
    // see if we can find a match.
    //
    errorCode = ulm_make_progress();
    if (errorCode == ULM_ERR_OUT_OF_RESOURCE || errorCode == ULM_ERR_FATAL)
        return errorCode;

    //
    // wild source and wild tag
    //
    if (sourceProc == ULM_ANY_PROC && tag == ULM_ANY_TAG) {
        // loop over all source queues   !!!!!! change this to look only at list with data
        for (int SrcIndex = 0; SrcIndex < remoteGroup->groupSize;
             SrcIndex++) {
#ifdef SHARED_MEMORY
            if (lampiState.map_global_rank_to_host
                [remoteGroup->mapGroupProcIDToGlobalProcID[SrcIndex]] ==
                myhost()) {
                // if no frags - continue
                if (privateQueues.OkToMatchSMPFrags[SrcIndex]->size() == 0)
                    continue;
                //
                // Get the list of frags from the given processor.
                //

                // lock list for thread safety, since we need to make it through
                //   the full list
                if (usethreads())
                    privateQueues.OkToMatchSMPFrags[SrcIndex]->Lock.lock();

                for (SMPFragDesc_t *
                         RecDesc = (SMPFragDesc_t *) privateQueues.
                         OkToMatchSMPFrags[SrcIndex]->begin();
                     RecDesc != (SMPFragDesc_t *) privateQueues.
                         OkToMatchSMPFrags[SrcIndex]->end();
                     RecDesc = (SMPFragDesc_t *) RecDesc->next) {
                    // do not match on a negative tag for a ULM_ANY_TAG probe
                    if (RecDesc->tag_m < 0) {
                        continue;
                    }
                    //
                    // Any message will do
                    //
                    *found = 1;
                    status->proc.source = SrcIndex;
                    status->tag = RecDesc->tag_m;
                    status->matchedSize = RecDesc->msgLength_m;

                    //
                    // And get out of here.
                    //
                    break;
                }

                // unlock list
                if (usethreads())
                    privateQueues.OkToMatchSMPFrags[SrcIndex]->Lock.
                        unlock();

                if (*found)
                    break;
            } else {
#endif                          // SHARED_MEMORY
                // if no frags - continue
                if (privateQueues.OkToMatchRecvFrags[SrcIndex]->size() ==
                    0)
                    continue;
                //
                // Get the list of frags from the given processor.
                //

                // lock list for thread safety, since we need to make it through
                //   the full list
                privateQueues.OkToMatchRecvFrags[SrcIndex]->Lock.lock();

                for (BaseRecvFragDesc_t *
                         RecDesc =
                         (BaseRecvFragDesc_t *) privateQueues.
                         OkToMatchRecvFrags[SrcIndex]->begin();
                     RecDesc !=
                         (BaseRecvFragDesc_t *) privateQueues.
                         OkToMatchRecvFrags[SrcIndex]->end();
                     RecDesc = (BaseRecvFragDesc_t *) RecDesc->next) {
                    // do not match on a negative tag for a ULM_ANY_TAG probe
                    if (RecDesc->tag_m < 0) {
                        continue;
                    }
                    //
                    // Any message will do
                    //
                    *found = 1;
                    status->proc.source = SrcIndex;
                    status->tag = RecDesc->tag_m;
                    status->matchedSize = RecDesc->msgLength_m;

                    //
                    // And get out of here.
                    //
                    break;
                }

                // unlock list
                privateQueues.OkToMatchRecvFrags[SrcIndex]->Lock.unlock();
                if (*found)
                    break;
#ifdef SHARED_MEMORY
            }                   // end on host/off host test
#endif                          // SHARED_MEMORY
        }                       // end loop

    }                           // end if both SourceProc and tag wild
    //
    // wild source specific tag
    //
    else if (sourceProc == ULM_ANY_PROC) {
        long Tag = tag;
        // loop over all source queues
        for (int SrcIndex = 0; SrcIndex < remoteGroup->groupSize;
             SrcIndex++) {
            // if no frags - continue    !!!! look at better way to do this
#ifdef SHARED_MEMORY
            if (lampiState.map_global_rank_to_host
                [remoteGroup->mapGroupProcIDToGlobalProcID[SrcIndex]] ==
                myhost()) {
                // if no frags - continue
                if (privateQueues.OkToMatchSMPFrags[SrcIndex]->size() == 0)
                    continue;
                //
                // Get the list of frags from the given processor.
                //

                // lock list for thread safety
                if (usethreads())
                    privateQueues.OkToMatchSMPFrags[SrcIndex]->Lock.lock();

                for (SMPFragDesc_t *
                         RecDesc = (SMPFragDesc_t *) privateQueues.
                         OkToMatchSMPFrags[SrcIndex]->begin();
                     RecDesc != (SMPFragDesc_t *) privateQueues.
                         OkToMatchSMPFrags[SrcIndex]->end();
                     RecDesc = (SMPFragDesc_t *) RecDesc->next) {
                    // look for matching tag
                    if (Tag == RecDesc->tag_m) {
                        *found = 1;

                        status->proc.source = SrcIndex;
                        status->tag = RecDesc->tag_m;
                        status->matchedSize = RecDesc->msgLength_m;

                        //
                        // And get out of here.
                        //
                        break;
                    }
                }

                // lock list for thread safety
                if (usethreads())
                    privateQueues.OkToMatchSMPFrags[SrcIndex]->Lock.
                        unlock();

                if (*found)
                    break;
            } else {
#endif                          // SHARED_MEMORY
                // if no frags - continue
                if (privateQueues.OkToMatchRecvFrags[SrcIndex]->size() ==
                    0)
                    continue;
                //
                // Get the list of frags from the given processor.
                //

                // lock list for thread safety
                privateQueues.OkToMatchRecvFrags[SrcIndex]->Lock.lock();

                for (BaseRecvFragDesc_t *
                         RecDesc =
                         (BaseRecvFragDesc_t *) privateQueues.
                         OkToMatchRecvFrags[SrcIndex]->begin();
                     RecDesc !=
                         (BaseRecvFragDesc_t *) privateQueues.
                         OkToMatchRecvFrags[SrcIndex]->end();
                     RecDesc = (BaseRecvFragDesc_t *) RecDesc->next) {
                    // look for matching tag
                    if (Tag == RecDesc->tag_m) {
                        *found = 1;

                        status->proc.source = SrcIndex;
                        status->tag = RecDesc->tag_m;
                        status->matchedSize = RecDesc->msgLength_m;

                        //
                        // And get out of here.
                        //
                        break;
                    }
                }

                // lock list for thread safety
                privateQueues.OkToMatchRecvFrags[SrcIndex]->Lock.unlock();

                if (*found)
                    break;
#ifdef SHARED_MEMORY
            }                   // end on-host/off-host loop
#endif                          // SHARED_MEMORY
        }                       // end loop

    }                           // end wild source
    //
    // specific source wild tag
    //
    else if (tag == ULM_ANY_TAG) {
        //
        // Make sure the input makes sense.
        //
        assert((sourceProc >= 0) && (sourceProc < remoteGroup->groupSize));
        //
        // Loop over the frags in that list.  We won't need to delete
        // any elements, so don't bother keeping a pointer to the previous
        // node.
        //
        int SrcIndex = sourceProc;

#ifdef SHARED_MEMORY
        if (lampiState.map_global_rank_to_host
            [remoteGroup->mapGroupProcIDToGlobalProcID[SrcIndex]] ==
            myhost()) {
            // lock list for thread safety, since we need to make it through
            //   the full list
            if (usethreads())
                privateQueues.OkToMatchSMPFrags[SrcIndex]->Lock.lock();

            for (SMPFragDesc_t *
                     RecDesc = (SMPFragDesc_t *) privateQueues.
                     OkToMatchSMPFrags[SrcIndex]->begin();
                 RecDesc != (SMPFragDesc_t *) privateQueues.
                     OkToMatchSMPFrags[SrcIndex]->end();
                 RecDesc = (SMPFragDesc_t *) RecDesc->next) {
                // do not match on a negative tag for a ULM_ANY_TAG probe
                if (RecDesc->tag_m < 0) {
                    continue;
                }
                //
                // Record that we found one.
                //
                *found = 1;

                status->proc.source = SrcIndex;
                status->tag = RecDesc->tag_m;
                status->matchedSize = RecDesc->msgLength_m;

                //
                // And get out of here.
                //
                break;
            }
            // unlock list
            if (usethreads())
                privateQueues.OkToMatchSMPFrags[SrcIndex]->Lock.unlock();
        } else {
#endif                          // SHARED_MEMORY
            // lock list for thread safety, since we need to make it through
            //   the full list
            privateQueues.OkToMatchRecvFrags[SrcIndex]->Lock.lock();

            for (BaseRecvFragDesc_t *
                     RecDesc =
                     (BaseRecvFragDesc_t *) privateQueues.
                     OkToMatchRecvFrags[SrcIndex]->begin();
                 RecDesc !=
                     (BaseRecvFragDesc_t *) privateQueues.
                     OkToMatchRecvFrags[SrcIndex]->end();
                 RecDesc = (BaseRecvFragDesc_t *) RecDesc->next) {
                // do not match on a negative tag for a ULM_ANY_TAG probe
                if (RecDesc->tag_m < 0) {
                    continue;
                }
                //
                // Record that we found one.
                //
                *found = 1;

                status->proc.source = SrcIndex;
                status->tag = RecDesc->tag_m;
                status->matchedSize = RecDesc->msgLength_m;

                //
                // And get out of here.
                //
                break;
            }
            // unlock list
            privateQueues.OkToMatchRecvFrags[SrcIndex]->Lock.unlock();

#ifdef SHARED_MEMORY
        }                       // end on-host/off-host
#endif                          // SHARED_MEMORY
    }                           // end wild tag

    // specific source and specific tag
    else {

        //
        // Make sure the input makes sense.
        //
        assert(tag >= 0);
        assert((sourceProc >= 0) && (sourceProc < remoteGroup->groupSize));

        int SrcIndex = sourceProc;
#ifdef SHARED_MEMORY
        if (lampiState.map_global_rank_to_host
            [remoteGroup->mapGroupProcIDToGlobalProcID[SrcIndex]] ==
            myhost()) {
            // lock list for thread safety, since we need to make it through
            //   the full list
            if (usethreads())
                privateQueues.OkToMatchSMPFrags[SrcIndex]->Lock.lock();

            for (SMPFragDesc_t *
                     RecDesc = (SMPFragDesc_t *) privateQueues.
                     OkToMatchSMPFrags[SrcIndex]->begin();
                 RecDesc != (SMPFragDesc_t *) privateQueues.
                     OkToMatchSMPFrags[SrcIndex]->end();
                 RecDesc = (SMPFragDesc_t *) RecDesc->next) {
                //
                // If the tags match, then we have a match, since we're looking
                // through messages that all match the source.
                //
                if (tag == RecDesc->tag_m) {
                    //
                    //
                    // Record that we found one.
                    //
                    *found = 1;
                    status->proc.source = SrcIndex;
                    status->tag = RecDesc->tag_m;
                    status->matchedSize = RecDesc->msgLength_m;

                    //
                    // And get out of here.
                    //
                    break;
                }
            }

            // unlock list
            if (usethreads())
                privateQueues.OkToMatchSMPFrags[SrcIndex]->Lock.unlock();
        } else {
#endif                          // SHARED_MEMORY
            // lock list for thread safety, since we need to make it through
            //   the full list
            privateQueues.OkToMatchRecvFrags[SrcIndex]->Lock.lock();

            for (BaseRecvFragDesc_t *
                     RecDesc =
                     (BaseRecvFragDesc_t *) privateQueues.
                     OkToMatchRecvFrags[SrcIndex]->begin();
                 RecDesc !=
                     (BaseRecvFragDesc_t *) privateQueues.
                     OkToMatchRecvFrags[SrcIndex]->end();
                 RecDesc = (BaseRecvFragDesc_t *) RecDesc->next) {
                //
                // If the tags match, then we have a match, since we're looking
                // through messages that all match the source.
                //
                if (tag == RecDesc->tag_m) {
                    //
                    //
                    // Record that we found one.
                    //
                    *found = 1;
                    status->proc.source = SrcIndex;
                    status->tag = RecDesc->tag_m;
                    status->matchedSize = RecDesc->msgLength_m;

                    //
                    // And get out of here.
                    //
                    break;
                }
            }

            // unlock list
            privateQueues.OkToMatchRecvFrags[SrcIndex]->Lock.unlock();

#ifdef SHARED_MEMORY
        }                       // end on-host/off-host
#endif                          // SHARED_MEMORY
    }                           // end specific tag and SourceProc

    status->error = errorCode;
    return errorCode;
}
