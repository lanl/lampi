/*
 *  hashtable.cpp
 *  HashTest
 *
 *  Created by Rob Aulwes on Fri Dec 06 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */

#include "HashTable.h"
#include "internal/Private.h"
#include "internal/malloc.h"

#define BLK_SIZE	64
#define COLL_SIZE	10

typedef struct
{
	HashNode	*head;
	HashNode	*tail;
	
} bucket_t;

#define buckets		((bucket_t *)buckets_m)



HashTable::HashTable()
{
	long int		sz;
	
	freeFunc_m = NULL;
	sz_m = BLK_SIZE;
	sz = sizeof(bucket_t) * sz_m;
    buckets_m = (bucket_t *)ulm_malloc(sz);
	
	bzero(buckets_m, sz);
	cnt_m = 0;
	mask_m = BLK_SIZE - 1;	/* must be updated if table size changes. */
}


HashTable::~HashTable()
{
	bucket_t	*aBucket;
	HashNode	*ptr, *nxt;
	long int	cnt;
	
	if ( !buckets ) return;
	
	cnt = 0;
	aBucket = buckets;
	while ( cnt++ < sz_m )
	{
		/* free collision list. */
		ptr = aBucket->head;
		while ( ptr )
		{
			/* ptr is a HashNode object. */
			nxt = ptr->nextNode();
			if ( freeFunc_m )
				freeFunc_m(ptr->value()->value());	/* ptr->value() is HashValue object. */
			
			delete ptr;
			ptr = nxt;
		}
		aBucket++;
	}
	free(buckets);
}


void HashTable::setValueForKey(HashValue *value, HashKey *key)
{
	long int	hval;
	bucket_t	*aBucket;
	HashNode	*ptr;
	
	/* ASSERT: table size is power of 2. */
	hval = key->hashValue() & mask_m;	/* same as mod table size. */
	aBucket = buckets + hval;
	if ( NULL == aBucket->head )
	{
		/* create collision list. */
		aBucket->head = aBucket->tail = new HashNode(key, value);
	}
	else
	{
		/* Check if key is same as existing key. */
		ptr = aBucket->head;
		while ( ptr != NULL )
		{
			/* ptr is a HashNode object. */
			if ( true == ptr->key()->isEqual(key) )
			{
				/* delete old value if required. */
				if ( freeFunc_m )
					freeFunc_m(ptr->value());
					
				ptr->setValue(value);
				break;
			}
			ptr = ptr->nextNode();
		}
		if ( NULL == ptr )
		{
			ptr = new HashNode(key, value);
			aBucket->tail->setNextNode(ptr);
			aBucket->tail = ptr;
		}
	}
	cnt_m++;
}


void HashTable::setValueForKey(void *value, int key)
{
	HashKeyInteger	ikey(key);
	this->setValueForKey(value, &ikey);
}

void HashTable::setValueForKey(void *value, const char *key)
{
	HashKeyString	ikey(key);
	this->setValueForKey(value, &ikey);
}

void HashTable::setValueForKey(const char *value, const char *key)
{
	HashValueString		val(value);
	HashKeyString		ikey(key);
	
	this->setValueForKey(&val, &ikey);
}


void HashTable::setValueForKey(void *value, HashKey *key)
{
	HashValue	val(value);
	this->setValueForKey(&val, key);
}


void *HashTable::valueForKey(HashKey *key)
{
	void		*item = NULL;
	long int	hval;
	bucket_t	*aBucket;
	HashNode	*ptr;

	hval = key->hashValue() & mask_m;	/* same as mod table size. */
	aBucket = buckets + hval;
	ptr = aBucket->head;
	while ( ptr != NULL )
	{
		if ( true == ptr->key()->isEqual(key) )
		{
			item = ptr->value()->value();
			break;
		}
		ptr = ptr->nextNode();
	}
	
	return item;
}


void *HashTable::valueForKey(const char *key)
{
	HashKeyString		ikey(key);
	return this->valueForKey(&ikey);
}

void *HashTable::valueForKey(int key)
{
	HashKeyInteger		ikey(key);
	return this->valueForKey(&ikey);
}


void HashTable::removeValueForKey(HashKey *key)
{
	long int	hval;
	bucket_t	*aBucket;
	HashNode	*ptr, *prev;

	hval = key->hashValue() & mask_m;	/* same as mod table size. */
	aBucket = buckets + hval;
	prev = ptr = aBucket->head;
	while ( ptr != NULL )
	{
		if ( true == ptr->key()->isEqual(key) )
		{
			/* set previous node to point to current node's
			next node. Also check if we're removing from end
			of list. */
			prev->setNextNode(ptr->nextNode());
			if ( aBucket->tail == ptr )
				aBucket->tail = prev;
			
			delete ptr;
			
			cnt_m--;
			break;
		}
		prev = ptr;
		ptr = ptr->nextNode();
	}
}



