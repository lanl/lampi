/*
 * Copyright 2002-2005. The Regents of the University of
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include "queue/globals.h"
#include "path/gm/path.h"
#include "os/atomic.h"


inline bool gmPath::canReach(int globalDestProcessID)
{
    // return true only for processes not on our "box"

    int destinationHostID;
    int dev;

    destinationHostID = global_proc_to_host(globalDestProcessID);
    if (myhost() != destinationHostID) {
        for (dev = 0; dev < gmState.nDevsAllocated; dev++) {
            if (gmState.localDevList[dev].remoteDevList[globalDestProcessID].node_id
                != (unsigned int) -1) {
                return true;
            }
        }
    }

    return false;
}


bool gmPath::send(SendDesc_t *message, bool *incomplete, int *errorCode)
{
    Group *group;
    gmSendFragDesc *sfd;
    gmSendFragDesc *afd;
    int globalDestProc;
    int nDescsToAllocate;
    int rc;
    int tmapIndex;
    unsigned numSent;
    size_t leftToSend;
    size_t payloadSize;
    size_t offset;
    double timeNow = -1;
    
    assert(message);

    *incomplete = true;
    *errorCode = ULM_SUCCESS;

    group = communicators[message->ctx_m]->remoteGroup;
    globalDestProc = group->mapGroupProcIDToGlobalProcID[message->posted_m.peer_m];

    // always allocate and try to send the first frag

    if (message->NumFragDescAllocated == 0) {
        message->clearToSend_m = true;
    }

    // allocate and initialize fragment descriptors

    offset = gmState.bufSize * message->NumFragDescAllocated;
    leftToSend = message->posted_m.length_m - offset;
    nDescsToAllocate = message->numfrags - message->NumFragDescAllocated;

    while (nDescsToAllocate--) {

        // slow down the send -- always initialize and send the first frag
        if (message->NumFragDescAllocated != 0) {
            if ((gmState.maxOutstandingFrags != -1) &&
                (message->NumFragDescAllocated -
                 message->NumAcked >= (unsigned int) gmState.maxOutstandingFrags)) {
                message->clearToSend_m = false;
            }

            // Request To Send/Clear To Send - always send the first frag
            if (!message->clearToSend_m) {
                break;
            }
        }

        // how much data left?
        payloadSize = (leftToSend < gmState.bufSize) ? leftToSend : gmState.bufSize;

        // get an unused frag descriptor
        if (usethreads()) {
            sfd = gmState.sendFragList.getElement(0, rc);
        } else {
            sfd = gmState.sendFragList.getElementNoLock(0, rc);
        }
        if (rc != ULM_SUCCESS) {
            // try to make progress here...
            if ((rc == ULM_ERR_OUT_OF_RESOURCE) || (rc == ULM_ERR_FATAL)) {
                *errorCode = rc;
                return false;
            } else {
                // clean-up here...
                break;
            }
        }

        // calculate tmapIndex for non-zero non-contiguous data
        if (message->datatype == NULL || message->datatype->layout == CONTIGUOUS) {
            // an invalid array index is used to indicate that no packedData buffer
            // is needed
            tmapIndex = -1;
        } else {
            int dtype_cnt = offset / message->datatype->packed_size;
            size_t data_copied = dtype_cnt * message->datatype->packed_size;
            ssize_t data_remaining = (ssize_t)(offset - data_copied);
            tmapIndex = message->datatype->num_pairs - 1;
            for (int ti = 0; ti < message->datatype->num_pairs; ti++) {
                if (message->datatype->type_map[ti].seq_offset == data_remaining) {
                    tmapIndex = ti;
                    break;
                } else if (message->datatype->type_map[ti].seq_offset > data_remaining) {
                    tmapIndex = ti - 1;
                    break;
                }
            }
        }

        // initialize descriptor...does almost everything if it can...
        sfd->init(globalDestProc,
                  tmapIndex,
                  message,
                  offset,
                  payloadSize);

        // put on the FragsToSend list -- may not be able to send immediately
        sfd->WhichQueue = GMFRAGSTOSEND;
        message->FragsToSend.AppendNoLock(sfd);
        (message->NumFragDescAllocated)++;

        // update offset and leftToSend
        leftToSend -= payloadSize;
        offset += payloadSize;
    }


    // send fragments if they are ready; otherwise, try to finish their initialization

    // We use a local numSent to mean how many fragments in
    // FragsToSend were able to start sending from
    // gm_send_with_callback().  We can't use message->NumSent
    // because its value means the number of frags whose send
    // completed.  However, GM does not complete a send until it
    // calls the callback function.

    if ((timeNow < 0) && message->FragsToSend.size()) {
        timeNow = dclock();
    }

    numSent = 0;
    for (sfd = (gmSendFragDesc *) message->FragsToSend.begin();
         sfd != (gmSendFragDesc *) message->FragsToSend.end();
         sfd = (gmSendFragDesc *) sfd->next) {

        bool OKToSend = true;

        if (!sfd->initialized()) {
            OKToSend = sfd->init();
        }

        if (OKToSend) {

            // only send if we have an implicit send token
            if (usethreads()) {
                gmState.localDevList[sfd->dev_m].Lock.lock();
            }
            
            if (gmState.localDevList[sfd->dev_m].sendTokens) {
                gmState.localDevList[sfd->dev_m].sendTokens--;
            } else {
                if (usethreads()) {
                    gmState.localDevList[sfd->dev_m].Lock.unlock();
                }
                continue;
            }
            
            if (usethreads()) {
                gmState.localDevList[sfd->dev_m].Lock.unlock();
            }

            void *addr = &(((gmFragBuffer *) sfd->currentSrcAddr_m)->header);

            if (ENABLE_RELIABILITY) {
                sfd->initResendInfo(message, timeNow);
            }

            if (usethreads()) {
                gmState.gmLock.lock();
            }

            gm_send_with_callback(sfd->gmPort(),
                                  addr,
                                  gmState.log2Size,
                                  sfd->length_m + sizeof(gmHeader),
                                  GM_LOW_PRIORITY,
                                  sfd->gmDestNodeID(),
                                  sfd->gmDestPortID(),
                                  callback,
                                  (void *) sfd);
            if (usethreads()) {
                gmState.gmLock.unlock();
            }

            // switch frag to ack list and remove from send list
            afd = sfd;
            afd->WhichQueue = GMFRAGSTOACK;
            sfd = (gmSendFragDesc *) message->FragsToSend.RemoveLinkNoLock(afd);
            message->FragsToAck.AppendNoLock(afd);
            numSent++;
        }
    }

    if (message->messageDone == REQUEST_INCOMPLETE) {
        if ((numSent + message->NumSent) == message->numfrags &&
            message->sendType != ULM_SEND_SYNCHRONOUS) {
            message->messageDone = REQUEST_COMPLETE;
        }
    }

    if ((numSent + message->NumSent) == message->numfrags) {
        *incomplete = false;
    }

    return true;
}


bool gmPath::receive(double timeNow, int *errorCode, recvType recvTypeArg = ALL)
{
    int i, rc;
    bool HeaderOK;
    unsigned int checksum;

    *errorCode = ULM_SUCCESS;

    // check each Myrinet adapter
    for (i = 0; i < gmState.nDevsAllocated; i++) {
        bool keepChecking = true;
        gm_recv_event_t *event;
        gmRecvFragDesc *rf = 0;
        localDevInfo_t *devInfo = &(gmState.localDevList[i]);

        while (keepChecking) {

            // grab receive descriptor
            if (!rf) {
                rf = gmState.recvFragList.getElement(0, rc);
                if (rc != ULM_SUCCESS) {
                    if ((rc == ULM_ERR_OUT_OF_RESOURCE)
                        || (rc == ULM_ERR_FATAL)) {
                        *errorCode = rc;
                        return false;
                    }
                    rf = 0;
                    break;
                }
                rf->Init();
            }

            if (usethreads()) {
                gmState.gmLock.lock();
            }
            event = gm_receive(devInfo->gmPort);
            if (usethreads()) {
                gmState.gmLock.unlock();
            }

            switch (GM_RECV_EVENT_TYPE(event)) {
            case GM_NO_RECV_EVENT:
                keepChecking = false;
                break;
            case GM_RECV_TOKEN_VIOLATION_EVENT:
                ulm_err(("Process rank  %d (%s): GM error: GM_RECV_TOKEN_VIOLATION_EVENT.\n",
                         myproc(), mynodename()));
                break;
            case GM_BAD_RECV_TOKEN_EVENT:
                ulm_err(("Process rank  %d (%s): GM error: GM_BAD_RECV_TOKEN_EVENT.\n",
                         myproc(), mynodename()));
                break;
            case GM_BAD_SEND_DETECTED_EVENT:
                ulm_err(("Process rank  %d (%s): GM error: GM_BAD_SEND_DETECTED_EVENT.\n",
                         myproc(), mynodename()));
                break;
            case GM_SEND_TOKEN_VIOLATION_EVENT:
                ulm_err(("Process rank  %d (%s): GM error: GM_SEND_TOKEN_VIOLATION_EVENT.\n",
                         myproc(), mynodename()));
                break;
            case GM_RECV_EVENT:
                // increment implicit # of receive tokens
                if (usethreads()) {
                    fetchNadd((volatile int *) &(devInfo->recvTokens), 1);
                } else {
                    (devInfo->recvTokens)++;
                }

                rf->gmFragBuffer_m =
                    *(gmFragBuffer **) ((unsigned char *)
                                        gm_ntohp(event->recv.buffer) -
                                        sizeof(gmFragBuffer *));

                // copy and calculate checksum...if wanted...
                checksum = 0;
                rf->gmHeader_m = (gmHeader *) gm_ntohp(event->recv.buffer);

                if (DEBUG_INSERT_ARTIFICIAL_HEADER_CORRUPTION) {
                    static int first = 1;
                    int frequency = 10000;
                    int test;

                    if (first) {
                        srand((unsigned int) myproc() * 1021 + 79);
                        first = 0;
                    }
                    test = 1 + (int) ((double) frequency * rand() / (RAND_MAX + 1.0));
                    if ((test % frequency) == 0) {
                        ulm_err(("Inserting artificial header corruption\n"));
                        rf->gmHeader_m->data.checksum |= 0xA4A4;
                    }
                }

                if (ENABLE_RELIABILITY && gmState.doChecksum) {
                    checksum = BasePath_t::headerChecksum(rf->gmHeader_m,
                                                          sizeof(gmHeader),
                                                          GM_HDR_WORDS);
                }
                
                rf->addr_m = (void *) ((unsigned char *)
                                       gm_ntohp(event->recv.buffer) +
                                       sizeof(gmHeader));
                rf->dev_m = i;
                rf->length_m =
                    gm_ntoh_u32(event->recv.length) - sizeof(gmHeader);

                if (!gmState.doChecksum) {
                    HeaderOK = true;
                } else if (usecrc()) {
                    if (checksum == 0) {
                        HeaderOK = true;
                    } else {
                        HeaderOK = false;
                    }
                } else {
                    if (checksum ==                       
                        (rf->gmHeader_m->data.checksum +
                         rf->gmHeader_m->data.checksum)) {
                        HeaderOK = true;
                    } else {
                        HeaderOK = false;
                    }
                }                        

                if (HeaderOK) {

                    switch (rf->gmHeader_m->common.type) {
                    case MESSAGE_DATA:
                        rf->msgData(timeNow);
                        break;
                    case MESSAGE_DATA_ACK:
                        rf->msgDataAck(timeNow);
                        break;
                    default:
                        ulm_warn(("gmPath::receive() message of unknown type %d\n",
                                  rf->gmHeader_m->common.type));
                        break;
                    }
                    rf = 0;
                    break;

                } else {

                    unsigned int received_checksum;

                    if (usecrc()) {
                        received_checksum = rf->gmHeader_m->data.checksum;
                    } else {
                        received_checksum = 2 * rf->gmHeader_m->data.checksum;
                    }

                    ulm_err(("Warning: Corrupt fragment header received "
                             "[rank %d --> rank %d (%s)]: "
                             "%s received=0x%x, calculated=0x%x\n", 
                             rf->gmHeader_m->data.senderID, myproc(), mynodename(),
                             usecrc() ? "CRC" : "checksum",
                             received_checksum, checksum));

                    if (ENABLE_RELIABILITY && gmState.doAck) {
                        // Return descriptor to the pool and
                        // proceed. We don't send a NACK but the data
                        // will be sent again after timeout :)
                        ulm_err(("Warning: Waiting for fragment to be retransmitted.\n"));
                        rf->ReturnDescToPool(0);
                        rf = 0;
                        continue;
                    } else {
                        ulm_exit(("Error: Cannot recover\n"));
                    }
                }

            default:
                if (usethreads()) {
                    gmState.gmLock.lock();
                }
                gm_unknown(devInfo->gmPort, event);
                if (usethreads()) {
                    gmState.gmLock.unlock();
                }
                break;
            }

        }                       // end while (keepChecking)...

        // free any additional unused gmRecvFragDesc
        if (rf) {
            rf->ReturnDescToPool(0);
            rf = 0;
        }
        // feed GM all the buffers it can take since we
        // won't be back for an indeterminate amount of time
        if (devInfo->recvTokens) {
            if (usethreads())
                devInfo->Lock.lock();

            while (devInfo->recvTokens) {
                // get another buffer
                gmFragBuffer *buf =
                    devInfo->bufList.getElementNoLock(0, rc);
                if (rc != ULM_SUCCESS) {
                    break;
                }
                // give it to GM...
                buf->me = buf;
                void *addr = &(buf->header);
                if (usethreads()) {
                    gmState.gmLock.lock();
                }

                gm_provide_receive_buffer_with_tag(devInfo->gmPort,
                                                   addr,
                                                   gmState.log2Size,
                                                   GM_LOW_PRIORITY, i);
                if (usethreads())
                    gmState.gmLock.unlock();

                // decrement recvTokens
                (devInfo->recvTokens)--;

            }
            if (usethreads()) {
                devInfo->Lock.unlock();
            }
        }
    }                           // end for loop

    return true;
}


void gmPath::callback(struct gm_port *port,
                      void *context,
                      gm_status_t status)
{
    gmSendFragDesc *sfd = (gmSendFragDesc *) context;
    SendDesc_t *bsd = (SendDesc_t *) sfd->parentSendDesc_m;

    if (usethreads()) {
        gmState.localDevList[sfd->dev_m].Lock.lock();
    }

    gmState.localDevList[sfd->dev_m].sendTokens++;

    if (usethreads()) {
        gmState.localDevList[sfd->dev_m].Lock.unlock();
    }

    switch (status) {
    case GM_TRY_AGAIN:
    case GM_BUSY:
    case GM_SEND_TIMED_OUT:
        // try again if the receiver is busy 
        ulm_err(("Warning: Myrinet/GM: Retrying send: %d (%s) -> %d (status = %d)\n",
                 myproc(), mynodename(), sfd->globalDestProc_m, status));
        bsd->FragsToSend.Append(sfd);
        return;
    case GM_SUCCESS:
        break;
    default:
        // otherwise fail if there was a failure 
        if (sfd) {
            ulm_exit(("Error: Myrinet/GM: %s (%d). "
                      "Trying to send from rank %d (%s) to rank %d.\n",
                      gm_strerror(status),
                      (int) status, myproc(), mynodename(), sfd->globalDestProc_m));
        } else {
            ulm_exit(("Error: Myrinet/GM: %s (%d). "
                      "Trying to send from rank %d (%s).\n",
                      gm_strerror(status), (int) status, myproc(), mynodename()));
        }
    }

    // Register frag as acked, if this is not a synchronous message
    // first fragment.  If it is, we need to wait for a fragment ACK
    // upon matching the message.
    if (usethreads()) {
        bsd->Lock.lock();
    }

    (bsd->NumSent)++;

    sfd->setSendDidComplete(true);
    if ((bsd->sendType == ULM_SEND_SYNCHRONOUS) && (sfd->seqOffset_m == 0)) {
        if (sfd->didReceiveAck()) {
            sfd->freeResources(dclock(), bsd);
        }
    } else {
        if (false == gmState.doAck) {
            (bsd->NumAcked)++;
            sfd->freeResources(dclock(), bsd);
        } else if (sfd->didReceiveAck()) {
            sfd->freeResources(dclock(), bsd);
        }
    }

    if (usethreads()) {
        bsd->Lock.unlock();
    }
}
