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

#ifndef _LINKED_LIST_H
#define _LINKED_LIST_H

#include <stdio.h>
#include <stdlib.h>

#include "internal/linkage.h"

CDECL_BEGIN

typedef struct link_t link_t;

struct link_t {
    void *data;
    link_t *next;
};

/* listsize: number of elements in list */
int listsize(link_t *listp);

/* newitem: create new item for data */
link_t *newlink(void *data);

/* addfront: add link_t to front of list */
link_t *addfront(link_t *listp, link_t *newp);

/* addend: add newp to end of listp */
link_t *addend(link_t *listp, link_t *newp);

/* delete_link: search list for link pointed to by lp and removes from list.
	use fn to free the data.
 */
link_t *delete_link(link_t *listp, link_t *lp, void (*fn)(void *));

/* lookup: sequential search for datum in listp
 * fn takes two pointers to data as arguments and
 * returns 1 if they are equal, zero otherwise */
link_t *lookup(link_t *listp, int (*fn)(void*, void*),
               void *datum);

/*
	uses fn to find item in list to remove.  If fn returns 1, then
	link containing item is removed from list and new list is returned
	in newlist.  then item is returned from function.
	fn takes two args: arg 1 is linked list item, arg2 is context.
*/
void *remove_item(link_t *listp, link_t **newlist, int (*fn)(void*, void*), void *context);

/* apply: execute fn for each element of listp */
void apply(link_t *listp, void (*fn)(link_t*, void*),
           void *arg);

/* freeall: free all elements of listp */
void freeall(link_t *listp);

/* freeall: free all elements of listp and use fn to free the data. 
	fn should expect the first argument to be the data item. */
void freeall_with(link_t *listp, void (*fn)(void *));

/* delitem: delete first occurence of 'datum'
 * from listp
 * fn takes two pointers to data as arguments
 * and returns 1 if they are equal, zero otherwise */
link_t *delitem(link_t *listp, int (*fn)(void*,void*),
                void *datum);

CDECL_END

#endif
