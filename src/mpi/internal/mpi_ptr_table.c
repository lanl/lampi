/*
 * Copyright 2002-2003. The Regents of the University of
 * California. This material was produced under U.S. Government
 * contract W-7405-ENG-36 for Los Alamos National Laboratory, which is
 * operated by the University of California for the U.S. Department of
 * Energy. The Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, and
 * perform publicly and display publicly. Beginning five (5) years
 * after October 10,2002 subject to additional five-year worldwide
 * renewals, the Government is granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable worldwide license
 * in this material to reproduce, prepare derivative works, distribute
 * copies to the public, perform publicly and display publicly, and to
 * permit others to do so. NEITHER THE UNITED STATES NOR THE UNITED
 * STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF CALIFORNIA, NOR
 * ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 * ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT,
 * OR PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE
 * PRIVATELY OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or any later version.  Accordingly, this
 * program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/*
 * MPI Fortran: miscellaneous utilities
 */

#include "internal/mpif.h"

/*
 * add a pointer to dynamic pointer table
 */
int _mpi_ptr_table_add(ptr_table_t *table, void *ptr)
{
    void **p;
    int	index, i;
    enum { TABLE_INIT = 1, TABLE_GROW = 2 };	/* ??? increase these after debug ??? */

    assert(table != NULL);

    _mpi_lock(&(table->lock));

    if (table->addr == NULL) {

	/*
	 * first time
	 */

        if (_MPI_DEBUG) {
	    _mpi_dbg("_mpi_ptr_table_add: initializing table (0x%p)\n", table);
        }

	p = ulm_malloc(TABLE_INIT * sizeof(void *));
	if (p == NULL) {
	    return -1;
	}
	table->lowest_free = 0;
	table->number_free = TABLE_INIT;
	table->size = TABLE_INIT;
	table->addr = p;
        for (i = 0; i < table->size; i++) {
	    table->addr[i] = NULL;
	}

    } else if (table->number_free == 0) {

	/*
	 * grow table
	 */

        if (_MPI_DEBUG) {
	    _mpi_dbg("_mpi_ptr_table_add: growing table (0x%p): %d -> %d\n",
		     table, table->size, table->size * TABLE_GROW);
        }

	p = realloc(table->addr, TABLE_GROW * table->size * sizeof(void *));
	if (p == NULL) {
        _mpi_unlock(&(table->lock));
	    return -1;
	}
	table->lowest_free = table->size;
	table->number_free = (TABLE_GROW - 1) * table->size;
	table->size *= TABLE_GROW;
	table->addr = p;
        for (i = table->lowest_free; i < table->size; i++) {
	    table->addr[i] = NULL;
	}
    }

    assert(table->addr != NULL);
    assert(table->size > 0);
    assert(table->lowest_free >= 0);
    assert(table->lowest_free < table->size);
    assert(table->number_free > 0);
    assert(table->number_free <= table->size);

    /*
     * add pointer to table, and return the index
     */

    index = table->lowest_free;
    assert(table->addr[index] == NULL);
    table->addr[index] = ptr;
    table->number_free--;
    if (table->number_free > 0) {
        for (i = table->lowest_free + 1; i < table->size; i++) {
	    if (table->addr[i] == NULL) {
	        table->lowest_free = i;
	        break;
	    }
        }
    }
    else {
        table->lowest_free = table->size;
    }

    if (_MPI_DEBUG) {
        _mpi_dbg("_mpi_ptr_table_add: adding 0x%p at index %d in table 0x%p\n",
		 ptr, index, table);
    }

    _mpi_unlock(&(table->lock));

    return index;
}

/*
 * free a slot in dynamic pointer table for reuse
 */
int _mpi_ptr_table_free(ptr_table_t *table, int index)
{
    /* Fortran MPI_REQUEST_NULL is -1 */
    if (index == -1)
        return 0;

    assert(table != NULL);
    assert(table->addr != NULL);
    assert(index >= 0);
    assert(index < table->size);

    if (_MPI_DEBUG) {
	_mpi_dbg("_mpi_ptr_table_free: freeing %d (former contents 0x%p)\n",
		 index, table->addr[index]);
    }

    _mpi_lock(&(table->lock));

    table->addr[index] = NULL;
    if (index < table->lowest_free) {
	table->lowest_free = index;
    }
    table->number_free++;

    if (_MPI_DEBUG) {
	_mpi_dbg("_mpi_ptr_table_free: "
		 "table size %ld, lowest free %ld, number free %ld\n",
		 table->size, table->lowest_free, table->number_free);
    }

    _mpi_unlock(&(table->lock));

    return 0;
}

/*
 * lookup pointer by index in pointer table
 */
void *_mpi_ptr_table_lookup(ptr_table_t *table, int index)
{
    void *p;

    if (_MPI_DEBUG) {
	_mpi_dbg("_mpi_ptr_table_lookup:\n");
    }

    _mpi_lock(&(table->lock));

    assert(table != NULL);
    assert(table->addr != NULL);
    assert(index >= 0);
    assert(index < table->size);

    p = table->addr[index];

    _mpi_unlock(&(table->lock));

    return p;
}
