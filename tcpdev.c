/*===========================================================================
FILE: tcpdev.c

    streams device module for TCP

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
#include "tcpdev.h"
#include "util.h"

P_QINIT tcpdev_wrinit={0};
P_QINIT tcpdev_rdinit={0};
P_STREAMTAB tcpdev_streamtab={0};
P_MODINFO tcpdev_wrmodinfo={0};
P_MODINFO tcpdev_rdmodinfo={0};

/******************************************************************************
Name: tcpdev_init
Purpose: initialise this module. constructor for this module.
Parameters:
Caveats:
******************************************************************************/
int
tcpdev_init()
{
    /*first initialize tcpdevmodinfo*/
    tcpdev_wrmodinfo.mi_idnum = 1;
    tcpdev_wrmodinfo.mi_idname = "TCPDEV WR";
    tcpdev_wrmodinfo.mi_minpsz = 0;
    tcpdev_wrmodinfo.mi_maxpsz = 100;
    tcpdev_wrmodinfo.mi_hiwat = 1024;
    tcpdev_wrmodinfo.mi_lowat = 256;

    tcpdev_rdmodinfo.mi_idnum = 1;
    tcpdev_rdmodinfo.mi_idname = "TCPDEV_RD";
    tcpdev_rdmodinfo.mi_minpsz = 0;
    tcpdev_rdmodinfo.mi_maxpsz = 100;
    tcpdev_rdmodinfo.mi_hiwat = 1024;
    tcpdev_rdmodinfo.mi_lowat = 256;

    /*init tcpdev_streamtab*/
#ifdef PSTREAMS_STRICTTYPES
    tcpdev_wrinit.qi_qopen = tcpdev_open;
    tcpdev_wrinit.qi_putp = tcpdev_wput;
    tcpdev_wrinit.qi_srvp = NULL;
    tcpdev_rdinit.qi_qopen = tcpdev_open;
    tcpdev_rdinit.qi_putp = tcpdev_rput;
    tcpdev_rdinit.qi_srvp = tcpdev_rsrvp;
#else
    tcpdev_wrinit.qi_qopen = (int (*)())tcpdev_open;
    tcpdev_wrinit.qi_putp = (int (*)())tcpdev_wput;
    tcpdev_wrinit.qi_srvp = NULL;
    tcpdev_rdinit.qi_qopen = (int (*)())tcpdev_open;
    tcpdev_rdinit.qi_putp = (int (*)())tcpdev_rput;
    tcpdev_rdinit.qi_srvp = (int (*)())tcpdev_rsrvp;
#endif

    tcpdev_wrinit.qi_minfo = &tcpdev_wrmodinfo;
    tcpdev_rdinit.qi_minfo = &tcpdev_rdmodinfo;

    tcpdev_streamtab.st_wrinit = &tcpdev_wrinit;
    tcpdev_streamtab.st_rdinit = &tcpdev_rdinit;

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: tcpdev_open
Purpose: open procedure. create socket.
Parameters:
Caveats:
******************************************************************************/
int
tcpdev_open(P_QUEUE *q)
{
    TCPDEVAREA *area=NULL;

    /*Default log trace filter value*/
    q->ltfilter = PSTREAMS_LT8; /*LTWARNING and up*/

    /*tcpdevarea is shared with the peer queue*/
    if(q->q_peer && q->q_peer->q_ptr)
    {
        q->q_ptr = q->q_peer->q_ptr;
    }
    else
    {
        area = tcpdev_getarea(NULL);

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
        area->sock = socket(AF_INET, SOCK_STREAM, 0);

        if(area->sock == INVALID_SOCKET)
        {
            PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                                WSAGetLastError();
#else
                                errno;
#endif
#endif
            pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_open: socket() failed. error %d",
                PSTRMHEAD(q)->perrno);

            return P_STREAMS_SUCCESS;
        }

        {
            const char trueval=1;
            
            setsockopt(area->sock, SOL_SOCKET, SO_REUSEADDR, &trueval, sizeof(trueval));
        }
    
        /*sock is in blocking mode*/

        area->flag = TCPDEVFLAG_INIT;
        area->state = TCPDEVSTATE_BIND;
        pstreams_log(q, PSTREAMS_LTINFO, "TCP socket opened");

        area->laddr.sin_family = area->raddr.sin_family = AF_INET;

        q->q_ptr = area;
    }

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: tcpdev_wput
Purpose: put procedure for downstream traffic. process data or control messages
Parameters:
Caveats: Callees should clear msg, on both success and failure
******************************************************************************/
int tcpdev_wput(P_QUEUE *q, P_MSGB *msg)
{
    int status=P_STREAMS_FAILURE;

    ASSERT(msg && msg->b_datap);

    switch(msg->b_datap->db_type)
    {
    case P_M_DATA:
        status = tcpdev_wput_data(q, msg);
        break;
    case P_M_CTL:
    case P_M_PROTO:
        status = tcpdev_wput_ctl(q, msg);
        break;
    default:
        ASSERT(0);/*TODO*/
    }

    /*msg was free'd by a callee above*/

    return status;
}

/******************************************************************************
Name: tcpdev_wput_data
Purpose: 
Parameters:
Caveats: msg is freed/consumed only on any call to this function
******************************************************************************/
int
tcpdev_wput_data(P_QUEUE *q, P_MSGB *msg)
{
    TCPDEVAREA *area=NULL;
    int32 msgsize=0;
    int sockstatus=0;

    ASSERT(msg);

    area = (TCPDEVAREA *)q->q_ptr;

    /*process*/
    msgsize = pstreams_msgsize(msg);

    /*if we do not have access to scatter write for 
     *TCP send then the following block helps
     */
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

#ifdef PSTREAMS_TCPDUMP
    {
        uchar hexbuf[1792*2]={0};

        bintohex(hexbuf, msg->b_rptr, msgsize);

        pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_wput_data: sending %ld bytes\n%s",
            msgsize, hexbuf);
    }
#endif

    sockstatus = send(area->sock, (char *)msg->b_rptr, msgsize, 0);
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
        pstreams_log(q, PSTREAMS_LTERROR, "tcpdev_wput_data: send failed. error %d", 
            PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
       /*
        * NOTE: current policy is to assume something is wrong with this message
        * so drop it; do not retry by putting it back in queue - thomas
        */
        pstreams_freemsg(PSTRMHEAD(q), msg);
        return P_STREAMS_FAILURE; 
    }

    /*sockstatus holds bytes sent by send()*/
    ASSERT(sockstatus <= pstreams_msgsize(msg));

    pstreams_log(q, PSTREAMS_LTINFO, "TCP socket send succeeded."
            " Device State: 0x%x", area->state);

    pstreams_msgconsume(msg, sockstatus);/*consume number of bytes sent*/

    if(pstreams_msgsize(msg) == 0)
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
Name: tcpdev_wput_ctl
Purpose: 
Parameters:
Caveats: msg is freed/consumed on all calls to this function
******************************************************************************/
int
tcpdev_wput_ctl(P_QUEUE *q, P_MSGB *msg)
{
    TCPDEVAREA *area=(TCPDEVAREA *)q->q_ptr;
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
            case TCPDEV_LADDR:
                ASSERT(pstreams_msgsize(msg) >= sizeof(struct sockaddr_in));
                memcpy(&area->laddr, msg->b_rptr, sizeof(struct sockaddr_in));
                area->flag = (TCPDEVFLAG)(area->flag | TCPDEVFLAG_LADDRSET);
                break;

            case TCPDEV_RADDR:
                ASSERT(pstreams_msgsize(msg) >= sizeof(struct sockaddr_in));
                memcpy(&area->raddr, msg->b_rptr, sizeof(struct sockaddr_in));
                area->flag = (TCPDEVFLAG)(area->flag | TCPDEVFLAG_RADDRSET);
                break;

            case TCPDEV_BIND:
                if(area->state != TCPDEVSTATE_BIND)
                {
                    pstreams_log(q, PSTREAMS_LTERROR, "TCP device: bind command"
                        " failed. Wrong state. State=0x%x",
                        area->state);
                       break;
                }
                if(pstreams_msgsize(msg) >= sizeof(struct sockaddr_in))
                {            
                       memcpy(&area->laddr, msg->b_rptr, sizeof(struct sockaddr_in));
                    area->flag = (TCPDEVFLAG)(area->flag | TCPDEVFLAG_LADDRSET);
                }
                else if(!(area->flag & TCPDEVFLAG_LADDRSET))
                {
                    pstreams_log(q, PSTREAMS_LTERROR, 
                            "TCPDEV_BIND command received with no LocalAddress");
                    break;
                }
                /*associate with given address*/
                sockstatus = 
                    bind(area->sock, (struct sockaddr *)&area->laddr, 
                        sizeof(struct sockaddr_in));
                if(sockstatus == SOCKET_ERROR)
                {
                    PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                        WSAGetLastError();
#else
                        errno;
#endif
#endif
                    pstreams_log(q, PSTREAMS_LTERROR, "TCP device: bind failed."
                        " retval %d. lasterror %d",
                        sockstatus, PSTRMHEAD(q)->perrno);
                    break;
                }
                area->state = TCPDEVSTATE_CONNECT;/*ready to connect*/
            
                pstreams_log(q, PSTREAMS_LTINFO, "TCP socket bind succeeded."
                    " Device State: 0x%x", area->state);

                break;
        
            case TCPDEV_CONNECT:
                if(!(area->state & TCPDEVSTATE_CONNECT))
                {
                    pstreams_log(q, PSTREAMS_LTERROR, "TCP device: connect command"
                        " failed. Wrong state. state=0x%x",
                        area->state);
                    break;
                }

                if(pstreams_msgsize(msg) >= sizeof(struct sockaddr_in))
                {
                    memcpy(&area->raddr, msg->b_rptr, sizeof(struct sockaddr_in));
                    area->flag = (TCPDEVFLAG)(area->flag | TCPDEVFLAG_RADDRSET);
                }
                else if(!(area->flag & TCPDEVFLAG_RADDRSET))
                {
                    pstreams_log(q, PSTREAMS_LTERROR, 
                        "TCPDEV_CONNECT received with no RemoteAddress");
                    break;
                }

                /*connect to given address*/
                sockstatus = 
                    connect(area->sock, (struct sockaddr *)&area->raddr, 
                           sizeof(struct sockaddr_in));
                if(sockstatus == SOCKET_ERROR)
                {
                    PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                    WSAGetLastError();
#else
                    errno;
#endif
#endif
                    pstreams_log(q, PSTREAMS_LTERROR, "TCP device: connect failed."
                        " retval %d. lasterror %d",
                        sockstatus, PSTRMHEAD(q)->perrno);
                    break;
                }

                area->state = TCPDEVSTATE_DATA;/*ready to connect*/
                pstreams_log(q, PSTREAMS_LTINFO, "TCP socket connect succeeded."
                    " Device State: 0x%x", area->state);
                break;
            
            case TCPDEV_DISCONNECT:
#if(0)
                sockstatus = shutdown(area->sock, 2);
#else
#ifdef PSTREAMS_WIN32
                sockstatus = closesocket(area->sock);
#else
                sockstatus = close(area->sock);
#endif
                area->sock = socket(AF_INET, SOCK_STREAM, 0);

                if(area->sock == INVALID_SOCKET)
                {
                    PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                                WSAGetLastError();
#else
                                errno;
#endif
#endif
                    pstreams_log(q, PSTREAMS_LTINFO, "socket() failed. error %d",
                        PSTRMHEAD(q)->perrno);
                    break;
                }

                {
                    const char trueval=1;
            
                    setsockopt(area->sock, SOL_SOCKET, SO_REUSEADDR, &trueval, sizeof(trueval));
                }
#endif
                if(sockstatus == SOCKET_ERROR)
                {
                    PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                            WSAGetLastError();
#else
                            errno;
#endif
#endif
                    pstreams_log(q, PSTREAMS_LTERROR, "TCP device: shutdown failed."
                        " retval %d. lasterror %d",
                        sockstatus, PSTRMHEAD(q)->perrno);
                    break;
                }
                area->state = TCPDEVSTATE_CONNECT;/*back to pre-connect state*/
                pstreams_log(q, PSTREAMS_LTINFO, "TCP socket sent disconnect"
                    " Device State: 0x%x", area->state);
                break;

            case TCPDEV_CLOSE:
#ifdef PSTREAMS_WIN32
                sockstatus = closesocket(area->sock);
#else
                sockstatus = close(area->sock);
#endif
                if(sockstatus == SOCKET_ERROR)
                {
                    PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                            WSAGetLastError();
#else
                            errno;
#endif
#endif
                    pstreams_log(q, PSTREAMS_LTERROR, "TCP device: closesocket failed."
                        " retval %d. lasterror %d",
                        sockstatus, PSTRMHEAD(q)->perrno);
                        break;
                }
                area->state = TCPDEVSTATE_INIT;
                area->sock = 0;
                pstreams_log(q, PSTREAMS_LTINFO, "TCPDEV socket closed"
                    " Device State: 0x%x", area->state);
                break;

            default:
                pstreams_log(q, PSTREAMS_LTERROR, "tcpdev_wput_ctl: unknown command %d"
                    " Device State: 0x%x", 
                    ctl.ctlfunc, area->state);
                break;
            }
        }
    
    default:
        ASSERT(0); /*TODO*/
        break;
    }

    pstreams_freemsg(PSTRMHEAD(q), msg);

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: tcpdev_rsrvp
Purpose: service procedure for upstream traffic
Parameters:
Caveats:
******************************************************************************/
int
tcpdev_rsrvp(P_QUEUE *q)
{
    TCPDEVAREA *area=NULL;
    P_MSGB *msg=NULL;
    fd_set sockfds={0};
    int activesockets=0;
    int len = 0;
    struct timeval timeout={0};

    area = (TCPDEVAREA *)q->q_ptr;
    FD_ZERO(&sockfds);
    FD_SET(area->sock, &sockfds);

    activesockets =
        select(1, &sockfds, NULL, NULL, &timeout);/*last argument => non-blocking*/

    if(activesockets == SOCKET_ERROR)
    {
            PSTRMHEAD(q)->perrno = 
#ifdef PSTREAMS_H8
                               tfError(area->sock);
#else
#ifdef PSTREAMS_WIN32
                WSAGetLastError();
#else
                errno;
#endif
#endif
        pstreams_log(q, PSTREAMS_LTERROR, "rsrvp: select failed."
                    " retval %d. lasterror %d",
                    activesockets, PSTRMHEAD(q)->perrno);
                return P_STREAMS_FAILURE;
    }

    while(activesockets > 0)
    {
        if(!FD_ISSET(area->sock, &sockfds))
        {
            continue;
        }

        activesockets--; /*this socket is set*/

        /*assuming only a read event - though other events are possible
         * if socket is non-blocking
         */
        msg = pstreams_allocb((P_STREAMHEAD *)q->strmhead, 1792, 0);
        if(!msg)
        {
#ifdef PSTREAMS_LT
        	pstreams_log(q, PSTREAMS_LTWARNING, "rsrvp: Unable to allocate read buffer. Not reading");
#endif
           	return P_STREAMS_SUCCESS;
        }

        len = recv(area->sock, (char *)msg->b_wptr, 1792, 0);

        if ( len != SOCKET_ERROR )
        {
#ifdef PSTREAMS_UDPDUMP
        /*the block below is space expensive! - TODO verify if needed*/
        	{
            	uchar hexbuf[1792*2] = { 0 };

                bintohex(hexbuf, (uchar *)hexbuf, len);

                pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_rsrvp: rx %d bytes\n%s", len, hexbuf);
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
            pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_wput_data: bytes read=%ld", len);
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
                	pstreams_log(q, PSTREAMS_LTINFO+1, "tcpdev_wput_data: "
                    	"failed in downsizing readbuffer from 1792 to %ld", len);
                }
#endif
			}
#ifdef PSTREAMS_LT
            else
            {
            	pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_wput_data: "
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
                pstreams_log(q, PSTREAMS_LTERROR, "tcpdev_rsrvp: recvfrom failed." " error %d", 
                    PSTRMHEAD(q)->perrno);
#endif /*PSTREAMS_LT*/
                return P_STREAMS_SUCCESS;
       	}

    }

    return P_STREAMS_SUCCESS;
}
    
/******************************************************************************
Name: tcpdev_rput
Purpose: put procedure for upstream traffic.
Parameters:
Caveats:
******************************************************************************/
int
tcpdev_rput(P_QUEUE *q, P_MSGB *msg)
{
    PDBG(msg=NULL); /*unused*/
    PDBG(q=NULL); /*unused*/

    /*can never be called*/
    ASSERT(0);

    return 0;
}


/******************************************************************************
Name: tcpdev_getarea
Purpose: 
Parameters:
Caveats:
******************************************************************************/
static TCPDEVAREA *
tcpdev_getarea(void *key)
{
    static TCPDEVAREA staticmem;
    TCPDEVAREA *tcpdevarea= (TCPDEVAREA *)&staticmem;

    PDBG(key=NULL); /*unused*/

    if(tcpdevarea)
    {
        memset(tcpdevarea, 0, sizeof(TCPDEVAREA));
    }

    tcpdevarea->state = TCPDEVSTATE_INIT;

    return tcpdevarea;
}
