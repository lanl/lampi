/*
 * This file is part of LA-MPI
 *
 * Copyright 2002 Los Alamos National Laboratory
 *
 * This software and ancillary information (herein called "LA-MPI") is
 * made available under the terms described here.  LA-MPI has been
 * approved for release with associated LA-CC Number LA-CC-02-41.
 * 
 * Unless otherwise indicated, LA-MPI has been authored by an employee
 * or employees of the University of California, operator of the Los
 * Alamos National Laboratory under Contract No.W-7405-ENG-36 with the
 * U.S. Department of Energy.  The U.S. Government has rights to use,
 * reproduce, and distribute LA-MPI. The public may copy, distribute,
 * prepare derivative works and publicly display LA-MPI without
 * charge, provided that this Notice and any statement of authorship
 * are reproduced on all copies.  Neither the Government nor the
 * University makes any warranty, express or implied, or assumes any
 * liability or responsibility for the use of LA-MPI.
 * 
 * If LA-MPI is modified to produce derivative works, such modified
 * LA-MPI should be clearly marked, so as not to confuse it with the
 * version available from LANL.
 * 
 * LA-MPI is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * LA-MPI is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 */

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifndef _ULMSTACK
#define _ULMSTACK

#include <stdlib.h>
#include <assert.h>

#include "util/Lock.h"

/*
 * Define parameters that will be used to construct compile
 *  time parameters.
 */

typedef void *STACKEL;

template <long MaxStackElements>
struct Stack
{
    public:
// constructor
Stack()
        {
            // no data on stack
            TopOfStack=-1;
        }

    ~Stack() {};

    int push(STACKEL NewElement)
        {
            //

            int retval = -1;
            // lock stack
            Lock.lock();

            // check and make sure that there is room on the stack
            if(TopOfStack == (MaxStackElements -1 ) ){
                Lock.unlock();
                return retval;
            }

            // move pointer
            TopOfStack++;
            stackel[TopOfStack] = NewElement;
            Lock.unlock();

            retval = 0;
            return retval;
        }

    STACKEL pop()
        {
            //

            STACKEL Return;

            // lock stack
            Lock.lock();

            if( TopOfStack >= 0 ){
                Return=stackel[TopOfStack];
#ifdef _DEBUGMEMORYSTACKS
                stackel[TopOfStack]=(void *)-1L;
#endif /* _DEBUGMEMORYSTACKS */
                TopOfStack--;
            }
            else
                Return=((STACKEL)-1L);

            // unlock stack
            Lock.unlock();

            return Return;
        }

    volatile long TopOfStack;         // When this is zero, nothing there.
    STACKEL stackel[MaxStackElements];
    Locks Lock;
    enum { MaxStkElems = MaxStackElements };
    private:
};

#endif /* _ULMSTACK */
