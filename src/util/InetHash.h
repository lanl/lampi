#ifndef _InetHashKey_
#define _InetHashKey_

#include <netinet/in.h>
#include "util/HashNode.h"


class InetHashKey : public HashKey
{
public:
    InetHashKey(const struct sockaddr_in& key) : key_value(key) {}
    InetHashKey(const InetHashKey& key) : key_value(key.key_value) {}

    virtual long int hashValue() {return key_value.sin_addr.s_addr; }
    virtual HashKey *copy() { return new InetHashKey(key_value); }
    virtual const void* keyValue() { return &key_value; }
    virtual size_t keyLength() { return sizeof(struct sockaddr_in); }

private:
    struct sockaddr_in key_value;
};

#endif
