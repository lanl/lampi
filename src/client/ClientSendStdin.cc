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



#include <stdio.h>
#include <unistd.h>
#include <sys/uio.h>

#include "internal/constants.h"
#include "internal/log.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "client/ULMClient.h"
#include "client/SocketGeneric.h"



int ClientSendStdin(int* src, int* dst)
{
    int size;
    int error;
    int IOReturn = _ulm_Recv_Socket(*src, &size, sizeof(size), &error);
    /* socket connection closed */
    if (IOReturn == 0) {
        close(*src);
        *src = -1;
        return -1;
    }

    if (IOReturn < 0 || error != ULM_SUCCESS) {
        ulm_exit((-1, "Error: reading STDIOMSG.  RetVal = %ld, error = %d\n", IOReturn, error));
    }

    char *buff = new char[size];
    if(buff == 0) {
        ulm_exit((-1, "ClientSendStdin: unable to allocate buffer for STDIOMSG\n"));
    }

    IOReturn = _ulm_Recv_Socket(*src, buff, size, &error);
    /* socket connection closed */
    if (IOReturn == 0 || size == 0) {
        close(*src);
        close(*dst);
        *src = *dst = -1;
        delete[] buff;
        return -1;
    }

    if (IOReturn < 0 || error != ULM_SUCCESS) {
        ulm_exit((-1, "ClientSendStdin: error reading STDIOMSG, error = %d\n", error));
    }

    IOReturn = write(*dst, buff, size);
    if(IOReturn < 0) {
        close(*src);
        close(*dst);
        *src = *dst = -1;
    }
    delete[] buff;
    return 0;
}

