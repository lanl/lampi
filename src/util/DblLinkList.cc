/*
 * Copyright 2002-2003. The Regents of the University of California. This material 
 * was produced under U.S. Government contract W-7405-ENG-36 for Los Alamos 
 * National Laboratory, which is operated by the University of California for 
 * the U.S. Department of Energy. The Government is granted for itself and 
 * others acting on its behalf a paid-up, nonexclusive, irrevocable worldwide 
 * license in this material to reproduce, prepare derivative works, and 
 * perform publicly and display publicly. Beginning five (5) years after 
 * October 10,2002 subject to additional five-year worldwide renewals, the 
 * Government is granted for itself and others acting on its behalf a paid-up, 
 * nonexclusive, irrevocable worldwide license in this material to reproduce, 
 * prepare derivative works, distribute copies to the public, perform publicly 
 * and display publicly, and to permit others to do so. NEITHER THE UNITED 
 * STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE UNIVERSITY OF 
 * CALIFORNIA, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS OR 
 * IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, 
 * COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR 
 * PROCESS DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY 
 * OWNED RIGHTS.

 * Additionally, this program is free software; you can distribute it and/or 
 * modify it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation; either version 2 of the License, 
 * or any later version.  Accordingly, this program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Lesser General Public License for more details.
 */
/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "util/Lock.h"
#include "util/DblLinkList.h"

DoubleLinkList::DoubleLinkList()
{
    Head.prev=NULL;
    Head.next=&Tail;
    Tail.prev=&Head;
    Tail.next=NULL;
    length  = 0;
    readers = 0;
    writers = 0;
    needs_cleaning = 0;
    Lock.init();
}

void DoubleLinkList::AddChunk(Links_t *TopOfChunk, int NLinks, 
        size_t SizeOfElement)
{
    Lock.lock();
    char *Tmp=(char *)TopOfChunk;
    for(int ele=0 ; ele < NLinks ; ele++ ) {
	AppendNoLock((Links_t *)Tmp);
	Tmp+=SizeOfElement;
    }
    Lock.unlock();
}

void DoubleLinkList::AddChunkNoLock(Links_t *TopOfChunk, int NLinks, 
        size_t SizeOfElement)
{
    char *Tmp=(char *)TopOfChunk;
    for(int ele=0 ; ele < NLinks ; ele++ ) {
	AppendNoLock((Links_t *)Tmp);
	Tmp+=SizeOfElement;
    }
}

void DoubleLinkList::MoveList (DoubleLinkList &NewList)
{
    Lock.lock();
    NewList.AppendList(Head.next, Tail.prev, length);
    Head.next=&Tail;
    Tail.prev=&Head;
    length=0;
    Lock.unlock();
}

void DoubleLinkList::MoveListNoLock (DoubleLinkList &NewList)
{
    NewList.AppendListNoLock(Head.next, Tail.prev, length);
    Head.next=&Tail;
    Tail.prev=&Head;
    length=0;
}

void DoubleLinkList::AppendList (volatile Links_t *FirstElement, 
        volatile Links_t *LastElement, long long LengthOfList)
{
    Lock.lock();
    volatile Links_t *lst=Tail.prev;
    lst->next=FirstElement;
    FirstElement->prev=lst;
    LastElement->next=&Tail;
    Tail.prev=LastElement;
    length+=LengthOfList;
    Lock.unlock();
}

void DoubleLinkList::AppendListNoLock(volatile Links_t *FirstElement, 
        volatile Links_t *LastElement, long long LengthOfList)
{
    volatile Links_t *lst=Tail.prev;
    lst->next=FirstElement;
    FirstElement->prev=lst;
    LastElement->next=&Tail;
    Tail.prev=LastElement;
    length+=LengthOfList;
}
