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

#ifndef _Reactor_
#define _Reactor_
#include "util/DblLinkList.h"
#include "util/HashTable.h"
#include "util/Lock.h"
#include "util/Vector.h"


//
//  Utilizes select() to provide callbacks when an event (e.g. readable,writeable,exception)
//  occurs on a designated descriptor.  Objects interested in receiving callbacks must implement
//  the Listener interface.
//

class Reactor {
public:
    Reactor();

    static const int NotifyAll;
    static const int NotifyRecv;
    static const int NotifySend;
    static const int NotifyExcept;

    class Listener {
    public:
        Listener() {}
        virtual ~Listener() {}
        virtual void recvEventHandler(int sd) {}
        virtual void sendEventHandler(int sd) {}
        virtual void exceptEventHandler(int sd) {}
    };

    bool insertListener(int sd, Listener*, int flags);
    bool removeListener(int sd, Listener*, int flags);

    void poll();
    void run();

private:

    struct Descriptor : public Links_t {
        int sd;
        volatile int flags;
        Listener *recvListener;
        Listener *sendListener;
        Listener *exceptListener;
    };

    Locks              sd_lock;
    DoubleLinkList     sd_active;
    DoubleLinkList     sd_free;
    DoubleLinkList     sd_pending;
    HashTable          sd_table;
    int                sd_max;
    bool               sd_run;
    int                sd_changes;
    fd_set             sd_send_set;
    fd_set             sd_recv_set;
    fd_set             sd_except_set;

    void dispatch(int, fd_set&, fd_set&, fd_set&);
    inline Descriptor* getDescriptor(int);
};


inline Reactor::Descriptor* Reactor::getDescriptor(int sd)
{
    Descriptor *descriptor;
    if(sd_free.size())
        descriptor = (Descriptor*)sd_free.GetfirstElementNoLock();
    else
        descriptor = new Descriptor();
    if(descriptor == 0) {
        ulm_err(("Reactor::getDescriptor: new(%d) failed.", sizeof(Descriptor)));
        return 0;
    }
    descriptor->sd = sd;
    descriptor->flags = 0;
    descriptor->recvListener = 0;
    descriptor->sendListener = 0;
    descriptor->exceptListener = 0;
    return descriptor;
}


#endif
