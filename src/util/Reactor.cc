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

#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include "util/Reactor.h"
#include "util/ScopedLock.h"
#include "util/Vector.h"


const int Reactor::NotifyRecv = 1;
const int Reactor::NotifySend = 2;
const int Reactor::NotifyExcept = 4;
const int Reactor::NotifyAll = 7;

#define MAX_DESCRIPTOR_POOL_SIZE 256



Reactor::Reactor() :
    sd_table(1024),
    sd_max(-1),
    sd_run(true),
    sd_changes(0)
{
    ULM_FD_ZERO(&sd_recv_set);
    ULM_FD_ZERO(&sd_send_set);
    ULM_FD_ZERO(&sd_except_set);
}


bool Reactor::insertListener(int sd, Listener* listener, int flags)
{
#ifndef NDEBUG
    if(sd < 0 || sd > ULM_FD_SETSIZE) {
        ulm_err(("Reactor::insertListener(%d) invalid descriptor.\n", sd));
        return false;
    }
#endif

    if(usethreads()) sd_lock.lock(); 
    Descriptor *descriptor = (Descriptor*)sd_table.valueForKey(sd);
    if(descriptor == 0) {
        descriptor = getDescriptor(sd);
        if(descriptor == 0) {
            if(usethreads()) sd_lock.unlock();
            return false;
        }
        sd_pending.AppendNoLock(descriptor);
        sd_table.setValueForKey(descriptor,sd);
    }

    descriptor->flags |= flags;
    if(flags & NotifyRecv) {
        descriptor->recvListener = listener;
        ULM_FD_SET(sd, &sd_recv_set);
    }
    if(flags & NotifySend) {
        descriptor->sendListener = listener;
        ULM_FD_SET(sd, &sd_send_set);
    }
    if(flags & NotifyExcept) {
        descriptor->exceptListener = listener;
        ULM_FD_SET(sd, &sd_except_set);
    }
    sd_changes++;
    if(usethreads()) sd_lock.unlock();
    return true;
}


bool Reactor::removeListener(int sd, Listener* listener, int flags)
{
#ifndef NDEBUG
    if(sd < 0 || sd > ULM_FD_SETSIZE) {
        ulm_err(("Reactor::insertListener(%d) invalid descriptor.\n", sd));
        return false;
    }
#endif

    if(usethreads()) sd_lock.lock();
    Descriptor* descriptor = (Descriptor*)sd_table.valueForKey(sd);
    if(descriptor == 0) {
        ulm_err(("Reactor::removeListener(%d): invalid descriptor.\n", sd));
        if(usethreads()) sd_lock.unlock();
        return false;
    }
    descriptor->flags &= ~flags;
    if(flags & NotifyRecv) {
        descriptor->recvListener = 0;
        ULM_FD_CLR(sd, &sd_recv_set);
    }
    if(flags & NotifySend) {
        descriptor->sendListener = 0;
        ULM_FD_CLR(sd, &sd_send_set);
    }
    if(flags & NotifyExcept) {
        descriptor->exceptListener = 0;
        ULM_FD_CLR(sd, &sd_except_set);
    }
    sd_changes++;
    if(usethreads()) sd_lock.unlock();
    return true;
}


void Reactor::poll()
{
    struct timeval tm;
    tm.tv_sec = 0;
    tm.tv_usec = 0;
    ulm_fd_set_t rset = sd_recv_set;
    ulm_fd_set_t sset = sd_send_set;
    ulm_fd_set_t eset = sd_except_set;
    int rc = select(sd_max+1, (fd_set*)&rset, (fd_set*)&sset, (fd_set*)&eset, &tm);
    if(rc < 0) {
        if(errno != EINTR)
           ulm_exit((-1, "Reactor::poll: select() failed with errno=%d\n", errno));
        return;
    }
    dispatch(rc, rset, sset, eset);
}


void Reactor::run()
{
    while(sd_run == true) {
        ulm_fd_set_t rset = sd_recv_set;
        ulm_fd_set_t sset = sd_send_set;
        ulm_fd_set_t eset = sd_except_set;
        int rc = select(sd_max+1, (fd_set*)&rset, (fd_set*)&sset, (fd_set*)&eset, 0);
        if(rc < 0) {
            if(errno != EINTR)
                ulm_exit((-1, "Reactor::run: select() failed with errno=%d\n", errno));
            continue;
        }
        dispatch(rc, rset, sset, eset);
    }
}


