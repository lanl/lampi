/*
 * Copyright 2002-2004. The Regents of the University of
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

#ifndef _ULM_INTERNAL_CONSTANTS_H_
#define _ULM_INTERNAL_CONSTANTS_H_

/* constants */

#define ULM_MAX_COMMAND_STRING          2048
#define ULM_MAX_CONF_FILELINE_LEN       1024
#define ULM_MAX_HOSTNAME_LEN            64
#define ULM_MAX_HOSTS                   1024
#define ULM_MAX_IFNAME_LEN              16
#define ULM_MAX_LEN_NETWORK_DEVTYPE     64
#define ULM_MAX_PATH_LEN                512
#define ULM_MAX_PREFIX                  256
#define ULM_MAX_TMP_BUFFER              20000
#define ULM_MAX_IO_BUFFER               65536
#define ULM_MAX_VERSION_STRING          256

/* Times: alarms, heartbeats, retransmission (in seconds) */

#define ALARMTIME                       300
#define MIN_CONNECT_ALARMTIME           30
#define PERPROC_CONNECT_ALARMTIME       2
#define HEARTBEATINTERVAL               (double)(1)
#define HEARTBEATTIMEOUT                (double)(10*HEARTBEATINTERVAL)
/* time to allow for orderly shutdown (abort on expiry) */
#define SHUTDOWNINTERVAL                (double)(4*HEARTBEATINTERVAL)
/* time from first client startup to timing out if startup not finished */
#define STARTUPINTERVAL                 (double)(120*HEARTBEATINTERVAL)
/* retransmit timeout */
#define RETRANS_TIME                    (double)(5.0)
/* retransmit time for control messages (no received feedback) */
#define CTL_RETRANS_TIME                (double)(5.0)
/* maximum power of two multiple of RETRANS_TIME for exponential backoff */
#define MAXRETRANS_POWEROFTWO_MULTIPLE  (int)(15)
/* minimum of RETRANS_TIME and CTL_RETRANS_TIME */
#define MIN_RETRANS_TIME                (double)(1.0)


/* network path types */
enum PathType_t {
    PATH_GM = 1,
    PATH_UDP,
    PATH_TCP,
    PATH_QUADRICS,
    PATH_IB,
    PATH_SHAREDMEM
};


/* message types */
enum MsgType_t {
    MSGTYPE_PT2PT=0,       /* standard point-to-point send */
    MSGTYPE_PT2PT_SYNC,    /* synchronous point-to-point send */
    MSGTYPE_END
};


/* control message types */
enum CtlMsgType_t {
    HEARTBEAT = 1,    /* heartbeat message */
    STDIOMSG,         /* stdio message */
    STDIOMSG_CTS,     /* stdio flow control message */
    NORMALTERM,       /* normal termination (client -> mpirun) */
    ABNORMALTERM,     /* abnormal termination (client -> mpirun) */
    TERMINATENOW,     /* mpirun -> client */
    ACKTERMINATENOW,  /* client -> mpirun */
    ALLHOSTSDONE,     /* mpirun -> client (hosts can stop processing data) */
    ACKALLHOSTSDONE   /* client -> mpirun */
};


/* queue/list types */
enum QueueType_t {

    ONNOLIST = -1,

    /* request pool */
    REQUESTFREELIST = 1,
    REQUESTINUSE,

    /* send queues */
    SENDDESCFREELIST,
    INCOMPLETEISENDQUEUE,
    UNACKEDISENDQUEUE,

    /* recv queues */
    IRECVFREELIST,
    POSTEDIRECV,
    POSTEDWILDIRECV,
    MATCHEDIRECV,

    /* frag queues */
    UNMATCHEDFRAGS,
    AHEADOFSEQUENCEFRAGS,
    FRAGSTOACK,

    /* SMP definitions */
    SMPFREELIST,
    SMPFRAGSTOSEND,
    SMPFRAGSTORECV,

    /* UDP definitions */
    UDPFRAGFREELIST,
    UDPFRAGSTOSEND,
    UDPFRAGSTOACK,
    UDPRECVFRAGSFREELIST,

    /* TCP definitions */
    TCPFRAGFREELIST,
    TCPFRAGSTOSEND,
    TCPFRAGSTOACK,
    TCPRECVFRAGSFREELIST,

    /* Quadrics definitions */
    QUADRICSFRAGFREELIST,
    QUADRICSFRAGSTOSEND,
    QUADRICSFRAGSTOACK,
    QUADRICSRECVFRAGFREELIST,
#ifdef USE_ELAN_COLL
    QUADRICS_BCAST_LIST,
#endif

    /* GM definitions */
    GMFRAGFREELIST,
    GMFRAGSTOSEND,
    GMFRAGSTOACK,
    GMRECVFRAGFREELIST,

    /* IB definitions */
    IBFRAGFREELIST,
    IBFRAGSTOSEND,
    IBFRAGSTOACK,
    IBRECVFRAGFREELIST
};


#ifdef __cplusplus

/*
 * Functions for packing and unpacking message type and context ID
 * into a 32-bit int.
 */

enum {
    MSGTYPE_BITS  = 4   /* num of bits used for message type */
};

inline unsigned int EXTRACT_MSGTYPE(unsigned int x)
{
    return x & ((1 << MSGTYPE_BITS) - 1);
}

/*
 * Macro for adding N bits from a number
 */
inline unsigned int GENERATE_CTX_AND_MSGTYPE(unsigned int ctx,
                                             unsigned int msgtype)
{
    return (ctx << MSGTYPE_BITS) + msgtype;
}

inline unsigned int EXTRACT_CTX(unsigned int ctx)
{
    return ctx >> MSGTYPE_BITS;
}

#endif

#endif                          /* !_ULM_INTERNAL_CONSTANTS_H_ */
