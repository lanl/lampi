/*
 *  HashNode.cpp
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Fri Jan 10 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include "HashNode.h"

#define MULTIPLIER		31

long int HashKeyString::hashValue()
{
	long int		h;
	char			*p;
	
	h = 0;
	for (p = kval_m; *p; p++)
		h = MULTIPLIER*h + *p;
	
	return h;
}


HashValueString::HashValueString(const char *str) : HashValue(NULL)
{
	value_m = (void *)strdup(str);
}

HashValueString::~HashValueString()
{
	if ( value_m ) free((char *)value_m);
}


HashNode::HashNode(HashKey *key, HashValue *val)
{
	key_m = key->copy();
	val_m = val->copy();
	next_m = NULL;
}


void HashNode::deleteKeyAndValue()
{
    delete key_m; key_m = 0;
    delete val_m; val_m = 0;
}

void HashNode::setKeyAndValue(HashKey *key, HashValue *val)
{
    deleteKeyAndValue();
    key_m = key->copy();
    val_m = val->copy();
}

HashNode::~HashNode()
{
    delete key_m;
    delete val_m;
}

