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



#include <stdio.h>

#include "internal/new.h"
#include "util/dclock.h"
#include "init/init.h"
#include "queue/globals.h"

#undef ALPHA
#ifdef __osf__
#define ALPHA
#endif

#if defined(__alpha) && defined(__osf__)

/*
 * A simple interface to the alpha PCC (Processor Cycle Counter)
 *
 * Probably not very robust, especially in the face of context
 * switches, interrupts, etc.  but may be OK for fine-grained
 * benchmarking
 */

#include <limits.h>
#include <unistd.h>
#include <c_asm.h>
#include <regdef.h>

#define HWCLOCK_MHZ 834

unsigned long hwclock_frequency;
double hwclock_period;
double hwclock_period_usecs;

inline static unsigned long hwclock_cycles(void)
{
    /*
      return asm("rpcc %v0\n"
      "sll %v0, 32, %t0\n",
      "addq %v0, %t0, %v0\n"
      "srl %v0, 32, %v0\n");
    */
    return asm("rpcc %v0\n"
               "zapnot %v0, 15, %v0\n");
}

inline static double hwclock_maxseconds(void)
{
    return hwclock_period * 4294967296;
}

inline static double hwclock_maxusecs(void)
{
    return hwclock_period_usecs * 4294967296;
}

inline static double hwclock_seconds(void)
{
    return hwclock_period * hwclock_cycles();
}

inline double hwclock_usecs(void)
{
    return hwclock_period_usecs * hwclock_cycles();
}

inline static void hwclock_init(void)
{
#ifdef TRY_TO_CALCULATE_FREQUENCY
    unsigned long t0, t1;
    long freq = -1;

    do {
        t0 = hwclock_cycles();
        sleep(1);
        t1 = hwclock_cycles();
        freq = t1 - t0;
        printf("t0 = %uld\n", t0);
        printf("t1 = %uld\n", t1);
        printf("freq = %ld\n", freq);
    } while (freq < 0);

    hwclock_frequency = freq;
    hwclock_period = 1.0 / (double) hwclock_frequency;
    hwclock_period_usecs = hwclock_period * 1.0e6;
#else
    hwclock_frequency = HWCLOCK_MHZ * 1000000;
    hwclock_period = 1.0 / (double) hwclock_frequency;
    hwclock_period_usecs = hwclock_period * 1.0e6;
#endif
}

#endif /* if defined(__alpha) && defined(__osf__) */

#if defined(__alpha) && defined(__osf__)
#define CLOCK() hwclock_seconds()
#else
#define CLOCK() dclock()
#endif

typedef struct pTime {
    double enterTime;
    double currentDiff;
    double cumDiff;
    double maxDiff;
    double minDiff;
    long int cnt;
    long int badcnt;
    int condition;
} pTime_t;

#define MAXTIMETHRESHOLD (double)(10.0e-3)

class timingObject {
public:
    int nPositions_m;
    pTime_t *pTimes;
    char **nameTable_m;
    double maxTime_m;

    timingObject(int nPositions, char **nameTable, double maxTime = MAXTIMETHRESHOLD) {
        nPositions_m = nPositions;
        nameTable_m = nameTable;
        maxTime_m = maxTime;
        pTimes = ulm_new(pTime_t, nPositions_m);
        for (int i = 0; i < nPositions_m; i++) {
            pTimes[i].minDiff = -1.0;
        }
#if defined(__alpha) && defined(__osf__)
        hwclock_init();
#endif
    }

    ~timingObject() {
        ulm_delete(pTimes);
    }

    void enter(int position, int cond) {
        pTime_t *d = &(pTimes[position]);
        d->enterTime = CLOCK();
        d->currentDiff = 0.0;
        d->condition = cond;
    }

    void setCond(int position, int cond) {
        pTimes[position].condition = cond;
    }

