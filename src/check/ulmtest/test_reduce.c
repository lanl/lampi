/*
 * File:	test_reduce.c
 * Description:	reduce test
 */

/* Standard C header files ********************************************/

#include <stdio.h>
#include <stdlib.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

/* Other header files *************************************************/

#include <ulm/ulm.h>

#include "ulmtest.h"

/* Types **************************************************************/

enum { DEBUG = 0 };

enum { DELIBERATE_ERROR = 0 };

enum datatype {
    DT_ZERO, DT_CHAR, DT_INT, DT_LONG, DT_FLOAT, DT_DOUBLE, DT_END
};

enum optype {
    OT_NULL, OT_MAX, OT_MIN, OT_SUM, OT_PROD, OT_OR, OT_AND, OT_XOR
};

typedef union pointer_t pointer_t;
union pointer_t {
    void *v;
    char *c;
    unsigned char *u;
    int *i;
    long *l;
    float *f;
    double *d;
};

typedef struct function_info_t function_info_t;
struct function_info_t {
    enum datatype datatype;
    size_t size;
    enum optype optype;
    char *name;
    ULMFunc_t *func;
};


/* Functions **********************************************************/

/*
 * A null op for timing
 */

static void null(void *p1, void *p2, int *n, void *arg)
{
    return;
}

static struct {
    enum datatype datatype;
    size_t size;
    enum optype optype;
    char *name;
    ULMFunc_t *func;
} function_info[] = {
    {DT_CHAR, sizeof(char), OT_NULL, "null", null},
    {DT_INT, sizeof(int), OT_MIN, "imin", ulm_imin},
    {DT_INT, sizeof(int), OT_MAX, "imax", ulm_imax},
    {DT_INT, sizeof(int), OT_SUM, "isum", ulm_isum},
    {DT_INT, sizeof(int), OT_PROD, "iprod", ulm_iprod},
    {DT_INT, sizeof(int), OT_OR, "ior", ulm_ibor},
    {DT_INT, sizeof(int), OT_AND, "iand", ulm_iband},
    {DT_INT, sizeof(int), OT_XOR, "ixor", ulm_ibxor},
    {DT_LONG, sizeof(long), OT_MIN, "lmin", ulm_lmin},
    {DT_LONG, sizeof(long), OT_MAX, "lmax", ulm_lmax},
    {DT_LONG, sizeof(long), OT_SUM, "lsum", ulm_lsum},
    {DT_LONG, sizeof(long), OT_PROD, "lprod", ulm_lprod},
    {DT_LONG, sizeof(long), OT_OR, "lor", ulm_lbor},
    {DT_LONG, sizeof(long), OT_AND, "land", ulm_lband},
    {DT_LONG, sizeof(long), OT_XOR, "lxor", ulm_lbxor},
    {DT_FLOAT, sizeof(float), OT_MIN, "fmin", ulm_fmin},
    {DT_FLOAT, sizeof(float), OT_MAX, "fmax", ulm_fmax},
    {DT_FLOAT, sizeof(float), OT_SUM, "fsum", ulm_fsum},
    {DT_FLOAT, sizeof(float), OT_PROD, "fprod", ulm_fprod},
    {DT_DOUBLE, sizeof(double), OT_MIN, "dmin", ulm_dmin},
    {DT_DOUBLE, sizeof(double), OT_MAX, "dmax", ulm_dmax},
    {DT_DOUBLE, sizeof(double), OT_SUM, "dsum", ulm_dsum},
    {DT_DOUBLE, sizeof(double), OT_PROD, "dprod", ulm_dprod},
    {DT_END, 0, OT_NULL, NULL, NULL}
};

