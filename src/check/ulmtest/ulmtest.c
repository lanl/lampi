/*
 * ulmtest -- front-end to API-level tests for ULM
 *
 * Provides a common interface for a variety of tests to exercise
 * various aspects of ULM.
 *
 * Adding a new test:
 *
 * The various tests are accessed via a dispatch table.  To add a new
 * test add an appropriate entry to the dispatch table "test_table",
 * create a file test_???.c containing a function "int
 * test_???(status_t *)" to implement the test, add a prototype to
 * ulmtest.h, and add the new test_???.c file to SRC in the Makefile.
 *
 * Where possible, make use of the existing state variables (state_t),
 * so that the interface can be reused.
 *
 */

/* Standard C header files ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Other header files *************************************************/

#include <getopt.h>

#include <ulm/ulm.h>

#include "ulmtest.h"

/* File scope variables ***********************************************/

/*
 * Dispatch table
 */

static test_table_t test_table[] = {
    {test_sping,	"sping",	"simplex ping"},
    {test_dping,	"dping",	"duplex ping"},
    {test_wildproc,	"wildproc",	"ping with wildcard proc"},
    {test_wildtag,	"wildtag",	"ping with wildcard tag"},
    {test_wildall,	"wildall",	"ping with wildcard proc and tag"},
    {test_env,	"env",		"print local environment"},
    {test_bcast,    "bcast",	"test the broadcast"},
#if 0
    {test_allreduce, "allreduce",	"allreduce functional test"},
    {test_reduce,	"reduce",	"reduce functional test"},
    {test_monte_carlo, "monte_carlo", "Monte Carlo calculation of pi"},
#endif
    {NULL,		"",		""}
};

/*
 * Strings
 */

static const char *usage_string =
"ulmtest [-h] [-c] [-b size[,size2,...] [-m size] [-p pat] [-r nreps] \n"
"  [-s groupsize] [-R root] [-S seed] test [test2 test3 ...]\n";

static const char *help_string =
"An API-level exerciser for ULM.\n\n"
"  -h                    This message\n"
"  -b <min>[,max[,inc]]  Buffer size (min, max, and increment)\n"
"  -c                    Check data\n"
"  -m <size>             Misalign buffers by this amount\n"
"  -p <pattern>          Hexadecimal seed pattern for generating data\n"
"  -r <reps>             Number of timing repetitions\n"
"  -s <size>             Size of the subgroup (where applicable)\n"
"  -R <root>             Root process (for mulitcasts etc.)\n"
"  -S <seed>             Seed for pseudorandom number generator\n"
"  test                  Test(s) to run. Possible tests are:\n";

/* Functions **********************************************************/

/*
 * Dispatcher
 */

static int	dispatch(state_t *state, char *test)
{
    test_table_t *p = test_table;

    while (p->func) {
	if (strcmp(p->verb, test) == 0) {
	    print_root(state->group,
		       "\nTest: %s (%s)\n\n", p->verb, p->desc);
	    return (p->func(state));
	}
	p++;
    }

    return (-1);
}


/*
 * Argument processing
 */

static void help(state_t *state)
{
    test_table_t *p = test_table;

    print_root(state->group, "%s", usage_string);
    print_root(state->group, "%s", help_string);
    while (p->func) {
	print_root(state->group,
		   "                          "
		   "%-12s - %s\n", p->verb, p->desc);
	p++;
    }
    print_root(state->group, "\n");

    ulm_finalize();
    exit(EXIT_SUCCESS);
}


static void usage(state_t *state)
{
    print_root(state->group, "Usage: %s", usage_string);

    ulm_finalize();
    exit(EXIT_FAILURE);
}

/*
 * Functions for using readable sizes
 */

static long str2size(char *str)
{
    int size;
    char mod[32];

    switch (sscanf(str, "%d%1[gGmMkK]", &size, mod)) {

    case 1:
	return ((long) size);
	break;

    case 2:
	switch (*mod) {

	case 'g':
	case 'G':
	    return ((long) size << 30);
	    break;

	case 'm':
	case 'M':
	    return ((long) size << 20);
	    break;

	case 'k':
	case 'K':
	    return ((long) size << 10);
	    break;

	default:
	    return (-1);
	    break;
	}

    default:
	return (-1);
	break;
    }
}


