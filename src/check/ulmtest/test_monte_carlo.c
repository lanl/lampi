#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "ulm/ulm.h"
#include "ulmtest.h"
#include "prand.h"

int test_monte_carlo(state_t *state)
{
    int self, nproc;
    long nreps, count;
    double mypi, pi;
    prand_t prand;

    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_PROCID,
                           &self, sizeof(int)));
    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_NUMBER_OF_PROCS,
                           &nproc, sizeof(int)));

    prand.prand_multiplier = PRAND_DEFAULT_MULTIPLIER;
    prand.prand_seed = state->seed;

    prand_advance(&prand, 2 * state->nreps * self);

    nreps = state->nreps;
    count = 0;
    while (nreps--) {

	double r2, x, y;

	x = prand_double(&prand);
	y = prand_double(&prand);

	r2 = x * x + y * y;
	if (r2 < 1.0) {
	    count++;
	}
    }

    mypi = 4.0 * (double) count / (double) state->nreps;

    print_in_order(state->group, "proc %d has mypi = %f\n", self, mypi);

    CHECK_ULM(ulm_allreduce(state->group, &pi, &mypi, sizeof(double), 1,
			    NULL, ulm_dsum, 1));

    pi /= (double) nproc;
    print_in_order(state->group, "proc %d has pi = %f\n", self, pi);

    return 0;
}
