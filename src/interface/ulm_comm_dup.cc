/*
 * Copyright 2002-2004. The Regents of the University of
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

#include "internal/ftoc.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "mpi/constants.h"
#include "ulm/ulm.h"
#include "queue/globals.h"

//
//! duplicate inputContextID, creating outputContextID
//

extern "C" void mpi_null_copy_fn_f(MPI_Fint *, MPI_Fint *, MPI_Fint *,
                                   MPI_Fint *, MPI_Fint *, MPI_Fint *,
                                   MPI_Fint *);

extern "C" int ulm_comm_dup(int inputContextID, int *outputContextID)
{
    int returnValue = MPI_SUCCESS;
    int nAttribs;
    int threadUsage;
    int lGroup, rGroup;
    int i;
    int ndims;
    int topo_type;
    int size_of_attr;
    Communicator *in_comm;
    ULMTopology_t *topology;
    Communicator *out_comm;
    if (ENABLE_CHECK_API_ARGS) {
        if (communicators[inputContextID] == 0L) {
            ulm_err(("Error: ulm_comm_dup: bad communicator %d\n",
                     inputContextID));
            returnValue = MPI_ERR_COMM;
            goto BarrierTag;
        }
    }
    in_comm = communicators[inputContextID];

    // get new contextID
    returnValue = in_comm->getNewContextID(outputContextID);
    if (returnValue != MPI_SUCCESS) {
        goto BarrierTag;
    }
    // get localGroup index, and increment it's reference count
    lGroup = in_comm->localGroup->groupID;

    // increment reference count
    in_comm->localGroup->incrementRefCount(1);

    // get remoteGroup index, and increment it's reference count
    rGroup = in_comm->remoteGroup->groupID;

    // increment reference count
    in_comm->remoteGroup->incrementRefCount(1);

    // for inter_communicators increment localGroupsComm
    if (communicators[inputContextID]->communicatorType ==
        Communicator::INTER_COMMUNICATOR) {
        communicators[in_comm->localGroupsComm]->refCounLock.lock();
        (communicators[in_comm->localGroupsComm]->refCount)++;
        communicators[in_comm->localGroupsComm]->refCounLock.unlock();
        communicators[in_comm->localGroupsComm]->localGroup->
            incrementRefCount(1);
        communicators[in_comm->localGroupsComm]->remoteGroup->
            incrementRefCount(1);
    }
    // setup new Communicator
    threadUsage =
        in_comm->useThreads ? ULM_THREAD_SAFE : ULM_THREAD_UNSAFE;
    returnValue =
        ulm_communicator_alloc(*outputContextID, threadUsage, lGroup,
                               rGroup, false, 0, (int *) NULL,
                               communicators[inputContextID]
                               ->communicatorType, true, 1,
                               in_comm->useSharedMemForCollectives);

    // the communicator out
    out_comm = communicators[*outputContextID];
    // for set localGroupsComm
    if ((in_comm->communicatorType == Communicator::INTER_COMMUNICATOR) &&
        (returnValue == ULM_SUCCESS)) {
        out_comm->localGroupsComm = in_comm->localGroupsComm;

    }
    // replicate errorhandler index
    out_comm->errorHandlerIndex = in_comm->errorHandlerIndex;

    // When a communicator is duped it's topology is also
    // replicated.  so send in the communicator
    if (in_comm->topology != NULL) {
        topology = (ULMTopology_t *) ulm_malloc(sizeof(ULMTopology_t));
        if (topology == (ULMTopology_t *) NULL) {
            returnValue = MPI_ERR_OTHER;
            exit(EXIT_FAILURE);
        }
        topo_type = in_comm->topology->type;
        if (topology) {
            switch (topo_type) {
            case MPI_CART:
                topology->cart.type = in_comm->topology->cart.type;
                topology->cart.nnodes = in_comm->topology->cart.nnodes;
                ndims = topology->cart.ndims =
                    in_comm->topology->cart.ndims;
                topology->cart.dims =
                    (int *) ulm_malloc(ndims * sizeof(int));
                topology->cart.periods =
                    (int *) ulm_malloc(ndims * sizeof(int));
                topology->cart.position =
                    (int *) ulm_malloc(ndims * sizeof(int));
                for (i = 0; i < topology->cart.ndims; i++) {
                    topology->cart.dims[i] =
                        in_comm->topology->cart.dims[i];
                    topology->cart.periods[i] =
                        in_comm->topology->cart.periods[i];
                    topology->cart.position[i] =
                        in_comm->topology->cart.position[i];
                }
                break;
            case MPI_GRAPH:
                topology->graph.type = in_comm->topology->graph.type;
                topology->graph.nnodes = in_comm->topology->graph.nnodes;
                topology->graph.nedges = in_comm->topology->graph.nedges;
                topology->graph.index =
                    (int *) ulm_malloc(topology->graph.nnodes *
                                       sizeof(int));
                topology->graph.edges =
                    (int *) ulm_malloc(topology->graph.nedges *
                                       sizeof(int));
                for (i = 0; i < topology->graph.nnodes; i++)
                    topology->graph.index[i] =
                        in_comm->topology->graph.index[i];

                for (i = 0; i < topology->graph.nedges; i++)
                    topology->graph.edges[i] =
                        in_comm->topology->graph.edges[i];
                break;
            }

        }
        returnValue = ulm_set_topology(*outputContextID, topology);
    }
    //
    // replicate communictor attributes
    //
    // allocate attribute array

    out_comm->attributeCount = 0;
    out_comm->sizeOfAttributeArray = in_comm->sizeOfAttributeArray;
    size_of_attr = in_comm->sizeOfAttributeArray;
    if (size_of_attr > 0) {
        out_comm->attributeList =
            (commAttribute_t *) ulm_malloc(sizeof(commAttribute_t) *
                                           size_of_attr);
        if (!out_comm->attributeList) {
            returnValue = MPI_ERR_NO_SPACE;
            goto BarrierTag;
        }
        for (int i = 0; i < size_of_attr; i++) {
            out_comm->attributeList[i].keyval = -1;
            out_comm->attributeList[i].attributeValue = (void *) 0;
        }
    }

    nAttribs = 0;
    // loop over the attributes of the inputContextID
    for (int attrib = 0; attrib <
         communicators[inputContextID]->sizeOfAttributeArray; attrib++) {
        /* check to see if copy function is null copy function */
        int atr =
            communicators[inputContextID]->attributeList[attrib].keyval;
        if (atr == -1)
            continue;
        bool copyAtribs = false;
        if (attribPool.attributes[atr].setFromFortran == 0) {
            if (attribPool.attributes[atr].cCopyFunction !=
                mpi_null_copy_fn_c)
                copyAtribs = true;
        } else {
            if (attribPool.attributes[atr].fCopyFunction !=
                mpi_null_copy_fn_f)
                copyAtribs = true;
        }
        if (copyAtribs) {
            /* increment attribute reference count */
            attribPool.attributes[atr].Lock.lock();
            attribPool.attributes[atr].refCount++;
            attribPool.attributes[atr].Lock.unlock();

            /* copy data */
            communicators[*outputContextID]->attributeList[nAttribs].
                keyval = atr;
            communicators[*outputContextID]->attributeCount++;

            /* run copy function */
            if (attribPool.attributes[atr].setFromFortran == 1) {
                /* fortran version */
                FortranLogical flag;
                int newAttribute;
                long int longOldAttribute =
                    (long int) communicators[inputContextID]->
                    attributeList[attrib].attributeValue;
                int oldAttribute = (int) longOldAttribute;
                attribPool.attributes[atr].fCopyFunction(&inputContextID,
                                                         &atr,
                                                         (int *)
                                                         attribPool.
                                                         attributes[atr].
                                                         extraState,
                                                         &oldAttribute,
                                                         &newAttribute,
                                                         &flag,
                                                         &returnValue);
                if (returnValue != MPI_SUCCESS) {
                    goto BarrierTag;
                }
                int setAttrib;
                LOGICAL_SET(setAttrib, &flag);
                if (setAttrib) {
                    communicators[*outputContextID]->
                        attributeList[nAttribs].attributeValue =
                        (void *) newAttribute;
                    /* inctement count */
                    nAttribs++;
                } else {
                    communicators[*outputContextID]->
                        attributeList[nAttribs].keyval = -1;
                }
            } else {
                /* c version */
                void *newAttribute;
                int flag;
                returnValue =
                    attribPool.attributes[atr].
                    cCopyFunction(inputContextID, atr,
                                  attribPool.attributes[atr].extraState,
                                  communicators[inputContextID]->
                                  attributeList[attrib].attributeValue,
                                  &newAttribute, &flag);
                if (returnValue != MPI_SUCCESS) {
                    goto BarrierTag;
                }
                if (flag) {
                    communicators[*outputContextID]->
                        attributeList[nAttribs].attributeValue =
                        newAttribute;
                    /* inctement count */
                    nAttribs++;

                } else {
                    communicators[*outputContextID]->
                        attributeList[nAttribs].keyval = -1;
                }
            }


        }
    }

  BarrierTag:

    int tmpreturnValue = returnValue;

    /*
     * block to insure that setup is complete.  For intra-communicators
     * we use a barrier, for inter-communicators, we use barrier -
     * peerExchange() - barrier.
     */
    if (communicators[inputContextID]->communicatorType !=
        Communicator::INTER_COMMUNICATOR) {
        returnValue = ulm_barrier(inputContextID);
        if (returnValue != ULM_SUCCESS) {
            return returnValue;
        }
#ifdef USE_ELAN_COLL
	/* Not all processes have a new communicator generated */
	if ( *outputContextID != MPI_COMM_NULL && tmpreturnValue == ULM_SUCCESS)
	  ulm_alloc_bcaster(*outputContextID, 
	      communicators[inputContextID]->useThreads);
#endif
    } else {
        /* barrier the local group */
        returnValue =
            ulm_barrier(communicators[inputContextID]->localGroupsComm);
        if (returnValue != MPI_SUCCESS) {
            return returnValue;
        }
        /* exchange with remote peer */
        ULMStatus_t recvStatus;
        int tag = Communicator::INTERCOMM_TAG;
        returnValue =
            peerExchange(inputContextID,
                         communicators[inputContextID]->localGroupsComm, 0,
                         0, tag, (void *) NULL, 0,
                         (void *) NULL, 0, &recvStatus);
        if (returnValue != MPI_SUCCESS) {
            return returnValue;
        }
        /* barrier the local group */
        returnValue =
            ulm_barrier(communicators[inputContextID]->localGroupsComm);
        if (returnValue != MPI_SUCCESS) {
            return returnValue;
        }
    }

    returnValue = tmpreturnValue;
    return returnValue;
}