static char *size2str(long n)
{
    static char *letters[] = { " ", "K", "M", "G" };
    static char cbuf[256];
    static char *next = cbuf;
    char *result = (next + 32 > cbuf + sizeof(cbuf)) ? cbuf : next;
    int i;

    for (i = 0;; i++, n >>= 10) {
	if (n >= (1 << 10) && i < 4) {
	    continue;
	}
	next = result + 1 + sprintf(result, "%d%s", (int) n, letters[i]);
	break;
    }

    return (result);
}


/*
 * Main function
 */

int main(int argc, char *argv[])
{
    int c, i, self, nproc;
    state_t state[1];
    char *tok;
    extern char *optarg;
    extern int optind;

    /*
     * ULM initialization
     */

    ulm_init();

    /*
     * Default state
     */

    state->group = ULM_COMM_WORLD;
    state->bufmin = 64 << 10;
    state->bufmax = 0;
    state->bufinc = 0;
    state->misalign = 0;
    state->nreps = 0;
    state->pattern = 0xAA;
    state->check = 0;
    state->subgroup_size = 0;
    state->root = 0;
    state->seed = 1;

    /*
     * Parse arguments
     */

    while ((c = getopt(argc, argv, "hcvb:m:p:r:G:R:S:")) != -1) {

	switch (c) {

	case 'h':
	    help(state);
	    break;

	case 'c':
	    state->check = 1;
	    break;

	case 'b':
	    if ((tok = strtok(optarg, ",")) != NULL) {
		state->bufmin = str2size(tok);
		if ((tok = strtok(NULL, ",")) != NULL) {
		    state->bufmax = str2size(tok);
		    if ((tok = strtok(NULL, ",")) != NULL) {
			state->bufinc = str2size(tok);
		    }
		}
	    }
	    break;

	case 'm':
	    state->misalign = str2size(optarg);
	    break;

	case 'p':
	    state->pattern = 0xFF & strtoul(optarg, NULL, 0);
	    break;

	case 'r':
	    state->nreps = str2size(optarg);
	    break;

	case 'G':
	    state->subgroup_size = str2size(optarg);
	    break;

	case 'R':
	    state->root = strtoul(optarg, NULL, 0);
	    break;

	case 'S':
	    state->seed = str2size(optarg);
	    break;

	default:
	    usage(state);
	    break;
	}
    }
    argv += optind;
    argc -= optind;
    if (argc < 1) {
	usage(state);
    }

    /*
     * Sanity checks for state
     */

    if (state->nreps == 0) {
	state->nreps = state->bufmin < 1 << 10 ? 1000 : 100;
    }

    if (state->bufmax < state->bufmin) {
	state->bufmax = state->bufmin;
    }

    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_PROCID,
			   &self, sizeof(int)));
    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_NUMBER_OF_PROCS,
			   &nproc, sizeof(int)));

    if (state->root < 0 || state->root >= nproc) {
	state->root = 0;
    }

    if (state->subgroup_size <= 0 || state->subgroup_size > nproc) {
	state->subgroup_size = nproc;
    }

    print_root(state->group, "ulmtest -- ULM tests\n\n");
    print_root(state->group, "Buffer min    %ld\n", state->bufmin);
    print_root(state->group, "Buffer max    %ld\n", state->bufmax);
    print_root(state->group, "Buffer inc    %ld\n", state->bufinc);
    print_root(state->group, "Check data    %c\n", state->check ? 'y' : 'n');
    print_root(state->group, "Misalign      %ld\n", state->misalign);
    print_root(state->group, "Pattern       0x%X\n", state->pattern);
    print_root(state->group, "Repetitions   %ld\n", state->nreps);
    print_root(state->group, "RNG seed      %d\n", state->seed);
    print_root(state->group, "Root proc     %d\n", state->root);
    print_root(state->group, "Subgroup size %d\n", state->subgroup_size);
    print_root(state->group, "Tests       ");
    for (i = 0; i < argc; i++) {
	print_root(state->group, "%s ", argv[i]);
    }
    print_root(state->group, "\n");

    /*
     * Run tests
     */

    while (argc--) {
	if (dispatch(state, *argv) < 0) {
	    print_root(state->group,
		       "WARNING: %s: failed or not implemented\n", *argv);
	}
	argv++;
	fflush(stdout);
	barrier(state->group);
    }

    print_root(state->group, "Done\n");

    ulm_finalize();

    return (EXIT_SUCCESS);
}
