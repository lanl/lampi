/* Standard C header files ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

/* Other header files *************************************************/

#include "ulm/ulm.h"

#include "ulmtest.h"

/* Functions **********************************************************/

int test_mcast(state_t *state)
{
    return (0);
}

#if 0
int test_mcast(state_t *state)
{
    long bufsize = state->bufmin;
    int self;
    int nproc;
    int seed;
    int contextID;
    int group_members[];
    int self_in_group;
    int i, j, pos;
    char s[10<<10]; /* 10K */

    if (DEBUG) {
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
    }

    /*
     * CAUTION:
     *
     * Here I require that state->group is ULM_COMM_WORLD, because
     * I don't know what do otherwise.
     *
     * This suggests there is something broken about the interface.
     * Really I should never have to refer to global IDs from the API
     * level: everything should be relative to the context I am given.
     */

    assert(state->group == ULM_COMM_WORLD);

    CHECK_ULM(ulm_GetGroupSize(state->group, &nproc));
    CHECK_ULM(ulm_GetMyGroupProcRank(state->group, &self));

    /*
     * Set up a group.  All procs have the same state so this can be
     * done in parallel without communication.
     */

    group_members = (int *) malloc(state->subgroup_size * sizeof(int));
    assert(group_members);

    if (state->subgroup_size == size) {

	/*
	 * Just use the group we are given.
	 */

	contextID = state->group;

	for (i = 0; i < size; i++) {
	    group_members[i] = i;
	}

    } else {

	/*
	 * Generate a random subgroup, but make sure the root proc is
	 * included.
	 */

	int useThreads = 1;
	int copyData = 1;
	functions_list *flagBarriers;

	contextID = 9;
        srandom(state->seed);

	group_members[0] = state->root;
	for (i = 1; i < state->subgroup_size; i++) {
	    while (1) {
		unsigned int value = (random() & 65535);
		unsigned int index = value * sizeofGroup / 65536;
	        int found = 0;
		for (j = 0; j < i; j++) {
		    if (group_members[j] == index) {
			found = 1;
			break;
		    }
		}
		if (!found) {
		    group_members[i] == index;
		    break;
		}
	    }
	}

	CHECK_ULM(ulm_context_alloc(contextID, state->subgroup_size,
				    group_members, &flagBarriers,
				    useThreads, copyData));
    }

    self_in_group = 0;
    pos = 0;
    for (i = 0; i < state->subgroup_size; i++) {
	pos += sprintf(s + pos, " %d", group_members[i]);
	if (self == group_members[i]) {
	    self_in_group = 1;
	}
    }
    print_in_order("Group members: %s\n", s);

    CHECK_ULM(ulm_GetInfo(contextID, ULM_INFO_HOSTID, &hostID, sizeof(int)));
    CHECK_ULM(ulm_GetInfo(contextID, ULM_INFO_HOSTID, &hostID, sizeof(int)));

    collInfo[i].hostID = hostID;

    while (self_in_group && bufsize <= state->bufmax) {

	int rep;
	double t, t0, t1;
	char s[256];
	int fn, i;
	unsigned char *buf1, *buf2;
	unsigned char *bufo, *bufi;

	/*
	 * Allocate buffers, and (mis)align them.
	 */

	buf1 = (unsigned char *) malloc(bufsize + state->misalign);
	buf2 = (unsigned char *) malloc(bufsize + state->misalign);
	bufo = buf1 + state->misalign;
	bufi = buf2 + state->misalign;

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

    barrier(state->group);

    return (0);
}
#endif
