/* Standard C header files ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

/* Other header files *************************************************/

#include <ulm/ulm.h>
#include <mpi/mpi.h>

#include "ulmtest.h"

/* Functions **********************************************************/

int	test_bcast(state_t *state)
{
    long bufsize = state->bufmin;
    int comm;
    int self;
    int size;
    int subgroup_root;
    int in_group;
    int rep;
    int *listofRanks;
    unsigned char *buf;
    double t, t0, t1;
    char s[10 << 10];		/* 10K */
    int i, j, pos, found;
    unsigned int index;

    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_PROCID,
			   &self, sizeof(int)));
    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_NUMBER_OF_PROCS,
			   &size, sizeof(int)));

    listofRanks = (int *) malloc(sizeof(int) * state->subgroup_size);
    if (listofRanks == NULL) {
	return (-1);
    }

    if (state->subgroup_size < size) {

	/*
	 * Create a new context for a subgroup of the processes.
	 *
	 * Require state->group == ULM_COMM_WORLD so that I can get
	 * the global procid
	 */

	assert(state->group == ULM_COMM_WORLD);

	comm = 9;

	/*
	 * Randomly (but uniquely) fill subgroup member array from
	 * non-root procs, then put root proc at a random position.
	 */

	srandom(state->seed);
	for (i = 0; i < state->subgroup_size;) {
	    found = 0;
	    index = random() % size;
	    if (index != state->root) {
		for (j = 0; j < i; j++) {
		    if (listofRanks[j] == index) {
			found = 1;
		    }
		}
		if (!found) {
		    listofRanks[i++] = index;
		}
	    }
	}
	subgroup_root = random() % state->subgroup_size;
	listofRanks[subgroup_root] = state->root;

    } else {

	/*
	 * Use the current context
	 */

	comm = state->group;
	subgroup_root = state->root;
	for (i = 0; i < size; i++) {
	    listofRanks[i] = i;
	}
    }

    pos = 0;
    in_group = 0;
    for (i = 0; i < state->subgroup_size; i++) {
	pos += sprintf(s + pos, " %d", listofRanks[i]);
	if (listofRanks[i] == self) {
	    in_group = 1;
	}
    }
    print_in_order(state->group, "Subgroup members:%s, Subgroup root: %d\n",
		   s, subgroup_root);


    while (in_group && bufsize <= state->bufmax) {

	buf = (unsigned char *) malloc(bufsize + state->misalign);

	barrier(comm);
	t0 = second();

	for (rep = 0; rep < state->nreps; rep++) {
	    if (self == state->root) {
		if (state->check) {
		    set_data(state->root, rep, state->pattern, buf,
			     bufsize);
		}
	    }
	    CHECK_ULM(ulm_bcast(buf, bufsize, (ULMType_t *)MPI_BYTE,
                                subgroup_root, comm));

	    if (state->check) {
		CHECK_DATA(state->root, rep, state->pattern, buf, bufsize);
	    }

	}

	t1 = second();
	barrier(comm);

	t = 1.0e6 * (t1 - t0) / state->nreps;

	print_root(comm,
		   "bcast from %d: %.1f us for %ld bytes (%.2f MB/s/proc)\n",
		   subgroup_root, t, bufsize, ((double) bufsize) / t);

	if (buf) {
	    free(buf);
	}

	if (state->bufinc == 0) {
	    bufsize *= 2;
	} else if (state->bufinc > 0) {
	    bufsize += state->bufinc;
	} else {
	    /* run forever */ ;
	}
    }

    barrier(state->group);

    return 0;
}
