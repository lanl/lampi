#ifndef _IF_UTIL_
#define _IF_UTIL_

#ifdef __cplusplus
extern "C" {
#endif

int ulm_ifnametoaddr(const char* ifName, char* ifAddr, int);
int ulm_ifaddrtoname(const char* ifAddr, char* ifName, int);
int ulm_ifnametoindex(const char* ifName);

int ulm_ifcount();
int ulm_ifbegin(); 
int ulm_ifnext(int ifIndex);

int ulm_ifindextoname(int ifIndex, char* ifName, int);
int ulm_ifindextoaddr(int ifIndex, char* ifAddr, int);

#ifdef __cplusplus
}
#endif
#endif

