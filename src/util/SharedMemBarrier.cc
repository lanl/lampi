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
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <strings.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "internal/mmap_params.h"
#include "internal/system.h"
#include "util/SharedMemBarrier.h"

#if defined (__linux__) || defined(__APPLE__)
int BarrierSharedMemory=-1;
char BarrierSMFileName[ULM_MAX_PATH_LEN];
#endif
typedef struct _ulm_Barrier{ int CheckedIn ;
    int ExitFlag;
    int Padding[(CACHE_ALIGNMENT/sizeof(int)) - 2] ;
} _ulm_barrier ;

/* static data */
static volatile _ulm_barrier *BarrierStruct;
static int *MyStartOffset;
static int *StartingRow;
static int HighestRow;
static int ArenaSize;
static int MyStartingRow;
static int MyBarrierRank;
static size_t cnt=0;
static volatile _ulm_barrier *BPtr;
static int NumBarriering;

/*
 * Shared memory binary tree barrier.  Release is done by spinning on
 * a shared variable, becasue this seems to be faster than fanning out
 * after all process have fanned in.  This is a "generic" routine, and
 * does not take advantage of any atomic hardware capabilities.  As
 * such this is not a high performance barrier.
 */
void _ulm_SharedMemBarrier(void)
{
    int up, mask;
    int partner;
    volatile int *Test;
    int Offset = 0;
    cnt++;

    /* alternate data structures */
    mask = (cnt & 1);
    BPtr = BarrierStruct + (mask * ArenaSize / (2 * sizeof(_ulm_barrier)));
    BPtr->ExitFlag = 0;

    /* fan in */
    Offset = 0;
    for (up = HighestRow; up >= MyStartingRow; up--) {
        if (up == 0)
            partner = -1;
        else if (MyBarrierRank < (1 << (up - 1))) {
            partner = MyBarrierRank + (1 << (up - 1));
        } else {
            partner = MyBarrierRank - (1 << (up - 1));
        }
        Test = &((BPtr + (MyBarrierRank + Offset))->CheckedIn);
        *Test = 1;
        if ((partner < NumBarriering) && (partner >= 0)) {
            Test = &((BPtr + (partner + Offset))->CheckedIn);
            while ((*Test) == 0);
            *Test = 0;
        } else {
            *Test = 0;
        }
        Offset += (1 << up);
    }

    if (MyBarrierRank == 0) {
        BPtr->ExitFlag = 1;
    } else {
        Test = &(BPtr->ExitFlag);
        while ((*Test) != 1);
    }
}

/*
 * Set up the shared memory data for the binary tree shared memory
 * barrier.  This must be called before the fork().
 */
void _ulm_BinaryTreeSharedMemBarrierPreForkInit(int TotNum)
{
    int i, j, Flag;
#ifndef __linux__
    int shm_descr;
#endif

    /* set size of barrier group */
    NumBarriering = TotNum;

    StartingRow = (int *) ulm_malloc(sizeof(int) * (TotNum + 1));
    StartingRow[0] = 0;
    MyStartOffset = (int *) ulm_malloc(sizeof(int) * (TotNum + 1));
    MyStartOffset[0] = 0;
    Flag = 0;

    for (i = 0; !Flag; i++) {
        for (j = (1 << i); j < (1 << (i + 1)); j++) {
            MyStartOffset[j] = 1 << i;
            StartingRow[j] = i + 1;
            if (j == (TotNum)) {
                Flag = 1;
                break;
            }
        }
        if (Flag)
            break;
    }
    HighestRow = StartingRow[TotNum - 1];

    ArenaSize = 4 * (1 << HighestRow) * sizeof(_ulm_barrier);

#if defined (__linux__) || defined(__APPLE__)

    bzero(BarrierSMFileName, ULM_MAX_PATH_LEN);
    tmpnam(BarrierSMFileName);
    BarrierSharedMemory =
        open(BarrierSMFileName, O_CREAT | O_RDWR, S_IRWXU);
    int RetVal = ftruncate(BarrierSharedMemory, ArenaSize);
    if (RetVal < 0) {
        printf("Error: in ftruncating SharedMemPool.\n");
        exit(17);
    }
    BarrierStruct =
        (_ulm_barrier *) mmap(0, ArenaSize, MMAP_SHARED_PROT,
                              MMAP_SHARED_FLAGS, BarrierSharedMemory, 0);
    if (BarrierStruct == MAP_FAILED) {
        printf(" Error in mmap\n");
        exit(18);
    }
#elif __osf__
    shm_descr = -1;
    BarrierStruct =
        (_ulm_barrier *) mmap(0, ArenaSize, MMAP_SHARED_PROT,
                              MMAP_SHARED_FLAGS, shm_descr, 0);
    if (BarrierStruct == MAP_FAILED) {
        printf(" Error in mmap\n");
        exit(19);
    }
#else

    shm_descr = open("/dev/zero", O_CREAT | O_RDWR, S_IRWXU);
    BarrierStruct =
        (_ulm_barrier *) mmap(0, ArenaSize, MMAP_SHARED_PROT,
                              MMAP_SHARED_FLAGS, shm_descr, 0);
    if (BarrierStruct == MAP_FAILED) {
        printf(" Error in mmap\n");
        exit(20);
    }
    close(shm_descr);

#endif                          /* LINUX */

    bzero((void *) BarrierStruct, ArenaSize);
}

/*
 * Complete the initialization of private data for the shared memory
 * binary tree barrier routine.  Should be called after the fork()
 */
void _ulm_BinaryTreeSharedMemBarrierPostForkInit(int MyRank)
{
    /* private data */
    int up;

    /* ProcID within the barriering group is set */
    MyBarrierRank = MyRank;

    MyStartingRow = 0;
    if (MyBarrierRank > 0) {
        up = MyBarrierRank;
        while (up > 0) {
            MyStartingRow++;
            up >>= 1;
        }
    }
}

/*
 * Tear down the shared memory data for the binary tree shared memory
 * barrier
 */
void _ulm_BinaryTreeSharedMemBarrierFini(void)
{
#if defined (__linux__) || defined(__APPLE__)
    if (BarrierSharedMemory != -1)
        unlink(BarrierSMFileName);
    BarrierSharedMemory = -1;
#endif
}
