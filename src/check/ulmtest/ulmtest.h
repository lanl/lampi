/*
 * File:	ulmtest.h
 * Description:	header file for ulmtest
 *
 */

#ifndef ULMTEST_H_INCLUDED
#define ULMTEST_H_INCLUDED

#ifdef	__cplusplus
extern "C"
{
#endif

/* Macros *************************************************************/

#define CHECK_ULM(X)	check_ulm((X), # X, __FILE__, __LINE__)
#define CHECK_DATA(p, s, pat, b, n) \
	check_data((p), (s), (pat), (b), (n), __FILE__, __LINE__)
#define RANGE(x,a,b)	((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

/* Type definitions ***************************************************/

/*
 * State to pass to tests
 */
    typedef struct state_t state_t;
    struct state_t {
	long		bufmin;		/* minimum buffer size */
	long		bufmax;		/* maximum buffer size */
	long		bufinc;		/* buffer size increment */
	long		misalign;	/* buffer misalignment */
	long		nreps;		/* timing repetitions */
	int		check;		/* data checking switch */
	unsigned char	pattern;	/* seed for data checking */
	int		group;		/* ULM group ID */
	int		subgroup_size;	/* for collective op tests */
	int		root;		/* root procID (for mcast etc.) */
	unsigned int	seed;		/* RNG seed */
    };


/*
 * Test dispatch table
 */

    typedef int (*test_func_t)(state_t *);

    typedef struct test_table_t test_table_t;
    struct test_table_t {
	test_func_t	func;
	char		*verb;
	char		*desc;
    };


/* Prototypes *******************************************************/

/*
 * Tests (test_???.c)
 */

    int	test_sping(state_t *state);
    int	test_dping(state_t *state);
    int	test_wildproc(state_t *state);
    int	test_wildtag(state_t *state);
    int	test_wildall(state_t *state);
    int	test_env(state_t *state);
    int	test_allreduce(state_t *state);
    int	test_reduce(state_t *state);
    int	test_scan(state_t *state);
    int	test_barrier(state_t *state);
    int	test_bcast(state_t *state);
    int	test_mcast(state_t *state);
    int	test_monte_carlo(state_t *state);

/*
 * Utilities (util.c)
 */

    void	barrier(int group);
    void	print_in_order(int group, char *format, ...);
    void	print_root(int group, char *format, ...);
    void	print_simple(int group, char *format, ...);
    void	print_status(int group, ULMStatus_t *status);
    double	second(void);
    void	set_data(int self, int seq, unsigned char pattern,
                         unsigned char *buf, size_t size);
    void	check_data(int peer, int seq, unsigned char pattern,
                           unsigned char *buf, size_t size,
                           char *file, int line);
    void	check_ulm(int rc, char *expr, char* file, int line);

#ifdef	__cplusplus
}
#endif

#endif /* ULMTEST_H_INCLUDED */
