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

#include "init/init.h"
#include "path/common/pathContainer.h"
#include "path/sharedmem/SMPSharedMemGlobals.h"
#include "path/sharedmem/path.h"
#include "path/sharedmem/SMPfns.h"


void lampi_init_prefork_shared_memory(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_shared_memory");
    }

    if (ENABLE_SHARED_MEMORY) {
        InitSMPSharedMemDescriptors(s->local_size);
    }
}


void lampi_init_postfork_shared_memory(lampiState_t *s)
{
    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_postfork_shared_memory");
    }

    if (ENABLE_SHARED_MEMORY) {

        /*
         * Add Shared_Memory path to global pathContainer
         */

        int pathHandle;
        sharedmemPath *pathAddr;

        if (!s->iAmDaemon) {
            /*
             * Add UDP path to global pathContainer
             */

            pathAddr = (sharedmemPath *) (pathContainer()->add(&pathHandle));
            if (!pathAddr) {
                s->error = ERROR_LAMPI_INIT_PATH_CONTAINER;
                return;
            }
            new(pathAddr) sharedmemPath;
            pathContainer()->activate(pathHandle);
        }

    }
}


void lampi_init_prefork_receive_setup_msg_shared_memory(lampiState_t *s)
{
    int done;
    int errorCode; 
    int recvd;
    int tag;

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_params_shared_memory");
    }

    /*
     * number of bytes per process for the shared memory descriptor
     * pool RUNPARAMS exchange read the start of input parameters tag
     */
    if (false ==
        s->client->scatterv(-1, dev_type_params::START_SHARED_MEM_INPUT,
                            &errorCode)) {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_SHARED_MEMORY;
        return;
    }

    done = 0;
    while (!done) {
        /* read next tag */
        recvd = s->client->nextTag(&tag);
        if (recvd != adminMessage::OK) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_SHARED_MEMORY;
            return;
        }
        switch (tag) {
        case dev_type_params::END_SHARED_MEM_INPUT:
            /* done reading input params */
            done = 1;
            break;
        case shared_mem_intput_params::SMDESCPOOLBYTESPERPROC:
            s->client->unpackMessage(&bytesPerProcess,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;
            // number of pages of sharem memory per process for SMP messaging
        case shared_mem_intput_params::SMPSMPAGESPERPROC:
            s->client->unpackMessage(&NSMPSharedMemPagesPerProc,
                                     (adminMessage::packType) sizeof(int),
                                     1);
            break;
        case shared_mem_intput_params::SMPFRAGOUTOFRESRCABORT:
            SMPRecvDescAbortWhenNoResource = false;
            break;
        case shared_mem_intput_params::SMPFRAGRESOURCERETRY:
            s->client->unpackMessage(&maxSMPRecvDescRetries,
                                     (adminMessage::packType) sizeof(long),
                                     1);
            break;
        case shared_mem_intput_params::SMPFRAGMINPAGESPERCTX:
            s->client->unpackMessage(&minPgsIn1SMPRecvDescList,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPFRAGMAXPAGESPERCTX:
            s->client->unpackMessage(&maxPgsIn1SMPRecvDescList,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPFRAGMAXTOTPAGES:
            s->client->unpackMessage(&nSMPRecvDescPages,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPISENDOUTOFRESRCABORT:
            SMPISendDescAbortWhenNoResource = false;
            break;
        case shared_mem_intput_params::SMPISENDRESOURCERETRY:
            s->client->unpackMessage(&maxSMPISendDescRetries,
                                     (adminMessage::packType) sizeof(long),
                                     1);
            break;
        case shared_mem_intput_params::SMPISENDMINPAGESPERCTX:
            s->client->unpackMessage(&minPgsIn1SMPISendDescList,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPISENDMAXPAGESPERCTX:
            s->client->unpackMessage(&maxPgsIn1SMPISendDescList,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPISENDMAXTOTPAGES:
            s->client->unpackMessage(&nSMPISendDescPages,
                                     (adminMessage::
                                      packType) sizeof(ssize_t), 1);
            break;
        default:
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_SHARED_MEMORY;
            return;
        }
    }                           /* end while loop */
}


void lampi_init_prefork_receive_setup_params_shared_memory(lampiState_t *s)
{
    int done;
    int errorCode; 
    int recvd;
    int tag;

#if ENABLE_CT
    lampi_init_prefork_receive_setup_msg_shared_memory(s);
    return;
#endif

    if (s->error) {
        return;
    }
    if (s->verbose) {
        lampi_init_print("lampi_init_prefork_receive_setup_params_shared_memory");
    }

    /*
     * number of bytes per process for the shared memory descriptor
     * pool RUNPARAMS exchange read the start of input parameters tag
     */
    recvd = s->client->receive(-1, &tag, &errorCode);
    if ((recvd == adminMessage::OK) &&
        (tag == dev_type_params::START_SHARED_MEM_INPUT)) {
    } else {
        s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_SHARED_MEMORY;
        return;
    }

    done = 0;
    while (!done) {
        /* read next tag */
        recvd = s->client->receive(-1, &tag, &errorCode);
        if (recvd != adminMessage::OK) {
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_SHARED_MEMORY;
            return;
        }
        switch (tag) {
        case dev_type_params::END_SHARED_MEM_INPUT:
            /* done reading input params */
            done = 1;
            break;
        case shared_mem_intput_params::SMDESCPOOLBYTESPERPROC:
            s->client->unpack(&bytesPerProcess,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;
            // number of pages of sharem memory per process for SMP messaging
        case shared_mem_intput_params::SMPSMPAGESPERPROC:
            s->client->unpack(&NSMPSharedMemPagesPerProc,
                              (adminMessage::packType) sizeof(int), 1);
            break;
        case shared_mem_intput_params::SMPFRAGOUTOFRESRCABORT:
            SMPRecvDescAbortWhenNoResource = false;
            break;
        case shared_mem_intput_params::SMPFRAGRESOURCERETRY:
            s->client->unpack(&maxSMPRecvDescRetries,
                              (adminMessage::packType) sizeof(long), 1);
            break;
        case shared_mem_intput_params::SMPFRAGMINPAGESPERCTX:
            s->client->unpack(&minPgsIn1SMPRecvDescList,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPFRAGMAXPAGESPERCTX:
            s->client->unpack(&maxPgsIn1SMPRecvDescList,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPFRAGMAXTOTPAGES:
            s->client->unpack(&nSMPRecvDescPages,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPISENDOUTOFRESRCABORT:
            SMPISendDescAbortWhenNoResource = false;
            break;
        case shared_mem_intput_params::SMPISENDRESOURCERETRY:
            s->client->unpack(&maxSMPISendDescRetries,
                              (adminMessage::packType) sizeof(long), 1);
            break;
        case shared_mem_intput_params::SMPISENDMINPAGESPERCTX:
            s->client->unpack(&minPgsIn1SMPISendDescList,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPISENDMAXPAGESPERCTX:
            s->client->unpack(&maxPgsIn1SMPISendDescList,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;
        case shared_mem_intput_params::SMPISENDMAXTOTPAGES:
            s->client->unpack(&nSMPISendDescPages,
                              (adminMessage::packType) sizeof(ssize_t), 1);
            break;
        default:
            s->error = ERROR_LAMPI_INIT_RECEIVE_SETUP_PARAMS_SHARED_MEMORY;
            return;
        }
    }                           /* end while loop */
}
