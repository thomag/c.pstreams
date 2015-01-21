/*===========================================================================
FILE: udpdev.c

    streams device module for UDP

 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.

===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include "options.h"
#include "env.h"
#include "assert.h"
#include "listop.h"
#include "pstreams.h"
#include "udpdev.h"
#include "util.h"

P_QINIT udpdev_wrinit={0};
P_QINIT udpdev_rdinit={0};
P_STREAMTAB udpdev_streamtab={0};
P_MODINFO udpdev_wrmodinfo={0};
P_MODINFO udpdev_rdmodinfo={0};

/*DEBUG mode - overriding options.h settings*/
#define PSTREAMS_LT

/******************************************************************************
Name: udpdev_init
Purpose: initialise this module. constructor for this module.
Parameters:
Caveats:
******************************************************************************/
int
udpdev_init()
{
    /*first initialize udpdevmodinfo*/
    udpdev_wrmodinfo.mi_idnum = 1;
    udpdev_wrmodinfo.mi_idname = "UDPDEV WR";
    udpdev_wrmodinfo.mi_minpsz = 0;
    udpdev_wrmodinfo.mi_maxpsz = 100;
    udpdev_wrmodinfo.mi_hiwat = 1024;
    udpdev_wrmodinfo.mi_lowat = 256;

    udpdev_rdmodinfo.mi_idnum = 1;
    udpdev_rdmodinfo.mi_idname = "UDPDEV_RD";
    udpdev_rdmodinfo.mi_minpsz = 0;
    udpdev_rdmodinfo.mi_maxpsz = 100;
    udpdev_rdmodinfo.mi_hiwat = 1024;
    udpdev_rdmodinfo.mi_lowat = 256;

    /*init udpdev_streamtab*/
#ifdef M2STRICTTYPES
    udpdev_wrinit.qi_qopen = udpdev_open;
    udpdev_wrinit.qi_putp = udpdev_wput;
    udpdev_wrinit.qi_srvp = NULL;
    udpdev_rdinit.qi_qopen = udpdev_open;
    udpdev_rdinit.qi_putp = udpdev_rput;
    udpdev_rdinit.qi_srvp = udpdev_rsrvp;
    udpdev_rdinit.qi_qclose = udpdev_close;
#else
    udpdev_wrinit.qi_qopen = (int (*)())udpdev_open;
    udpdev_wrinit.qi_putp = (int (*)())udpdev_wput;
    udpdev_wrinit.qi_srvp = NULL;
    udpdev_rdinit.qi_qopen = (int (*)())udpdev_open;
    udpdev_rdinit.qi_putp = (int (*)())udpdev_rput;
    udpdev_rdinit.qi_srvp = (int (*)())udpdev_rsrvp;
    udpdev_rdinit.qi_qclose = (int (*)())udpdev_close;
#endif

    udpdev_wrinit.qi_minfo = &udpdev_wrmodinfo;
    udpdev_rdinit.qi_minfo = &udpdev_rdmodinfo;

    udpdev_streamtab.st_wrinit = &udpdev_wrinit;
    udpdev_streamtab.st_rdinit = &udpdev_rdinit;

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: udpdev_open
Purpose: open procedure. create socket.
Parameters:
Caveats:
******************************************************************************/
int
udpdev_open(P_QUEUE *q)
{
    UDPDEVAREA *area=NULL;

    /*Default log trace filter value*/
    q->ltfilter = UDPDEV_LTLEVEL; /*default Log/Trace level*/

    /*udpdevarea is shared with the peer queue*/
    if(q->q_peer && q->q_peer->q_ptr)
    {
        q->q_ptr = q->q_peer->q_ptr;
    }
    else
    {
        area = udpdev_getarea(NULL);

        ASSERT(area); /*TODO - handle*/

#ifdef PSTREAMS_WIN32
        /*init winsock*/
        if(PDEV_INIT(MAKEWORD(2,2), &area->wsadata) != 0)
        {
            PSTRMHEAD(q)->perrno = WSAGetLastError();

            /*ERROR*/
#ifdef PSTREAMS_LT
            pstreams_log(q, PSTREAMS_LTERROR, "udpdev_open: PDEV_INIT() failed. error %d",
                PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
            return P_STREAMS_FAILURE;
        }
#endif

        area->sock = socket(AF_INET, SOCK_DGRAM, 0);

        if(area->sock == INVALID_SOCKET)
        {
            PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfGetSocketError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                                WSAGetLastError();
#else
                                errno;
#endif
#endif
#ifdef PSTREAMS_LT
            pstreams_log(q, PSTREAMS_LTERROR, "udpdev_open: socket() failed. error %d",
                PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
            return P_STREAMS_FAILURE;
        }

        {
            const char trueval=1;
            
            setsockopt(area->sock, SOL_SOCKET, SO_REUSEADDR, &trueval, sizeof(trueval));
        }

        /*sock is in blocking mode*/

        area->laddr.sin_family = area->raddr.sin_family = AF_INET;

        q->q_ptr = area;
    }

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: udpdev_wput
Purpose: put procedure for downstream traffic. process data or control messages
Parameters:
Caveats: Callees should clear msg, on both success and failure
******************************************************************************/
int
udpdev_wput(P_QUEUE *q, P_MSGB *msg)
{
    int status=P_STREAMS_FAILURE;

    ASSERT(msg && msg->b_datap);

    switch(msg->b_datap->db_type)
    {
    case P_M_DATA:
        status = udpdev_wput_data(q, msg);
        break;
    case P_M_PROTO:
    case P_M_CTL:
        status = udpdev_wput_ctl(q, msg);
        break;
    default:
        ;/*TODO*/
    }


/* AT this point msg is not valid - it may have been free'd by a callee
    if(status != P_STREAMS_SUCCESS)
    {
        pstreams_freemsg(PSTRMHEAD(q), msg);
    }
*/

    return status;
}

/******************************************************************************
Name: udpdev_wput_data
Purpose: 
Parameters:
Caveats: msg is freed/consumed on any successful call to this function. Caller
            releases msg if unsuccessful
******************************************************************************/
int
udpdev_wput_data(P_QUEUE *q, P_MSGB *msg)
{
    UDPDEVAREA *area=NULL;
    int32 msgsize=0;
    int sockstatus=0;

    ASSERT(msg);

    area = (UDPDEVAREA *)q->q_ptr;

    msgsize = pstreams_msgsize(msg);

    if(pstreams_countmsgcont(msg) > 1)
    {
        P_MSGB *pullupmsg = pstreams_msgpullup(PSTRMHEAD(q), msg, msgsize);

        if(!pullupmsg)
        {
        /*cannot send message out now*/
#ifdef PSTREAMS_LT
            pstreams_log(q, (P_LTCODE)PSTREAMS_LTWARNING, "wput_data: pullupmsg failed for %ld bytes. Will retry",
                msgsize);
#endif
            pstreams_putq(q, msg);
            return P_STREAMS_SUCCESS;
        }

        ASSERT(msgsize == pstreams_msg1size(pullupmsg));

        pstreams_freemsg(PSTRMHEAD(q), msg);
        msg = pullupmsg;
    }

#ifdef PSTREAMS_UDPDUMP
    {
        uchar hexbuf[1792*2]={0};

        bintohex(hexbuf, msg->b_rptr, msgsize);

        pstreams_log(q, PSTREAMS_LTINFO, "udpdev_wput_data: sending %ld bytes\n%s",
            msgsize, hexbuf);
    }
#endif /*PSTREAMS_LT*/

    sockstatus = sendto(area->sock, (char *)msg->b_rptr, msgsize, 0,
                        (struct sockaddr *)&area->raddr, sizeof(area->raddr));

    if(sockstatus == SOCKET_ERROR)
     {
            PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfGetSocketError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                                WSAGetLastError();
#else
                                errno;
#endif
#endif
#ifdef PSTREAMS_LT
        pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_data: send failed. error %d", 
            PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
       /*
        * NOTE: current policy is to assume something is wrong with this message
        * so drop it; do not retry by putting it back in queue - thomas
        */
        pstreams_freemsg(PSTRMHEAD(q), msg);
        return P_STREAMS_FAILURE; 
    }

#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTINFO, "udpdev_wput_data: bytes written=%d", sockstatus);
#endif /*PSTREAMS_LT*/

    /*sockstatus holds bytes sent by send()*/
    ASSERT(sockstatus <= msgsize);

    pstreams_msgconsume(msg, sockstatus);/*consume number of bytes sent*/

    if ( pstreams_msgsize(msg) == 0 )
    {
        /*done sending msg*/
        pstreams_freemsg(PSTRMHEAD(q), msg);
    }
    else
    {
        /*send later*/
        pstreams_putq(q, msg);
    }

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: udpdev_wput_ctl
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
udpdev_wput_ctl(P_QUEUE *q, P_MSGB *msg)
{
    UDPDEVAREA *area = (UDPDEVAREA *)q->q_ptr;
    int sockstatus=0;

    switch(msg->b_datap->db_type)
    {
    case P_M_CTL:
        {
            MY_CTL ctl={0};

            ASSERT(pstreams_msgsize(msg) >= sizeof(MY_CTL));

            memcpy(&ctl, msg->b_rptr, sizeof(ctl)); /*memcpy instead of just a cast for safe alignment*/
            pstreams_msgconsume(msg, sizeof(MY_CTL));

            switch(ctl.ctlfunc)
            {
            case UDPDEV_RADDR:
                ASSERT(pstreams_msgsize(msg) >= sizeof(MY_REMOTEADDR));
                ASSERT(sizeof(struct sockaddr_in) == sizeof(MY_REMOTEADDR));

                memcpy(&area->raddr, msg->b_rptr, sizeof(struct sockaddr_in));
                break;

            case UDPDEV_SHAREFADDR:
                ASSERT(sizeof(struct sockaddr_in) == sizeof(MY_REMOTEADDR));
                /*
                 * in a break from the rest of pstreams philosophy - share addresses!
                 * BEWARE - not thread-safe
                 */
                {
                    MY_REMOTEADDR **addrloc = NULL;

                    /*memcpy() - instead of assigning - to avoid alignment issues*/
                    memcpy(&addrloc, msg->b_rptr, sizeof(addrloc));

                    *addrloc = &area->faddr;
                }
                
                break;

            default:
                ASSERT(0); /*unsupported commands not expected*/
            }
        }
        break;

    case P_M_PROTO:
        {
            MY_PROTO proto={0};

            ASSERT(pstreams_msgsize(msg) >= sizeof(MY_PROTO));

            memcpy(&proto, msg->b_rptr, sizeof(MY_PROTO));
            pstreams_msgconsume(msg, sizeof(MY_PROTO));

            switch(proto.ctlfunc)
            {
            case UDPDEV_RADDR:
                if(pstreams_msgsize(msg)  < sizeof(struct sockaddr_in))
                {
#ifdef PSTREAMS_LT
                    pstreams_log(q, PSTREAMS_LTERROR, "wput_ctl: ctl msg has "
                        "invalid payload for UDPDEV_RADDR command");
#endif /*PSTREAMS_LT*/
                    break;
                }

                memcpy(&area->raddr, msg->b_rptr, sizeof(struct sockaddr_in));
#if(0)
                /*first disassociate from existing associations*/
                {
                    struct sockaddr_in nulladdr={0};

                    nulladdr.sin_family = AF_INET;
                    nulladdr.sin_port = p_htons(5150);
                    nulladdr.sin_addr.s_addr = p_htonl(INADDR_ANY);
                    sockstatus = 
                        connect(area->sock, (SOCKADDR *)&nulladdr, sizeof(nulladdr));
                }
                if(sockstatus == SOCKET_ERROR)
                {
                    PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                        tfGetSocketError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                        WSAGetLastError();
#else
                        errno;
#endif
#endif
#ifdef PSTREAMS_LT
                    pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: connect failed."
                            " error %d", PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
                    break;
                }
#endif
#if(0)
                /*associate with given address*/
                sockstatus = 
                    connect(area->sock, (struct sockaddr *)&area->raddr, 
                        sizeof(struct sockaddr_in));
                if(sockstatus == SOCKET_ERROR)
                {
                    PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                        tfGetSocketError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                        WSAGetLastError();
#else
                        errno;
#endif
#endif
#ifdef PSTREAMS_LT
                    pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: connect failed."
                        " retval %d. lasterror %d",
                        sockstatus, PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
                    break;
                }
#endif
                break;

            case UDPDEV_LADDR:
                if(pstreams_msgsize(msg) < sizeof(struct sockaddr_in))
                {
#ifdef PSTREAMS_LT
                    pstreams_log(q, PSTREAMS_LTERROR, "wput_ctl: ctl msg has "
                        "invalid payload for UDPDEV_LADDR command");
#endif /*PSTREAMS_LT*/
                    break;
                }
                memcpy(&area->laddr, msg->b_rptr, sizeof(struct sockaddr_in));

                /*associate with given address*/
                sockstatus = 
                    bind(area->sock, (struct sockaddr *)&area->laddr, 
                        sizeof(struct sockaddr_in));
                if(sockstatus == SOCKET_ERROR)
                {
                    PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfGetSocketError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                                WSAGetLastError();
#else
                                errno;
#endif
#endif
#ifdef PSTREAMS_LT
                    pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: bind failed."
                                       " retval %d. lasterror %d",
                                    sockstatus, PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
                    break;
                }
                break;

            default:
#ifdef PSTREAMS_LT
                pstreams_log(q, PSTREAMS_LTWARNING, "udpdev_wput_ctl: unknown command %d",
                    proto.ctlfunc);
#endif /*PSTREAMS_LT*/
                break; /*unsupported commands ignored*/
            }
        }
        break;

    default:
        ASSERT(0);
    }
    
    pstreams_freemsg(PSTRMHEAD(q), msg);

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: udpdev_rsrvp
Purpose: service procedure for upstream traffic
Parameters:
Caveats:
******************************************************************************/
int
udpdev_rsrvp(P_QUEUE *q)
{
    UDPDEVAREA *area=NULL;
    P_MSGB *msg=NULL;
    fd_set sockfds={0};
    int32 activesockets=0;
    int32 len = 0;
    struct timeval timeout={0};

    area = (UDPDEVAREA *)q->q_ptr;
    FD_ZERO(&sockfds);
    FD_SET(area->sock, &sockfds);

    activesockets =
        select(area->sock+1, &sockfds, NULL, NULL, &timeout);/*last argument => non-blocking*/

    if(activesockets == SOCKET_ERROR)
    {
            PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfGetSocketError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                                WSAGetLastError();
#else
                                errno;
#endif
#endif
#ifdef PSTREAMS_LT
        pstreams_log(q, PSTREAMS_LTERROR, "udpdev_rsrvp: select failed."
                    " retval %d. lasterror %d",
                    activesockets, PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
                return P_STREAMS_FAILURE;
    }

    while ( activesockets > 0 )
    {
        int faddrlen=sizeof(area->faddr);/*length of data returned in faddr*/

        if(!FD_ISSET(area->sock, &sockfds))
        {
            continue;
        }

        activesockets--; /*this socket is set*/

        /*
         * Assuming only a read event - though other events are possible
         * if socket is non-blocking
         */

        /*len = recvfrom(area->sock, buf, 0, MSG_PEEK, (struct sockaddr *)&faddr, &faddrlen);*/

        {
            /*char __gc *p = new char[len];*/
            msg = pstreams_allocb((P_STREAMHEAD *)q->strmhead, 1792, 0);
            if(!msg)
            {
#ifdef PSTREAMS_LT
                pstreams_log(q, PSTREAMS_LTWARNING, "rsrvp: Unable to allocate read buffer. Not reading");
#endif
                return P_STREAMS_SUCCESS;
            }

            len = recvfrom(area->sock, (char *)msg->b_wptr, 1792, 0, (struct sockaddr *)&area->faddr, &faddrlen);

            if ( len != SOCKET_ERROR )
            {
#ifdef PSTREAMS_UDPDUMP
                /*the block below is space expensive! - TODO verify if needed*/
                {
                    uchar hexbuf[1792*2] = { 0 };

                    bintohex(hexbuf, (uchar *)hexbuf, len);

                    pstreams_log(q, PSTREAMS_LTINFO, "udpdev_rsrvp: rx %d bytes\n%s", len, hexbuf);
                }
#endif 
                if ( len > 1792 )
                {
                    pstreams_freemsg(PSTRMHEAD(q), msg);
#ifdef PSTREAMS_LT
                    pstreams_log(q, PSTREAMS_LTWARNING, 
                        "rsrvp: UDP datagram too large. dropped %d bytes.", len);
#endif /*PSTREAMS_LT*/
                    return P_STREAMS_SUCCESS;
                }

#ifdef PSTREAMS_LT
                pstreams_log(q, PSTREAMS_LTINFO, "udpdev_wput_data: bytes read=%ld", len);
#endif /*PSTREAMS_LT*/

                msg->b_wptr += len;

                if ( len < pstreams_mpool(1792)) /*will a smaller buffer do?*/
                {
                    P_MSGB *msgcpy = pstreams_copymsg(PSTRMHEAD(q), msg); /*will try smallest buffer*/
                    if(msgcpy) /*...and did we get a smaller buffer?*/
                    {
                        pstreams_freemsg(PSTRMHEAD(q), msg);
                        msg = msgcpy;
                    }
#ifdef PSTREAMS_LT
                    else
                    {
                        pstreams_log(q, PSTREAMS_LTINFO+1, "udpdev_wput_data: "
                            "failed in downsizing readbuffer from 1792 to %ld", len);
                    }
#endif
                }
#ifdef PSTREAMS_LT
                else
                {
                    pstreams_log(q, PSTREAMS_LTINFO, "udpdev_wput_data: "
                        "read non-downsizable message of length %ld", len);
                }
#endif
    
                pstreams_putnext(q, msg);
                msg = NULL;
            }
            else
            {
                pstreams_freemsg(PSTRMHEAD(q), msg);

                PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfGetSocketError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                                WSAGetLastError();
#else
                                errno;
#endif
#endif
#ifdef PSTREAMS_LT
                pstreams_log(q, PSTREAMS_LTERROR, "udpdev_rsrvp: recvfrom failed." " error %d", 
                    PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/

				/* ignore socket error - 
				 * this is because we'd rather rely on ACKs from NMC
				 * rather than on socket errors which depend on socket code
				 * - thomas - 09/19/2002.
				 */
				PSTRMHEAD(q)->perrno = 0;

                return P_STREAMS_SUCCESS;
            }
        }
    }

    return P_STREAMS_SUCCESS;
}
    
/******************************************************************************
Name: udpdev_rput
Purpose: put procedure for upstream traffic.
Parameters:
Caveats:
******************************************************************************/
int
udpdev_rput(P_QUEUE *q, P_MSGB *msg)
{
    /*can never be called*/
    ASSERT(0);

    PDBG(q=NULL); /*keep compiler happy*/
    PDBG(msg=NULL); /*keep compiler happy*/

    return 0;
}


/******************************************************************************
Name: udpdev_close
Purpose: destructor for this instance of this module
Parameters:
Caveats:
******************************************************************************/    
int
udpdev_close(P_QUEUE *q)
{
    if(!q)
    {
        return P_STREAMS_SUCCESS;
    }

    if(q->q_ptr)
    {
        UDPDEVAREA *area=(UDPDEVAREA *)q->q_ptr;

        /*TODO - orderly release*/
#ifdef PSTREAMS_WIN32
        closesocket(area->sock);
#else
#ifdef PSTREAMS_H8
        tfClose(area->sock);
#else
        /*TODO - general case*/
#endif
#endif
        memset(q->q_ptr, 0, sizeof(UDPDEVAREA));

        q->q_ptr = NULL;

        /*
         *since area is shared with peer, peer's q_ptr is
         *is no longer valid
         */
        if(q->q_peer && q->q_peer->q_ptr)
        {
            q->q_peer->q_ptr = NULL;
        }
    }

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: udpdev_getarea
Purpose: 
Parameters:
Caveats:
******************************************************************************/
static UDPDEVAREA *
udpdev_getarea(void *key)
{
    static UDPDEVAREA staticmem;
    UDPDEVAREA *udpdevarea= (UDPDEVAREA *)&staticmem;

    PDBG(key=NULL); /*keep compiler happy*/

    if(udpdevarea)
    {
        memset(udpdevarea, 0, sizeof(UDPDEVAREA));
    }

    return udpdevarea;
}