int test_reduce(state_t *state)
{
    long bufsize = state->bufmin;
    int self;
    int nproc;
    int seed;

    if (DEBUG) {
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
    }

    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_PROCID,
			   &self, sizeof(int)));
    CHECK_ULM(ulm_get_info(state->group, ULM_INFO_NUMBER_OF_PROCS,
			   &nproc, sizeof(int)));

    while (bufsize <= state->bufmax) {

	int rep;
	double t, t0, t1;
	char s[256];
	int fn, i;
	unsigned char *buf1, *buf2;
	pointer_t bufo, bufi;

	/*
	 * Allocate buffers, and (mis)align them.
	 */

	buf1 = (unsigned char *) malloc(bufsize + state->misalign);
	buf2 = (unsigned char *) malloc(bufsize + state->misalign);
	bufo.u = buf1 + state->misalign;
	bufi.u = buf2 + state->misalign;

	for (fn = 0; function_info[fn].datatype != DT_END; fn++) {

	    size_t size = function_info[fn].size;
	    int nobject = bufsize / size;

	    if (state->check) {

		seed = rand();
		switch (function_info[fn].datatype) {

		case DT_ZERO:
		case DT_CHAR:
		    break;

		case DT_INT:
		    for (i = 0; i < nobject; i++) {
			bufo.i[i] = seed + self + i;
		    }
		    break;

		case DT_LONG:
		    for (i = 0; i < nobject; i++) {
			bufo.l[i] = seed + self + i;
		    }
		    break;

		case DT_FLOAT:
		    for (i = 0; i < nobject; i++) {
			bufo.f[i] = (float) (seed + self + i);
		    }
		    break;

		case DT_DOUBLE:
		    for (i = 0; i < nobject; i++) {
			bufo.d[i] = (double) (seed + self + i);
		    }
		    break;

		default:
		    assert(0);
		}
	    }

	    t0 = second();
	    barrier(state->group);

	    for (rep = 0; rep < state->nreps; rep++) {
		CHECK_ULM(ulm_reduce(state->group, state->root,
				     bufi.v, bufo.v,
				     size, nobject,
				     NULL,
				     function_info[fn].func, 1));
	    }

	    t1 = second();
	    barrier(state->group);
	    t = 1.0e6 * (t1 - t0) / (state->nreps);
	    sprintf(s, "%s: %4d: %.1f us for %ld bytes (%.2f MB/s/proc)\n",
		    function_info[fn].name, self, t, bufsize,
		    (2.0 * (double) bufsize) / t);
	    print_in_order(state->group, s);

	    if (state->check && (state->root == self)) {
		if (function_info[fn].optype == OT_SUM) {
		    switch (function_info[fn].datatype) {
		    case DT_INT:
			for (i = 0; i < nobject; i++) {
			    int x =
				nproc * (seed + i) +
				(nproc * (nproc - 1)) / 2;
			    assert(bufi.i[i] == x);
			    if (DEBUG) {
				printf("%d :: %d :: %d\n", bufo.i[i],
				       bufi.i[i], x);
			    }
			}
			break;
		    case DT_LONG:
			if (DELIBERATE_ERROR) {
			    bufi.l[42] = 0L;
			}
			for (i = 0; i < nobject; i++) {
			    long x =
				nproc * (seed + i) +
				(nproc * (nproc - 1)) / 2;
			    assert(bufi.l[i] == x);
			    if (DEBUG) {
				printf("%ld :: %ld :: %ld\n", bufo.l[i],
				       bufi.l[i], x);
                            }
			}
			break;
		    case DT_FLOAT:
			for (i = 0; i < nobject; i++) {
			    float x =
				nproc * (seed + i) +
				(nproc * (nproc - 1)) / 2;
			    assert(bufi.f[i] == x);
			    if (DEBUG) {
				printf("%f :: %f :: %f\n", bufo.f[i],
				       bufi.f[i], x);
			    }
			}
			break;
		    case DT_DOUBLE:
			for (i = 0; i < nobject; i++) {
			    double x =
				nproc * (seed + i) +
				(nproc * (nproc - 1)) / 2;
			    assert(bufi.d[i] == x);
			    if (DEBUG) {
				printf("%f :: %f :: %f\n", bufo.d[i],
				       bufi.d[i], x);
			    }
			}
			break;
		    default:
			break;
		    }
		}
	    }
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
