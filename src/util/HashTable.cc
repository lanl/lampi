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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 *  hashtable.cpp
 *  HashTest
 *
 *  Created by Rob Aulwes on Fri Dec 06 2002.
 */

#include <strings.h>

#include "HashTable.h"
#include "internal/Private.h"
#include "internal/malloc.h"


typedef struct
{
    HashNode    *head;
    HashNode    *tail;

} bucket_t;

#define buckets         ((bucket_t *)buckets_m)
#define HASH_NODE_POOL_SIZE 256


HashTable::HashTable(int numBuckets)
{
    long int            sz;

    freeFunc_m = NULL;
    sz_m = numBuckets;
    sz = sizeof(bucket_t) * sz_m;
    buckets_m = (bucket_t *)ulm_malloc(sz);

    bzero(buckets_m, sz);
    cnt_m = 0;
    mask_m = numBuckets - 1;      /* must be updated if table size changes. */
    freeList_m = 0;
    freeCnt_m = 0;
}


HashTable::~HashTable()
{
    bucket_t    *aBucket;
    HashNode    *ptr, *nxt;
    long int    cnt;

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
                freeFunc_m(ptr->value()->value());      /* ptr->value() is HashValue object. */

            delete ptr;
            ptr = nxt;
        }
        aBucket++;
    }
    ulm_free2(buckets);
}


void HashTable::setValueForKey(HashValue *value, HashKey *key)
{
    long int    hval;
    bucket_t    *aBucket;
    HashNode    *ptr;

    /* ASSERT: table size is power of 2. */
    hval = key->hashValue() & mask_m;
    aBucket = buckets + hval;
    if ( NULL == aBucket->head )
    {
        /* create collision list. */
        aBucket->head = aBucket->tail = getHashNode(key,value);
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
                    freeFunc_m(ptr->value()->value());
                ptr->setValue(value);
                return;
            }
            ptr = ptr->nextNode();
        }
        if ( NULL == ptr )
        {
            ptr = getHashNode(key, value);
            aBucket->tail->setNextNode(ptr);
            aBucket->tail = ptr;
        }
    }
    cnt_m++;
}


void HashTable::setValueForKey(void *value, int key)
{
    HashKeyInteger ikey(key);
    this->setValueForKey(value, &ikey);
}

void HashTable::setValueForKey(void *value, const char *key)
{
    HashKeyString skey(key);
    this->setValueForKey(value, &skey);
}

void HashTable::setValueForKey(const char *value, const char *key)
{
    HashValueString             val(value);
    HashKeyString               ikey(key);
    this->setValueForKey(&val, &ikey);
}


void HashTable::setValueForKey(void *value, HashKey *key)
{
    HashValue   val(value);
    this->setValueForKey(&val, key);
}


void *HashTable::valueForKey(HashKey *key)
{
    void                *item = NULL;
    long int    hval;
    bucket_t    *aBucket;
    HashNode    *ptr;

    hval = key->hashValue() & mask_m;
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
    HashKeyString ikey(key);
    return this->valueForKey(&ikey);
}

void *HashTable::valueForKey(int key)
{
    HashKeyInteger ikey(key);
    return this->valueForKey(&ikey);
}

HashValue **HashTable::allValues()
{
    long int    i, cnt;
    bucket_t    *aBucket;
    HashNode    *ptr;
    HashValue 	**values;

    if ( !cnt_m )
        return NULL;
    
    if ( !(values = (HashValue **)ulm_malloc(cnt_m * sizeof(HashValue *))) )
        return NULL;

    cnt = 0;
    i = 0;
    while ( (cnt < cnt_m) && (i < sz_m) )
    {
        aBucket = buckets + i;
        ptr = aBucket->head;
        while ( (ptr != NULL) && (cnt < cnt_m) )
        {
            values[cnt++] = ptr->value();
            ptr = ptr->nextNode();
        }
        i++;
    }
    return values;
}


void HashTable::removeValueForKey(int key)
{
    HashKeyInteger ikey(key);
    this->removeValueForKey(&ikey);
}


void HashTable::removeValueForKey(const char* key)
{
    HashKeyString skey(key);
    this->removeValueForKey(&skey);
}


void HashTable::removeValueForKey(HashKey *key) 
{
    long int    hval;
    bucket_t    *aBucket;
    HashNode    *ptr, *prev;

    hval = key->hashValue() & mask_m;   /* same as mod table size. */
    aBucket = buckets + hval;
    prev = 0;
    ptr = aBucket->head;
    while ( ptr != 0 )
    {
        if ( true == ptr->key()->isEqual(key) )
        {
            /* set previous node to point to current node's
               next node. Also check if we're removing from end
               of list. */
         
            if(ptr == aBucket->head) {
                aBucket->head = ptr->nextNode();
                if(ptr == aBucket->tail)
                    aBucket->tail = aBucket->head;
            } else {
                prev->setNextNode(ptr->nextNode());
                if( ptr == aBucket->tail )
                    aBucket->tail = prev;
            }

            if(freeCnt_m < HASH_NODE_POOL_SIZE) {
                ptr->deleteKeyAndValue();
                ptr->setNextNode(freeList_m);
                freeList_m = ptr;
                freeCnt_m++;
            } else {
                delete ptr;
            }
            cnt_m--;
            break;
        }
        prev = ptr;
        ptr = ptr->nextNode();
    }
}

HashNode* HashTable::getHashNode(HashKey *key, HashValue* val)
{
    HashNode *hashNode = 0;
    if(freeList_m != 0) {
        hashNode = freeList_m;
        freeList_m = hashNode->nextNode();
        freeCnt_m--;
        hashNode->setNextNode(0);
        hashNode->setKeyAndValue(key,val);
    } else {
        hashNode = new HashNode(key, val);
    }
    return hashNode;
}


