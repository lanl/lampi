/*
 * Copyright 2002-2003. The Regents of the University of California. This material 
 * was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
 * National Laboratory, which is operated by the University of California for 
 * the U.S. Department of Energy. The Government is granted for itself and 
 * others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
 * license in this material to reproduce, prepare derivative works, and 
 * perform publicly and display publicly. Beginning five (5) years after 
 * October 10,2002 subject to additional five-year worldwide renewals, the 
 * Government is granted for itself and others acting on its behalf a paid-up, 
 * nonexclusive, irrevocable worldwide license in this material to reproduce, 
 * prepare derivative works, distribute copies to the public, perform publicly 
 * and display publicly, and to permit others to do so. NEITHER THE UNITED 
 * STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
 * CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
 * IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
 * PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
 * OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/



#include <assert.h>
#include <string.h>

#include "internal/malloc.h"
#include "internal/mpi.h"

static int *allocate_prime_factor_array(int n, int *nprime);

#pragma weak MPI_Dims_create = PMPI_Dims_create
int PMPI_Dims_create(int nnodes, int ndims, int *dims)
{
    int nfreeprocs;
    int nfreedims;
    int nprimes;
    int i;
    int j;
    int rc;
    int *primes;
    int *freedims;

    /*
     * Sanity checks and special cases
     */

    rc = MPI_SUCCESS;
    primes = NULL;
    freedims = NULL;

    if ((nnodes <= 0) || (ndims < 1) || (dims == 0)) {
	rc = MPI_ERR_DIMS;
	goto ERRHANDLER;
    }

    nfreeprocs = nnodes;
    nfreedims = 0;
    for (i = 0; i < ndims; ++i) {
	if (dims[i] == 0) {
	    nfreedims++;
	} else if (dims[i] < 0) {
	    rc = MPI_ERR_DIMS;
	    goto ERRHANDLER;
	} else if ((nnodes % dims[i]) != 0) {
	    rc = MPI_ERR_DIMS;
	    goto ERRHANDLER;
	} else {
	    nfreeprocs /= dims[i];
	}
    }

    if (nfreedims == 0) {
	if (nfreeprocs == 1) {
	    return MPI_SUCCESS;
	}
	rc = MPI_ERR_DIMS;
	goto ERRHANDLER;
    }

    if (nfreeprocs < 1) {
	rc = MPI_ERR_DIMS;
	goto ERRHANDLER;
    } else if (nfreeprocs == 1) {
	for (i = 0; i < ndims; ++i) {
	    if (dims[i] == 0) {
		dims[i] = 1;
	    }
	}
	return MPI_SUCCESS;
    }

    /*
     * If we get here we must have a non-trivial case...
     *
     * Get prime factors of nfreeprocs
     */

    primes = allocate_prime_factor_array(nfreeprocs, &nprimes);
    if (primes == (int *) NULL) {
	rc = MPI_ERR_OTHER;
	goto ERRHANDLER;
    }

    /*
     * Assign free procs to the free dimensions.
     *
     * Goal is to distribute the processes such that the (free) dims are
     * as close to each other as possible and in descending order.
     *
     * Achieve this as follows:
     * - allocate temporary dims array, freedims
     * - initialize freedims to all 1's
     * - fill it with products of the prime factors: assign the prime
     *   factors to freedims in descending order, always selecting the
     *   currently smallest element of freedims to receive the next prime.
     * - sort freedims in place (ndims small: simple exchange algorithm)
     * - put the sorted freedims into the free slots in the dims array
     */

    freedims = (int *) ulm_malloc(nfreedims * sizeof(int));
    if (freedims == (int *) NULL) {
	rc = MPI_ERR_OTHER;
	goto ERRHANDLER;
    }

    for (j = 0; j < nfreedims; j++) {
	freedims[j] = 1;
    }

    for (i = 0; i < nprimes; i++) {
	int smallest = 0;
	for (j = 1; j < nfreedims; j++) {
	    if (freedims[j] < freedims[smallest]) {
		smallest = j;
	    }
	}
	freedims[smallest] *= primes[i];
    }

    for (i = 0; i < nfreedims; i++) {
	for (j = i + 1; j < nfreedims; j++) {
	    if (freedims[j] > freedims[i]) {
		int tmp = freedims[i];
		freedims[i] = freedims[j];
		freedims[j] = tmp;
	    }
	}
    }

    j = 0;
    for (i = 0; i < ndims; ++i) {
	if (dims[i] == 0) {
	    dims[i] = freedims[j++];
	}
    }

ERRHANDLER:

    if (freedims) {
	ulm_free(freedims);
    }
    if (primes) {
	ulm_free(primes);
    }

    if (rc != MPI_SUCCESS) {
	_mpi_errhandler(MPI_COMM_WORLD, rc, __FILE__, __LINE__);
    }

    return rc;
}


/*
 * allocate_prime_factor_array():
 *
 * \param n		The integer to factorize. Must be greater than 1.
 * \param nprime	On exit, contains the number of prime factors.
 * \return		Pointer to the allocated array of prime factors.
 *
 * Allocate an integer array and fill it with n's prime factors.
 *
 * - Order is ascending or descending as determined by the compile time
 *   ORDER enum (compiler should optimize the other option away).
 *
 * - A primitive, exhaustive algorithm, but adequate for the relatively
 *   small integers ( < 100k) likely to occur here.
 */
static int *allocate_prime_factor_array(int n, int *nprime)
{
    enum { ASCENDING = 0, DESCENDING };
    enum { ORDER = DESCENDING };

    int count;
    int factor;
    int remainder;
    int product;
    int trial_remainder;
    int *prime;

    /*
     * pre-conditions
     */

    assert(n > 1);
    assert(nprime != (int *) NULL);

    /*
     * loop once to count
     */

    count = 0;
    remainder = n;
    product = 1;
    factor = 2;
    do {
	trial_remainder = remainder / factor;
	while (remainder == (trial_remainder * factor)) {
	    remainder = trial_remainder;
	    trial_remainder = remainder / factor;
	    product *= factor;
	    count++;
	}
	factor++;
    } while (product < n);

    /*
     * loop post-conditions
     */

    assert(product == n);
    assert(count > 0);

    /*
     * allocate space
     */

    prime = (int *) ulm_malloc(count * sizeof(int));
    if (prime == (int *) NULL) {
	return (int *) NULL;
    }
    memset(prime, 0, count * sizeof(int));
    *nprime = count;

    /*
     * loop again to fill in the factors (largest first)
     */

    if (ORDER == ASCENDING) {
	count = 0;
    }
    remainder = n;
    product = 1;
    factor = 2;
    do {
	trial_remainder = remainder / factor;
	while (remainder == (trial_remainder * factor)) {
	    remainder = trial_remainder;
	    trial_remainder = remainder / factor;
	    product *= factor;
	    if (ORDER == ASCENDING) {
		prime[count++] = factor;
	    } else if (ORDER == DESCENDING) {
		prime[--count] = factor;
	    }
	}
	factor++;
    } while (product < n);

    /*
     * post-conditions
     */

    assert(product == n);
    assert(*nprime > 0);
    assert(prime != (int *) NULL);

    return prime;
}
