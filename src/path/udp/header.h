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



#ifndef _UDP_HEADER_H_
#define _UDP_HEADER_H_

#include "internal/types.h"

#define PTR_PADDING 4

#ifdef __linux__
#include <endian.h>
# if BYTE_ORDER == LITTLE_ENDIAN
#   include <byteswap.h>

#   ifndef bswap_64
inline ulm_uint64_t bswap_64(ulm_uint64_t x)
{
    union { ulm_uint64_t ll;
        ulm_uint32_t l[2]; } w, r;
    w.ll = (x);
    r.l[0] = ntohl(w.l[1]);
    r.l[1] = ntohl(w.l[0]);
    return r.ll;
}
#   endif

#ifdef __GNUC__
#define ulm_bswap_32(x)	bswap_32(x)
#else
inline ulm_uint32_t ulm_bswap_32(ulm_uint32_t x)
{
    return ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |
            (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24));    
}
#endif	/*__GNUC__ */

#   define ulm_ntohl(x)	bswap_64 (x)
#   define ulm_ntohi(x)	ulm_bswap_32 (x)
#   define ulm_htonl(x)	bswap_64 (x)
#   define ulm_htoni(x)	ulm_bswap_32 (x)
# endif
#endif

#ifdef THIS_WAS_ALPHA___SHOULD_IT_BE_TRU64
inline ulm_uint64_t __ulm_ntohl(ulm_uint64_t x)
{
    union { unsigned ulm_int64 ll;
        unsigned ulm_int_32 l[2]; } w, r;
    w.ll = (x);
    r.l[0] = ntohl(w.l[1]);
    r.l[1] = ntohl(w.l[0]);
    return r.ll;
}
# define ulm_ntohl(x)   __ulm_ntohl(x)
# define ulm_ntohi(x)	      ntohl(x)
# define ulm_htonl(x)	__ulm_ntohl(x)
# define ulm_htoni(x)	      ntohl(x)
#endif

#ifndef ulm_ntohl
# define ulm_ntohl(x)	(x)
# define ulm_ntohi(x)	(x)
# define ulm_htonl(x)	(x)
# define ulm_htoni(x)	(x)
#endif

inline ulm_ptr_t ulm_htonp(ulm_ptr_t ptr)
{
    return ptr;
}

inline ulm_ptr_t ulm_ntohp(ulm_ptr_t ptr)
{
    return ptr;
}

//!----------------------------------------------------------------------------
//! UDP message header
//!----------------------------------------------------------------------------
struct udp_message_header_t
{
    ulm_uint32_t type;		 //!< type of message
    ulm_int32_t ctxAndMsgType;   //!< used for context of global ProcID ids
    ulm_int32_t src_proc;	 //!< global ProcID of sending process;
    ulm_int32_t dest_proc;	 //!< global ProcID of receiving process
    ulm_ptr_t udpio;		 //!< pointer to send frag desc

    ulm_uint64_t length;	 //!< length of data in this frag frag
    ulm_uint64_t messageLength;	 //!< length of data in entire message

    ulm_int32_t tag_m;		 //!< tag user gave in send
    ulm_uint32_t fragIndex_m;    //!< frag index
    ulm_uint64_t isendSeq_m;	 //!< sequence number of isend (source proc)
    ulm_uint64_t frag_seq;	 //!< frag sequence number
    ulm_uint32_t refCnt;
};


#define UDP_ACK_GOODACK ( int )(1)
#define UDP_ACK_NACK ( int )(2)

//!----------------------------------------------------------------------------
//! UDP ACK payload header
//!----------------------------------------------------------------------------
struct udp_ack_header_t
{
    //
    // The first 5 items are identical with udp_message_header (except that
    // dest_proc and src_proc have been reversed).  This allows copies
    // to be made from a message header to an ack header.
    //

    ulm_uint32_t type;		 //!< type of message
    ulm_int32_t ctxAndMsgType;	 //!< used for context of global ProcID ids
    ulm_int32_t dest_proc;	 //!< global ProcID of original sending process
    ulm_int32_t src_proc;	 //!< global ProcID of the acknowledging process;
    ulm_ptr_t udpio;		 //!< pointer to original send frag desc
    ulm_uint64_t single_mseq;	 //!< message sequence number of single frag
    ulm_uint64_t single_fseq;	 //!< frag sequence number within message
    ulm_uint64_t single_fragseq; //!< frag sequence number

    // last consecutive in-order frag received but not necessarily
    // delivered to the src_proc - this sequence indicates frags that do not
    // need to be retransmitted CONDITIONALLY! Therefore this number may
    // decrease, if data is received in error.
    ulm_uint64_t received_fragseq; //!< largest in-order rec'd frag sequence

    // last consecutive in-order frag delivered to the src_proc (i.e., copied
    // out of ULM's library buffers successfully) - this sequence indicates
    // data that has been delivered; all send resources may be freed for frags
    // <= delivered_fragseq.
    ulm_uint64_t delivered_fragseq; //!< largest in-order delivered frag seq

    ulm_int32_t ack_or_nack;	 //!< GOODACK/NACK of single message frag
};

typedef struct udp_message_header_t udp_message_header;
typedef struct udp_ack_header_t udp_ack_header;
typedef union { udp_message_header msg; udp_ack_header ack; } udp_header;


#endif  // _UDP_HEADER_H_
