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

#ifndef _TCP_HEADER_H_
#define _TCP_HEADER_H_

#include "internal/types.h"
#include "path/common/BaseDesc.h"
#include "path/udp/header.h"


//----------------------------------------------------------------------------
// TCP message header
//----------------------------------------------------------------------------

#define TCP_MSGTYPE_MSG 0
#define TCP_MSGTYPE_ACK 1


struct tcp_msg_header
{
    ulm_uint32_t type;		 //!< type of message
    ulm_int32_t  ctxAndMsgType;   //!< used for context of global ProcID ids
    ulm_int32_t  src_proc;	 //!< global ProcID of sending process;
    ulm_int32_t  dst_proc;	 //!< global ProcID of receiving process
    ulm_uint64_t length;	 //!< length of data in this frag frag
    ulm_uint64_t msg_length;	 //!< length of data in entire message
    ulm_ptr_t    msg_desc;       //!< point to message descriptor
    ulm_int32_t  tag_m;		 //!< tag user gave in send
    ulm_uint32_t fragIndex_m;    //!< frag index
    ulm_uint64_t isendSeq_m;	 //!< sequence number of isend (source proc)
};

#endif 
