/*
 * File:	test_wildall.c
 * Description:	ping test with wildcard tag and proc fields
 *
 * Note:
 *
 * This is a more complicated test, since we need to try out only
 * one pair at a time but all possible pairs.
 *
 */

/* Standard C header files ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Other header files *************************************************/

#include <ulm/ulm.h>

#include "ulmtest.h"

/* Functions **********************************************************/


static int	next_proc_pair(int *proc0, int *proc1, int nproc)
{
    /*
     * Given
     */

    if (*proc0 < 0 || *proc1 < 0 || *proc0 > nproc || *proc1 > nproc) {
	return (-1);
    }

    do {
	if ((*proc1 + 1) < nproc) {
	    *proc1 += 1;
	} else if ((*proc0 + 1) < nproc) {
	    *proc0 += 1;
	    *proc1 = 0;
	} else {
	    return (-1);
	}
    } while (*proc0 == *proc1);

    return (0);
}

int	test_wildall(state_t *state)
{
    long bufsize = state->bufmin;
    int self;
    int nproc;

    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_PROCID,
			   &self, sizeof(int)));
    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_NUMBER_OF_PROCS,
			   &nproc, sizeof(int)));

    while (bufsize <= state->bufmax) {

	int proc0, proc1;
	int peer;
	int has_peer;
	int rep;
	int tag = 0xDD << 16;
	double t, t0, t1;
	ULMRequest_t req;
	ULMStatus_t status;
	char s[256];
	unsigned char *buf1, *buf2, *bufo, *bufi;

	/*
	 * Allocate buffers, and (mis)align them.
	 */

	buf1 = (unsigned char *) malloc(bufsize + state->misalign);
	buf2 = (unsigned char *) malloc(bufsize + state->misalign);
	if (!buf1 || !buf2) {
	    perror("out of memory");
	    return (-1);
	}

	bufo = buf1 + state->misalign;
	bufi = buf2 + state->misalign;

	/*
	 * Iterate over all proc pairs
	 */

	proc0 = 0;
	proc1 = 0;

	while (next_proc_pair(&proc0, &proc1, nproc) == 0) {

	    print_simple(state->group, "proc0, proc1 = %d, %d\n", proc0,
			 proc1);

	    if (self == proc0) {
		peer = proc1;
		has_peer = 1;
	    } else if (self == proc1) {
		peer = proc0;
		has_peer = 1;
	    } else {
		peer = -1;
		has_peer = 0;
	    }

	    barrier(state->group);
	    t0 = second();

	    if (self == proc0) {
		for (rep = 0; rep < state->nreps; rep++) {

		    if (state->check) {
			set_data(self, rep, state->pattern, bufo, bufsize);
		    }

		    CHECK_ULM(ulm_isend(bufo,  bufsize, NULL, peer,
                                        (tag | self) + rep, state->group, &req, ULM_SEND_STANDARD));
		    CHECK_ULM(ulm_wait(&req, &status));
		    CHECK_ULM(ulm_irecv(					bufi,  bufsize, NULL, -1,  -1,  state->group,  &req));
		    CHECK_ULM(ulm_wait(&req, &status));

		    if (state->check) {
			CHECK_DATA(peer, rep, state->pattern, bufi,
				   bufsize);
		    }
		}
	    } else if (self == proc1) {
		for (rep = 0; rep < state->nreps; rep++) {

		    if (state->check) {
			set_data(self, rep, state->pattern, bufo, bufsize);
		    }

		    CHECK_ULM(ulm_irecv(					bufi,  bufsize, NULL, peer,  -1,  state->group,  &req));
		    CHECK_ULM(ulm_wait(&req, &status));
		    CHECK_ULM(ulm_isend(bufo,  bufsize, NULL, peer,
                                        (tag | self) + rep, state->group, &req, ULM_SEND_STANDARD));
		    CHECK_ULM(ulm_wait(&req, &status));

		    if (state->check) {
			CHECK_DATA(peer, rep, state->pattern, bufi,
				   bufsize);
		    }
		}
	    }

	    t1 = second();
	    barrier(state->group);

	    t = 1.0e6 * (t1 - t0) / (2.0 * state->nreps);

	    if (has_peer) {
		sprintf(s,
			"%4d -> %4d: %.1f us for %ld bytes (%.2f MB/s)\n",
			self, peer, t, bufsize, ((double) bufsize) / t);
	    } else {
		strcpy(s, "no peer\n");
	    }

	    print_in_order(state->group, s);
	}

	if (buf1) {
	    free(buf1);
	}
	if (buf2) {
	    free(buf2);
	}

	/*
	 * Increment buffer size
	 */

	if (state->bufinc == 0) {
	    bufsize *= 2;
	} else if (state->bufinc > 0) {
	    bufsize += state->bufinc;
	} else {
	    /* run forever */ ;
	}
    }

    return (0);
}
