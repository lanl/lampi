/*
 *  An implementation of the random number algorithm outlined in the
 *  NAS Parallel Benchmarks.
 */

#ifndef PRAND_H_INCLUDED
#define PRAND_H_INCLUDED

#ifdef	__cplusplus
extern "C"
{
#endif

/* types **************************************************************/

    typedef struct {
	double	prand_multiplier;
	double	prand_seed;
    } prand_t;

/* macros *************************************************************/

#define PRAND_DEFAULT_MULTIPLIER	1220703125.0L
#define PRAND_DEFAULT_SEED		314159265.0L

/* prototypes *********************************************************/

    double	prand_double(prand_t *);
    long	prand_long(prand_t *);
    void	prand_advance(prand_t *, int);

#ifdef	__cplusplus
}
#endif

#endif /* PRAND_H_INCLUDED */