    void update(int position) {
        pTime_t *d = &(pTimes[position]);
        double now = CLOCK();
#if defined(__alpha) && defined(__osf__)
        if (now < d->enterTime) {
            /* assume single wrap */
            d->currentDiff += hwclock_maxseconds() - (d->enterTime - now);
        }
        else
            d->currentDiff += now - d->enterTime;
#else
        d->currentDiff += now - d->enterTime;
#endif
        d->enterTime = now;
    }

    void store(int position, double t) {
        pTime_t *d = &(pTimes[position]);
        // filter out "bad" values that might be due to being
        // context switched to another processor, interrupted,
        // or...
        if ((t < 0.0) || (t > maxTime_m)) {
            d->badcnt++;
            return;
        }
        d->cumDiff += t;
        d->cnt++;
        d->maxDiff = (t > d->maxDiff) ? t : d->maxDiff;
        d->minDiff = ((t < d->minDiff) || (d->minDiff == -1.0)) ? t : d->minDiff;
    }

    void exit(int position) {
        pTime_t *d = &(pTimes[position]);
        if (!d->condition) {
            return;
        }
        double now = CLOCK();
#if defined(__alpha) && defined(__osf__)
        double diff = now - d->enterTime;
        if (diff < 0.0) {
            /* assume single wrap */
            diff = hwclock_maxseconds() - (d->enterTime - now);
        }
#else
        double diff = now - d->enterTime;
#endif
        diff += d->currentDiff;
        // filter out "bad" values that might be due to being
        // context switched to another processor, interrupted,
        // or...
        if ((diff < 0.0) || (diff > maxTime_m)) {
            d->badcnt++;
            return;
        }
        d->cumDiff += diff;
        d->cnt++;
        d->maxDiff = (diff > d->maxDiff) ? diff : d->maxDiff;
        d->minDiff = ((diff < d->minDiff) || (d->minDiff == -1.0)) ? diff : d->minDiff;
    }

    void dump(FILE *fp) {
        fprintf(fp, "Timing differentials (nPositions = %d)\n", nPositions_m);
        for (int j = 0; j < nPositions_m; j++) {
            pTime_t *d = &(pTimes[j]);
            fprintf(fp, "\tglobal rank %d [%s]: average %e min %e max %e cnt %ld badcnt %ld\n",
                    lampiState.global_rank, nameTable_m[j],
                    (d->cnt) ? (d->cumDiff / d->cnt) : 0.0,
                    d->minDiff,
                    d->maxDiff,
                    d->cnt,
                    d->badcnt);
        }
    }
};

timingObject *timingObj = 0;

extern "C" int timingInit(int nPositions, char **nameTable, double maxTimingThreshold)
{
    timingObj = new timingObject(nPositions, nameTable, maxTimingThreshold);
    return (timingObj != 0);
}

extern "C" void timingEnter(int positionIndex)
{
    if (timingObj)
        timingObj->enter(positionIndex, 1);
}

extern "C" void timingCondEnter(int positionIndex, int cond)
{
    if (timingObj)
        timingObj->enter(positionIndex, cond);
}

extern "C" void timingSetCond(int positionIndex, int cond)
{
    if (timingObj)
        timingObj->setCond(positionIndex, cond);
}

extern "C" void timingUpdate(int positionIndex)
{
    if (timingObj)
        timingObj->update(positionIndex);
}

extern "C" void timingExit(int positionIndex)
{
    if (timingObj)
        timingObj->exit(positionIndex);
}

extern "C" void timingDump(FILE *fp)
{
    if (timingObj)
        timingObj->dump(fp);
}

extern "C" double timingClock(void)
{
    return ((timingObj) ? CLOCK() : -1.0);
}

extern "C" double timingDiff(double prevTime)
{
    if (!timingObj)
        return (-1.0);
    double now = CLOCK();
#if defined(__alpha) && defined(__osf__)
    double diff = now - prevTime;
    if (diff < 0.0) {
        /* assume single wrap */
        diff = hwclock_maxseconds() - (prevTime - now);
    }
#else
    double diff = now - prevTime;
#endif
    return diff;
}

extern "C" void timingStore(int positionIndex, double t)
{
    if (timingObj)
        timingObj->store(positionIndex, t);
}
