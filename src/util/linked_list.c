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

#include "internal/malloc.h"
//#include "util/linked_list.h"
#include "linked_list.h"

/* listsize: number of elements in list */
int listsize(link_t *listp)
{
    int sz = 0;
    link_t *l;

    for (l = listp; l != NULL; l = l->next) {
	sz++;
    }
    return sz;
}

/* newitem: create new item for data */
link_t *newlink(void *data)
{
    link_t *newp;

    newp = (link_t *) ulm_malloc(sizeof(link_t));
    if (newp == NULL) {
	perror("Error allocating new link");
	return NULL;
    }
    newp->data = data;
    newp->next = NULL;
    return newp;
}

/* addfront: add link to front of list */
link_t *addfront(link_t *listp, link_t *newp)
{
    newp->next = listp;
    return newp;
}

/* addend: add newp to end of listp */
link_t *addend(link_t *listp, link_t *newp)
{
    link_t *p;

    if (listp == NULL)
	return newp;
    for (p=listp; p->next != NULL; p=p->next)
	;
    p->next = newp;
    return listp;
}

/* delete_link: search list for link pointed to by lp and removes from list. */
link_t *delete_link(link_t *listp, link_t *lp, void (*fn)(void *))
{
	link_t		*ptr, *prev;
	
	/* check if deleting first item on list */
	if ( listp == lp )
	{
		ptr = listp->next;
		if ( fn )
			fn(listp->data);
		ulm_free2(listp);
		return ptr;
	}
	
	prev = ptr = listp;
	while ( ptr )
	{
		if ( ptr == lp )
		{
			prev->next = ptr->next;
			if ( fn )
				fn(ptr->data);
			ulm_free2(ptr);
			ptr = NULL;
		}
		else
		{
			prev = ptr;
			ptr = ptr->next;
		}
	}
	return listp;
}


/* lookup: sequential search for datum in listp
 * fn takes two pointers to data as arguments and
 * returns 1 if they are equal, zero otherwise */
link_t *lookup(link_t *listp, int (*fn)(void*, void*),
	       void *datum)
{
    for ( ; listp!=NULL; listp=listp->next) {
	if ((*fn)(listp->data, datum) == 1) {
	    return listp;
	}
    }
    return NULL; /* no match */
}

/*
	uses fn to find item in list to remove.  If fn returns 1, then
	link containing item is removed from list and new list is returned
	in newlist.  then item is returned from function.
*/
void *remove_item(link_t *listp, link_t **newlist, int (*fn)(void*, void*), void *context)
{
	void	*item;
	link_t	*ptr;
	
	item = NULL;
	*newlist = listp;
	ptr = NULL;
    for ( ; listp != NULL; listp = listp->next )
	{
		if ((*fn)(listp->data, context) == 1) 
		{
			item = listp->data;
			// check if we're removing first item in list.
			if ( ptr )
				ptr->next = listp->next;
			else
				*newlist = listp->next;
			ulm_free2(listp);
			break;
		}
		ptr = listp;
    }
	
    return item; /* no match */
}


/* apply: execute fn for each element of listp */
void apply(link_t *listp, void (*fn)(link_t*, void*),
	   void *arg)
{
    for ( ; listp!=NULL; listp=listp->next)
	(*fn)(listp, arg); /* call the function */
}

/* freeall: free all elements of listp */
void freeall(link_t *listp)
{
    link_t *next;
    for ( ; listp!=NULL; listp=next) {
	next = listp->next;
	if (listp->data) {
	    free(listp->data);
	    listp->data = 0;
	}
	ulm_free(listp);
    }
}

/* freeall: free all elements of listp and use fn to free the data. 
	fn should expect the first argument to be the data item. */
void freeall_with(link_t *listp, void (*fn)(void *))
{
    link_t *next;
    for ( ; listp!=NULL; listp=next) {
	next = listp->next;
	if (listp->data) {
	    if ( fn )
			(*fn)(listp->data);
	    listp->data = 0;
	}
	ulm_free(listp);
    }
}

/* delitem: delete first occurence of 'datum'
 * from listp
 * fn takes two pointers to data as arguments
 * and returns 1 if they are equal, zero otherwise */
link_t *delitem(link_t *listp, int (*fn)(void*,void*),
                void *datum)
{
    link_t *p, *prev;

    prev = NULL;
    for (p=listp; p!=NULL; p=p->next) {
	if ((*fn)(p->data, datum) == 1) {
	    if (prev == NULL) {
		listp = p->next;
	    }
	    else {
		prev->next = p->next;
	    }
	    if (p->data) {
		ulm_free(p->data);
		p->data = 0;
	    }
	    ulm_free(p);
	    return listp;
	}
	prev = p;
    }
    fprintf(stderr, "delitem: data not in list\n");
    return NULL;
}
