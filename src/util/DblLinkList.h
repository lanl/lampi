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



#ifndef _DBLLINKLIST
#define _DBLLINKLIST

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "util/Lock.h"
#include "util/Links.h"

class DoubleLinkList {

    public:
        // state
        volatile int readers;
        volatile int writers;
        volatile int needs_cleaning;
        volatile long long length;
        Locks Lock;
        Links_t Head;
        Links_t Tail;
        
        // methods
        DoubleLinkList();
        void InsertLink();
        long long size() {return length;}
        void MoveList(DoubleLinkList & NewList);
        void MoveListNoLock(DoubleLinkList & NewList);
        void AppendList(volatile Links_t *FirstElement, 
                volatile Links_t *LastElement,
                long long LengthOfList);
        void AppendListNoLock(volatile Links_t *FirstElement, 
                volatile Links_t *LastElement,
                long long LengthOfList);
        inline volatile Links_t *begin() { return (Head.next) ;}
        inline Links_t *end() { return &Tail; }
        void AddChunk(Links_t *TopOfChunk, int NLinks, size_t SizeOfElement);
        void AddChunkNoLock (Links_t *TopOfChunk, int NLinks, 
                size_t SizeOfElement);

        inline void Append(Links_t *Element)
        {
            Lock.lock();
            AppendNoLock(Element);
            Lock.unlock();
        }

        inline void AppendAsWriter(Links_t *Element)
        {
            Lock.lock();
            writers++;
            while (readers) {
                Lock.unlock();
                while (readers) ;
                Lock.lock();
            }
            AppendNoLock(Element);
            writers--;
            Lock.unlock();
        }

        inline void AppendNoLock(Links_t *Element)
        {
            volatile Links_t *LastEle=Tail.prev;
            LastEle->next=Element;
            Element->prev=LastEle;
            Element->next=&Tail;
            Tail.prev=Element;
            length++;
            return;
        }

        inline void Prepend(Links_t *Element)
        {
            Lock.lock();
            PrependNoLock(Element);
            Lock.unlock();
        }

        inline void PrependNoLock(Links_t *Element)
        {
            volatile Links_t *firstEle = Head.next;
            Element->next = firstEle;
            Element->prev = &Head;
            firstEle->prev = Element;
            Head.next = Element;
            length++;
            return;
        }

        inline volatile Links_t *GetLastElement()
        {
            volatile Links_t *ReturnValue=NULL;
            if(length == 0 )
                return ReturnValue;
            Lock.lock();
            if(length == 0 ) {
                Lock.unlock();
                return ReturnValue;
            }
            ReturnValue=Tail.prev;
            volatile Links_t *NewLastElement=ReturnValue->prev;
            NewLastElement->next=&Tail;
            Tail.prev=NewLastElement;
            length--;
            Lock.unlock();
            return ReturnValue;
        }
        
        inline volatile Links_t *GetLastElementNoLock()
        {
            volatile Links_t *ReturnValue=NULL;
            if(length == 0 ) {
                return ReturnValue;
            }
            ReturnValue=Tail.prev;
            volatile Links_t *NewLastElement=ReturnValue->prev;
            NewLastElement->next=&Tail;
            Tail.prev=NewLastElement;
            length--;
            return ReturnValue;
        }

        inline volatile Links_t *GetfirstElement()
        {
            volatile Links_t *ReturnValue=&Tail;
            if(length == 0) {
                return ReturnValue;
            }
            Lock.lock();
            if(length == 0 ){
                Lock.unlock();
                return ReturnValue;
            }
            ReturnValue=Head.next;
            volatile Links_t *NewFirstElement=ReturnValue->next;
            NewFirstElement->prev=&Head;
            Head.next=NewFirstElement;
            length--;
            Lock.unlock();
            return ReturnValue;
        }
        
        inline volatile Links_t *GetfirstElementNoLock()
        {
            volatile Links_t *ReturnValue=&Tail;
            if(length == 0 ) {
                return ReturnValue;
            }
            ReturnValue=Head.next;
            volatile Links_t *NewFirstElement=ReturnValue->next;
            NewFirstElement->prev=&Head;
            Head.next=NewFirstElement;
            length--;
            return ReturnValue;
        }

        volatile Links_t *RemoveLink(Links_t *Link) {
            volatile Links_t *ReturnValue;
            Lock.lock();
            ReturnValue = RemoveLinkNoLock(Link);
            Lock.unlock();
            return ReturnValue;
        }

        volatile Links_t *RemoveLinkNoLock(Links_t *Link) 
        {
            Links_t *ReturnValue=0;
            if(length == 0 ) {
                return ReturnValue;
            }
#ifdef _DEBUGQUEUES
            bool found=false;
            for ( Links_t *dsc=(Links_t *)begin();
                    dsc !=end(); dsc=(Links_t *) dsc->next ) {
                if( dsc->next == NULL ) {
                    // this is a tail
                    volatile Links_t *PreviousLink=dsc->prev;
                    fprintf(stderr, " changed list :: which queue %d\n", 
                            PreviousLink->WhichQueue);
                    fflush(stderr);
                }
                if( dsc == Link){
                    found=true;
                    break;
                }
            }
            if(!found){
                fprintf(stderr, " link not found in list\n");
                fflush(stderr);
                abort();
            }
#endif /* _DEBUGQUEUES */

            // sanity checks;
            volatile Links_t *Prv=Link->prev;
            assert(Prv!=(volatile Links_t *)0);
            volatile Links_t *Nxt=Link->next;
            assert(Nxt!=(volatile Links_t *)0);

            Prv->next=Nxt;
            Nxt->prev=Prv;
            length--;
#ifdef _DEBUGQUEUES
            if( length == 0 ) {
                volatile Links_t *First=Head.next;
                if( First != &Tail ) {
                    fprintf(stderr, " Error:: First element of empty list "
                            " does not point to tail. Link %p\n", Link);
                    fflush(stderr);
                    abort();
                }
                volatile Links_t *Last=Tail.prev;
                if( Last != &Head ) {
                    fprintf(stderr, " Error:: Last element of empty list does "
                            "not point to Head\n");
                    fflush(stderr);
                    abort();
                }
            }
#endif /* _DEBUGQUEUES */

            return Prv;
        }
};

typedef DoubleLinkList SharedMemDblLinkList;
typedef DoubleLinkList ProcessPrivateMemDblLinkList;

#endif /* _DBLLINKLIST */
