#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include "internal/log.h"
#include "internal/types.h"
#include "util/Links.h"
#include "util/DblLinkList.h"
#include "util/if.h"


struct Interface : public Links_t {
    InterfaceName_t     if_name;
    int                 if_index;
    int                 if_flags;
    struct sockaddr_in  if_addr;
    struct sockaddr_in  if_mask;
};
static DoubleLinkList ifList;


//
//  Discover the list of configured interfaces. Don't care about any
//  interfaces that are not up or are local loopbacks.
//

extern "C" int ulm_ifinit() 
{
    if (ifList.size() > 0)
        return ULM_SUCCESS;

    char buff[1024];
    char *ptr;
    struct ifconf ifconf;
    ifconf.ifc_len = sizeof(buff);
    ifconf.ifc_buf = buff;
                                                                                
    int sd;
    if((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        ulm_err(("ulm_ifinit: socket() failed with errno=%d\n", errno));
        return ULM_ERROR;
    }
                                                                                
    if(ioctl(sd, SIOCGIFCONF, &ifconf) < 0) {
        ulm_err(("ulm_ifinit: ioctl(SIOCGIFCONF) failed with errno=%d", errno));
        close(sd);
        return ULM_ERROR;
    }

    for(ptr = buff; ptr < buff + ifconf.ifc_len; ) {
        struct ifreq& ifr = *(struct ifreq*)ptr;
#if defined(__APPLE__)
        ptr += (sizeof(ifr.ifr_name) + 
               MAX(sizeof(struct sockaddr),ifr.ifr_addr.sa_len));
#else
        switch(ifr.ifr_addr.sa_family) {
        case AF_INET6:
            ptr += sizeof(ifr.ifr_name) + sizeof(struct sockaddr_in6);
            break;
        case AF_INET:
        default:
            ptr += sizeof(ifr.ifr_name) + sizeof(struct sockaddr);
            break;
        }
#endif
        if(ifr.ifr_addr.sa_family != AF_INET)
            continue;

        if(ioctl(sd, SIOCGIFFLAGS, &ifr) < 0) {
            ulm_err(("ulm_ifinit: ioctl(SIOCGIFFLAGS) failed with errno=%d", errno));
            continue;
        }
        if((ifr.ifr_flags & IFF_UP) == 0 || (ifr.ifr_flags & IFF_LOOPBACK) != 0)
            continue;
                                                                                
        Interface intf;
        strcpy(intf.if_name, ifr.ifr_name);
        intf.if_flags = ifr.ifr_flags;

#if defined(__APPLE__)
        intf.if_index = ifList.size() + 1;
#else
        if(ioctl(sd, SIOCGIFINDEX, &ifr) < 0) {
            ulm_err(("ulm_ifinit: ioctl(SIOCGIFINDEX) failed with errno=%d", errno));
            continue;
        }
#if defined(ifr_ifindex)
        intf.if_index = ifr.ifr_ifindex;
#elif defined(ifr_index)
        intf.if_index = ifr.ifr_index;
#else
        intf.if_index = -1;
#endif
#endif /* __APPLE__ */

        if(ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
            ulm_err(("ulm_ifinit: ioctl(SIOCGIFADDR) failed with errno=%d", errno));
            break;
        }
        if(ifr.ifr_addr.sa_family != AF_INET) 
            continue;

        memcpy(&intf.if_addr, &ifr.ifr_addr, sizeof(intf.if_addr));
        if(ioctl(sd, SIOCGIFNETMASK, &ifr) < 0) {
            ulm_err(("ulm_ifinit: ioctl(SIOCGIFNETMASK) failed with errno=%d", errno));
            continue;
        }
        memcpy(&intf.if_mask, &ifr.ifr_addr, sizeof(intf.if_mask));
        ifList.Append(new Interface(intf));
    }
    close(sd);
    return ULM_SUCCESS;
}


//
//  Look for interface by name and returns its address 
//  as a dotted decimal formatted string.
//

int ulm_ifnametoaddr(const char* if_name, char* if_addr, int length)
{
    int rc = ulm_ifinit();
    if(rc != ULM_SUCCESS)
        return rc;

    for(Interface *intf =  (Interface*)ifList.begin(); 
                   intf != (Interface*)ifList.end();
                   intf =  (Interface*)intf->next) {
        if(strcmp(intf->if_name, if_name) == 0) {
            strncpy(if_addr, inet_ntoa(intf->if_addr.sin_addr), length);
            return ULM_SUCCESS;
        }
    }
    return ULM_ERROR;
}

//
//  Attempt to resolve the adddress as either a dotted decimal formated
//  string or a hostname and lookup corresponding interface.
//

int ulm_ifaddrtoname(const char* if_addr, char* if_name, int length)
{
    int rc = ulm_ifinit();
    if(rc != ULM_SUCCESS)
        return rc;

    in_addr_t inaddr = inet_addr(if_addr);
    if(inaddr == INADDR_ANY) {
        struct hostent *h = gethostbyname(if_addr);
        if(h == 0) {
            ulm_err(("ulm_ifaddrtoname: unable to resolve %s\n", if_addr));
            return ULM_ERROR;
        }
        memcpy(&inaddr, h->h_addr, sizeof(inaddr));
    }

    for(Interface *intf =  (Interface*)ifList.begin(); 
                   intf != (Interface*)ifList.end();
                   intf =  (Interface*)intf->next) {
        if(intf->if_addr.sin_addr.s_addr == inaddr) {
            strncpy(if_name, intf->if_name, length);
            return ULM_ERROR;
        }
    }
    return ULM_SUCCESS;
}

//
//  Return the number of discovered interface.
//

int ulm_ifcount()
{
    if(ulm_ifinit() != ULM_SUCCESS)
        return (-1);
    return ifList.size();
}


//
//  Return the kernels interface index for the first
//  interface in our list.
//

int ulm_ifbegin()
{
    if(ulm_ifinit() != ULM_SUCCESS)
        return (-1);
    Interface *intf = (Interface*)ifList.begin();
    if(intf != 0)
        return intf->if_index;
    return (-1);
}


//
//  Located the current position in the list by if_index and
//  return the interface index of the next element in our list
//  (if it exists).
//

int ulm_ifnext(int if_index)
{
    if(ulm_ifinit() != ULM_SUCCESS)
        return (-1);

    for(Interface *intf =  (Interface*)ifList.begin(); 
                   intf != (Interface*)ifList.end();
                   intf =  (Interface*)intf->next) {
        if(intf->if_index == if_index) {
            return (intf->next ? ((Interface*)intf->next)->if_index : -1);
        }
    }
    return (-1);
}


// 
//  Lookup the interface by kernel index and return the 
//  primary address assigned to the interface.
//

int ulm_ifindextoaddr(int if_index, char* if_addr, int length)
{
    int rc = ulm_ifinit();
    if(rc != ULM_SUCCESS)
        return rc;

    for(Interface *intf =  (Interface*)ifList.begin(); 
                   intf != (Interface*)ifList.end();
                   intf =  (Interface*)intf->next) {
        if(intf->if_index == if_index) {
            strncpy(if_addr, inet_ntoa(intf->if_addr.sin_addr), length);
            return ULM_SUCCESS;
        }
    }
    return ULM_ERROR;
}


// 
//  Lookup the interface by kernel index and return
//  the associated name.
//

int ulm_ifindextoname(int if_index, char* if_name, int length)
{
    int rc = ulm_ifinit();
    if(rc != ULM_SUCCESS)
        return rc;

    for(Interface *intf =  (Interface*)ifList.begin(); 
                   intf != (Interface*)ifList.end();
                   intf =  (Interface*)intf->next) {
        if(intf->if_index == if_index) {
            strncpy(if_name, intf->if_name, length);
            return ULM_SUCCESS;
        }
    }
    return ULM_ERROR;
}

