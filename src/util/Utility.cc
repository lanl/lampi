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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>


#include "internal/constants.h"
#include "internal/malloc.h"
#include "internal/profiler.h"
#include "internal/types.h"
#include "util/Utility.h"
#include "client/SocketGeneric.h"

int _ulm_ParseString(char ***ArgList, const char* InputBuffer, const int NSeparators,
		     const char *SeparatorList)
{
    /*
      This function will parse the input string in InputBuffer and
      return an array of substrings that are separated by the characters
      identified by SeparatorList.
      PRE:	SeparatorList is an array of length NSeparators where each
      character is a delimiter for InputBuffer.
      ArgList is a reference parameter for an array of strings.
      POST:	function will create an array of substrings that are
      delimited by the chars in SeparatorList.  Caller is responsible
      for freeing the memory.
    */

    /*
      Example: InputBuffer = "foo:bar/moo;cow", SeparatorList = ":;/'", NSeparators = 4
      The function returns the array {"foo", "bar", "moo", cow"} in ArgList.
    */

    int i, j, cnt, InString;
    size_t len;
    char *StrtString, *EndString;
    int ThisIsSeprator;
    char Char;

    cnt=0;
    InString=0;
    StrtString=(char *)InputBuffer;
    EndString=StrtString+strlen(InputBuffer);

    for(i=0 ; i < (long)strlen(InputBuffer) ; i++ ) {
	Char=*(InputBuffer+i);
	ThisIsSeprator=0;
	for( j=0 ; j < NSeparators ; j++ ){
	    if(SeparatorList[j] == Char){
		ThisIsSeprator=1;
		break;
	    }
	}
	if(ThisIsSeprator){   /* process character */
	    if(InString){
		EndString=(char *) InputBuffer+i;
		cnt++;
		InString=0;
	    }
	} else {
	    if(!InString){
		InString=1;
		StrtString=(char *) InputBuffer+i;
		if(i==(long)(strlen(InputBuffer)-1)){  /* end of string code */
		    EndString=(char *) InputBuffer+i;
		    cnt++;
		}
	    } else {  /* end of string check */
		if(i==(long)(strlen(InputBuffer)-1)) {
		    EndString=(char *) InputBuffer+i;
		    cnt++;
		}
	    }
	}  /* end processing character */
    }

    // return if cnt == 0
    if( cnt == 0 )
	return cnt;

    *ArgList=(char **)ulm_malloc(sizeof(char *)*cnt);
    if( (*ArgList) == NULL ) {
	printf("Error: can't allocate memory in parser. Len %ld\n",
	       (long)(sizeof(char *)*cnt));
	perror(" Memory allocation failure ");
	exit(-1);
    }
    cnt=0;
    InString=0;
    StrtString=(char *) InputBuffer;
    EndString=StrtString+strlen(InputBuffer);

    for(i=0 ; i < (long)strlen(InputBuffer) ; i++ ) {
	Char=*(InputBuffer+i);
	ThisIsSeprator=0;
	for( j=0 ; j < NSeparators ; j++ ){
	    if(SeparatorList[j] == Char){
		ThisIsSeprator=1;
		break;
	    }
	}
	if(ThisIsSeprator){   /* process character */
	    if(InString){
		EndString=(char *) InputBuffer+i;
		len=(EndString-StrtString)/sizeof(char);
		(*ArgList)[cnt]=(char *) ulm_malloc(sizeof(char)*(len+1));
                memset((*ArgList)[cnt], 0, sizeof(char)*(len+1));
		strncpy((*ArgList)[cnt++], StrtString, len);
		InString=0;
	    }
	} else {
	    if(!InString){
		InString=1;
		StrtString=(char *) InputBuffer+i;
		if(i==(long)(strlen(InputBuffer)-1)){  /* end of string code */
		    EndString=(char *) InputBuffer+i;
		    len=(EndString-StrtString)/sizeof(char)+1;
		    (*ArgList)[cnt]=(char *) ulm_malloc(sizeof(char)*(len+1));
                    memset((*ArgList)[cnt], 0, sizeof(char)*(len+1));
		    strncpy((*ArgList)[cnt++], StrtString, len);
		}
	    } else {  /* end of string check */
		if(i==(long)(strlen(InputBuffer)-1)) {
		    EndString=(char *) InputBuffer+i;
		    len=(EndString-StrtString)/sizeof(char)+1;
		    (*ArgList)[cnt]=(char *) ulm_malloc(sizeof(char)*(len+1));
                    memset((*ArgList)[cnt], 0, sizeof(char)*(len+1));
		    strncpy((*ArgList)[cnt++], StrtString, len);
		}
	    }
	}  /* end processing character */
    }

    return cnt;
}


int _ulm_NStringArgs(char* InputBuffer, int NSeparators, char *SeparatorList)
{
    int i, j, cnt;
    int InString;
    int ThisIsSeprator;
    char Char;

    cnt=0;
    InString=0;

    for(i=0 ; i < (long)strlen(InputBuffer) ; i++ ) {
	Char=*(InputBuffer+i);
	ThisIsSeprator=0;
	for( j=0 ; j < NSeparators ; j++ ){
	    if(SeparatorList[j] == Char){
		ThisIsSeprator=1;
		break;
	    }
	}
	if(ThisIsSeprator){   /* process character */
	    if(InString){
		cnt++;
		InString=0;
	    }
	} else {
	    if(!InString){
		InString=1;
		if(i==(long)(strlen(InputBuffer)-1)){  /* end of string code */
		    cnt++;
		}
	    } else {  /* end of string check */
		if(i==(long)(strlen(InputBuffer)-1)) {
		    cnt++;
		}
	    }
	}  /* end processing character */
    }

    return cnt;
}

void _ulm_FreeStringMem(char ***ArgList, int NArgs)
{
    int i;
    for(i=0 ; i < NArgs ; i++)
	ulm_free( (*ArgList)[i]);
    ulm_free( (*ArgList) );
}

/*
 * This routine is used to send a heartbeat message.  All that is sent
 *  is a tag.  To avoid clock synchronization broblems, the receiving
 *  end supplies the time stamp.
 */

int _ulm_SendHeartBeat(int *ClientSocketFDList, int NHosts, int MaxDescriptor)
{
    unsigned int tag;
    long RetVal;
    ssize_t IOReturn;
    int i;
    struct timeval WaitTime;
    fd_set WriteSet;
    ulm_iovec_t IOVec[2];

    tag = HEARTBEAT;
    IOVec[0].iov_base=&tag;
    IOVec[0].iov_len=(ssize_t)(sizeof(unsigned int));

    WaitTime.tv_sec=0;
    WaitTime.tv_usec=0;

    FD_ZERO(&WriteSet);
    for (i=0 ; i < NHosts ; i++ )
	if(ClientSocketFDList[i]>= 0 )FD_SET(ClientSocketFDList[i], &WriteSet);

    RetVal=select(MaxDescriptor,NULL, &WriteSet,NULL, &WaitTime);
    if(RetVal < 0 )
	return (int) RetVal;

    /* send heart beat time stamp to each host */
    if( RetVal > 0 ) {
	for (i=0 ; i < NHosts ; i++ ) {
	    if( (ClientSocketFDList[i] > 0 ) && (FD_ISSET(ClientSocketFDList[i], &WriteSet)) ) {
		IOReturn=_ulm_Send_Socket(ClientSocketFDList[i], 1, IOVec);
		if( IOReturn < 0 )
		    return (int) IOReturn;
	    }
	}
    }
    return 0;
}