void Reactor::dispatch(int cnt, ulm_fd_set_t& rset, ulm_fd_set_t& sset, ulm_fd_set_t& eset)
{
    // walk through the active list w/out holding lock, as this thread
    // is the only one that modifies the active list. however, note
    // that the descriptor flags could have been cleared in a callback,
    // so check that the flag is still set before invoking the callbacks

    Descriptor *descriptor;
    for(descriptor =  (Descriptor*)sd_active.begin();
        descriptor != (Descriptor*)sd_active.end() && cnt > 0;
        descriptor =  (Descriptor*)descriptor->next) {
        int sd = descriptor->sd; 
        int flags = 0;
        if(ULM_FD_ISSET(sd, &rset) && descriptor->flags & NotifyRecv) {
            descriptor->recvListener->recvEventHandler(sd);
            flags |= NotifyRecv;
        }
        if(ULM_FD_ISSET(sd, &sset) && descriptor->flags & NotifySend) {
            descriptor->sendListener->sendEventHandler(sd);
            flags |= NotifySend;
        }
        if(ULM_FD_ISSET(sd, &eset) && descriptor->flags & NotifyExcept) {
            descriptor->exceptListener->exceptEventHandler(sd);
            flags |= NotifyExcept;
        }
        if(flags) cnt--;
    }

    if(usethreads()) {
        sd_lock.lock();
        if(sd_changes == 0) {
            sd_lock.unlock();
            return;
        }
    } else if (sd_changes == 0)
        return;

    // cleanup any pending deletes while holding the lock
    descriptor = (Descriptor*)sd_active.begin();
    while(descriptor != (Descriptor*)sd_active.end()) {
        Descriptor* next = (Descriptor*)descriptor->next;
        if(descriptor->flags == 0) {
            sd_table.removeValueForKey(descriptor->sd);
            sd_active.RemoveLinkNoLock(descriptor);
            if(sd_free.size() < MAX_DESCRIPTOR_POOL_SIZE) {
                sd_free.AppendNoLock(descriptor);
            } else {
                delete descriptor;
            }
        } 
        descriptor = next;
    } 

    // add any other pending inserts/deletes
    while(sd_pending.size()) {
        Descriptor* descriptor = (Descriptor*)sd_pending.GetfirstElementNoLock();
        if(descriptor->flags == 0) {
            sd_table.removeValueForKey(descriptor->sd);
            if(sd_free.size() < MAX_DESCRIPTOR_POOL_SIZE) {
                sd_free.AppendNoLock(descriptor);
            } else {
                delete descriptor;
            }
        } else {
            sd_active.AppendNoLock(descriptor);
            if(descriptor->sd > sd_max)
               sd_max = descriptor->sd;
        }
    }

    sd_changes = 0;
    if(usethreads()) sd_lock.unlock();
}



#ifndef DISABLE_UNIT_TEST

class TestListener : public Reactor::Listener {
public:
   
    TestListener(Reactor& r) :
        reactor(r),
        recvCount(0),
        sendCount(0),
        exceptCount(0)
    {
    }

    virtual void recvEventHandler(int sd) {
        if(recvCount % 2 == 0) {
           insert();
           remove(sd);
        }
        recvCount++;
    }
    virtual void sendEventHandler(int sd) {
        if(sendCount % 2 == 0) {
           insert();
           remove(sd);
        }
        sendCount++;
    }
    virtual void exceptEventHandler(int sd) {
        if(exceptCount % 2 == 0) {
           insert();
           remove(sd);
        }
        exceptCount++;
    }

    void remove(int sd) {
        reactor.removeListener(sd, this, Reactor::NotifyAll);
        close(sd);
    }
    
    void insert() {
        int sd = socket(AF_UNIX, SOCK_DGRAM, 0);
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0;
        bind(sd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
        reactor.insertListener(sd, this, Reactor::NotifyAll);
    }
        
    Reactor& reactor;
    long recvCount;
    long sendCount;
    long exceptCount;

};


#define TEST_ITERATIONS 1024
extern "C" int utest_reactor()
{
    Reactor reactor;
    Vector<TestListener*> listeners;
    listeners.size(256);

    size_t i;
    for(i=0; i<listeners.size(); i++) {
        TestListener* listener = new TestListener(reactor);
        listener->insert();
        listeners[i] = listener;
    }
    for(i=0; i<TEST_ITERATIONS; i++) {
        reactor.poll();
    }

    for(i=0; i<listeners.size(); i++) {
        TestListener *listener = listeners[i];
        if(listener->sendCount != TEST_ITERATIONS ||
           listener->exceptCount != 0 || 
           listener->recvCount != 0) {
           return(-1);
        }
    }
    return(0);
}

#endif

