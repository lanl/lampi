/*
 *  HashNode.h
 *  ScalableStartup
 *
 *  Created by Rob Aulwes on Fri Jan 10 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include <internal/MPIIncludes.h>

#ifndef _HASH_NODE_H_
#define _HASH_NODE_H_

/*
HashKey also acts as an interface for other key definitions.  If you need
to define another class to be used as a key, then simply subclass from
HashKey using multiple inheritance and override the necessary methods.
Ex:
class Foo : public Bar, public HashKey
{
	public:
	long int hashValue();
	inline bool isEqual(HashKey *key);	
};
*/

/*
 *
 *		Class HashKey
 *
 */
 
class HashKey
{
	public:
        virtual ~HashKey() {}
	virtual long  hashValue() = 0;
	virtual HashKey *copy() = 0;
        virtual const void* keyValue() = 0;
        virtual size_t keyLength() = 0;
	virtual bool isEqual(HashKey *key) { 
            return (key->keyLength() == keyLength()) && 
            (memcmp(key->keyValue(), keyValue(), keyLength()) == 0); 
        }
};


class HashKeyInteger : public HashKey
{
	private:
	long int	kval_m;
	
	public:
	HashKeyInteger(long int kval) : kval_m(kval) {}
	
	virtual long int hashValue() {return kval_m;}
	virtual HashKey *copy() { return new HashKeyInteger(kval_m);}
	virtual const void* keyValue() { return &kval_m; }
        virtual size_t keyLength() { return sizeof(kval_m); }
};


class HashKeyString : public HashKey
{
	private:
	char	*kval_m;
	
	public:
	HashKeyString(const char *val)
	{
		if ( val )
            kval_m = strdup(val);
        else
            kval_m = strdup("");
	}
	
	virtual ~HashKeyString() {if ( kval_m ) free(kval_m);}
	virtual long int hashValue();
	virtual HashKey *copy() { return new HashKeyString(kval_m); }
	virtual const void* keyValue() { return kval_m; }
        virtual size_t keyLength() { return (kval_m != 0) ? strlen(kval_m) : 0; }
};




/*
 *
 *		Class HashValue
 *
 */
 
class HashValue
{
	protected:
	void		*value_m;
	
	public:
	HashValue(void *value) : value_m(value) {}
	inline virtual HashValue *copy() {return new HashValue(value_m);}

	inline virtual void *value() {return value_m;}
	inline virtual void setValue(void *value) {value_m = value;}
};


class HashValueString : public HashValue
{
	public:
	HashValueString(const char *str);
	virtual ~HashValueString();
	
	inline virtual HashValue *copy() {return new HashValueString((const char *)value_m);}
	inline const char *stringValue() {return (const char *)value_m;}
};



/*
 *
 *		Class HashNode
 *
 */
 
class HashNode 
{
	private:
	HashKey			*key_m;
	HashValue		*val_m;
        HashNode                *next_m;
	
	public:
	HashNode(HashKey *key, HashValue *val);
	~HashNode();
	
	void deleteKeyAndValue();
	void setKeyAndValue(HashKey *key, HashValue *val);

	inline  HashKey *key() {return key_m;}
	inline  HashValue *value() {return val_m;}
	inline  void setValue(HashValue *val) {delete val_m; val_m = val->copy();}
	
	inline  HashNode *nextNode() {return next_m;}
	inline void setNextNode(HashNode *node) {next_m = node;}
};


#endif
