/*
 * File:	test_wildproc.c
 * Description:	ping test with wildcard process field
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

int	test_wildproc(state_t *state)
{
    long bufsize = state->bufmin;
    int self;
    int nproc;

    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_PROCID,
			   &self, sizeof(int)));
    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_NUMBER_OF_PROCS,
			   &nproc, sizeof(int)));

    while (bufsize <= state->bufmax) {

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
	}

	bufo = buf1 + state->misalign;
	bufi = buf2 + state->misalign;

	/*
	 * Find a peer
	 */

	peer = self ^ 1;
	has_peer = peer < nproc ? 1 : 0;

	barrier(state->group);
	t0 = second();

	if (has_peer && (self & 1)) {
	    for (rep = 0; rep < state->nreps; rep++) {

		if (state->check) {
		    set_data(self, rep, state->pattern, bufo, bufsize);
		}

		CHECK_ULM(ulm_isend(bufo,  bufsize, NULL, peer,
                                    tag | self, state->group, &req, ULM_SEND_STANDARD));
		CHECK_ULM(ulm_wait(&req, &status));
		CHECK_ULM(ulm_irecv(				    bufi,  bufsize, NULL, -1,  tag,  state->group,  &req));
		CHECK_ULM(ulm_wait(&req, &status));

		if (state->check) {
		    CHECK_DATA(peer, rep, state->pattern, bufi, bufsize);
		}
	    }
	} else if (has_peer) {
	    for (rep = 0; rep < state->nreps; rep++) {

		if (state->check) {
		    set_data(self, rep, state->pattern, bufo, bufsize);
		}

		CHECK_ULM(ulm_irecv(				    bufi,  bufsize, NULL, -1,  tag | self | 1,  state->group,  &req));
		CHECK_ULM(ulm_wait(&req, &status));
		CHECK_ULM(ulm_isend(bufo,  bufsize, NULL, peer,
                                    tag, state->group, &req, ULM_SEND_STANDARD));
		CHECK_ULM(ulm_wait(&req, &status));

		if (state->check) {
		    CHECK_DATA(peer, rep, state->pattern, bufi, bufsize);
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
