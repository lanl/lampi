/*
 *  An implementation of the random number algorithm outlined in the
 *  NAS Parallel Benchmarks.
 */

/* Standard C header files ********************************************/

#include <limits.h>

/* local header files *************************************************/

#include "prand.h"

/* file scope variables ***********************************************/

/*
 * r23 = 2^-23
 * r46 = 2^-46
 * t23 = 2^23
 * t46 = 2^46
 */

const double r23 = 0.11920928955078125E-06;
const double r46 = 0.142108547152020037174224853515625E-13;
const double t23 = 0.8388608E+07;
const double t46 = 0.70368744177664E+14;

/* functions **********************************************************/

static void	evolve(double *seed, double multiplier)
{
    double		x = *seed;
    double		a = multiplier;
    double		fa1, fa2, ft1, ft3, fx2, fz;
    int		ia1, it2, it4, ix1;

    ia1 = (int) (r23 * a);
    fa2 = a - t23 * (double) ia1;
    fa1 = (double) ia1;
    ix1 = (int) (r23 * x);
    fx2 = x - t23 * (double) ix1;
    ft1 = fa1 * fx2 + fa2 * (double) ix1;
    it2 = (int) (r23 * ft1);
    fz = ft1 - t23 * (double) it2;
    ft3 = t23 * fz + fa2 * fx2;
    it4 = (int) (r46 * ft3);
    x = ft3 - t46 * (double) it4;

    *seed = x;
}


void prand_advance(prand_t *p, int n)
{
    /*
     * Advance the seed by n steps in the sequence, using a
     * logarithmic algorithm.
     */

    int		j, k;
    double		sk, ak;

    ak = p->prand_multiplier;
    sk = p->prand_seed;
    k = n;
    while (k > 0) {
        j = k / 2;
        if (2 * j != k) {
            evolve(&sk, ak);
        }
        evolve(&ak, ak);
        k = j;
    }
    p->prand_seed = sk;
}


double prand_double(prand_t *p)
{
    evolve(&(p->prand_seed), p->prand_multiplier);

    return(p->prand_seed * r46);
}


long	prand_long(prand_t *p)
{
    double		value;

    evolve(&(p->prand_seed), p->prand_multiplier);
    value = (p->prand_seed * r46) * (double) LONG_MAX;

    return((long) value);
}


#ifdef DEBUG_PRAND

#include <stdio.h>

int main(int argc, char* argv[])
{
    int		i;
    prand_t		p;

    p.prand_multiplier = PRAND_DEFAULT_MULTIPLIER;
    p.prand_seed = PRAND_DEFAULT_SEED;

    i = 16;
    while (i--) {
        printf("%.15f\n", prand_double(&p));
    }

    return(0);
}

#endif /* DEBUG_PRAND */
