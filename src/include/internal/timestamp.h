#ifndef TIMESTAMP_H_INCLUDED
#define TIMESTAMP_H_INCLUDED

#include "util/dclock.h"

/*
 * macros variables for timing: collects and reports differential
 * times accumulated over many iterations
 */

#if TIMESTAMP_ENABLE > 0

#ifndef TIMESTAMP_MAX
#define TIMESTAMP_MAX 8
#endif

static long ts_count = 0;
static double ts_dt[TIMESTAMP_MAX];         /* cumulative differential time */
static double ts_tt[TIMESTAMP_MAX + 1];     /* total time */

/*
 * TIMESTAMP(0) initializes for this iteration, TIMESTAMP(1),
 * TIMESTAMP(2),... collect cummulative differential times
 */

#define TIMESTAMP(N)                                   \
do {                                                   \
    if (N == 0) {                                      \
        ts_tt[0] = dclock();                           \
        ts_count++;                                    \
    } else {                                           \
        ts_tt[N] = dclock();                           \
        ts_dt[N] += (ts_tt[N+1] - ts_tt[N]);           \
    }                                                  \
} while (0)


/*
 * TIMESTAMP_REPORT(COUNT) prints out the cumulative differential
 * times every COUNT iterations
 */

#define TIMESTAMP_REPORT(COUNT)                        \
do {                                                   \
    if (TIMESTAMP_ENABLE) {                            \
        if (ts_count % (COUNT) == 0) {                 \
            printf("timestamp: count = %ld",           \
                   ts_count);                          \
            for (int i = 0; i < TIMESTAMP_MAX; i++) {  \
               printf(", dt[%d] = %lf",                \
                      i, ts_dt[i]);                    \
            }                                          \
            printf("\n");                              \
            fflush(stdout);                            \
    }                                                  \
} while (0)

#else 

#define TIMESTAMP(N)
#define TIMESTAMP_REPORT(N)

#endif

#endif
