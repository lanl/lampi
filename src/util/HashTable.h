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

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#include "internal/MPIIncludes.h"
#include "util/HashNode.h"


typedef void    (*free_value_fn)(void *);

class HashTable
{
protected:
    void            *buckets_m;
    long int        cnt_m;
    long int        mask_m;
    long int        sz_m;
    free_value_fn   freeFunc_m;
        
public:
    HashTable();
    virtual ~HashTable();
        
    void setValueForKey(void *value, HashKey *key);
    void setValueForKey(void *value, int key);
    void setValueForKey(void *value, const char *key);
    void setValueForKey(const char *value, const char *key);
    void setValueForKey(HashValue *value, HashKey *key);

    void *valueForKey(const char *key);
    void *valueForKey(int key);
    void *valueForKey(HashKey *key);

    HashValue **allValues();
    /*
     POST:	Returns an array of all hash table values.  User is
     responsible for freeing array.
     */
    
    void removeValueForKey(HashKey *key);
        
    void setFreeFunction(free_value_fn fn) {freeFunc_m = fn;}
    
    long int count() {return cnt_m;}
};

#endif
