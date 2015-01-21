/*===========================================================================
FILE: tcpdev.c

	streams device module for TCP

 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "listop.h"
#include "pstreams.h"
#include "tcpdev.h"

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
#ifdef M2STRICTTYPES
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

	q->ltfilter = PSTREAMS_LT6;

	/*tcpdevarea is shared with the peer queue*/
	if(q->q_peer && q->q_peer->q_ptr)
	{
		q->q_ptr = q->q_peer->q_ptr;
	}
	else
	{
		area = tcpdev_getarea(NULL);

		assert(area); /*TODO - handle*/

		area->sock = socket(AF_INET, SOCK_STREAM, 0);

		if(area->sock == INVALID_SOCKET)
		{
			pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_open: socket() failed. error %d",
				errno);
			return P_STREAMS_SUCCESS;
		}

		ioctl(area->sock, SO_REUSEADDR, NULL);
	
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
Caveats:
******************************************************************************/
int
tcpdev_wput(P_QUEUE *q, P_MSGB *msg)
{
	int status=P_STREAMS_FAILURE;

	assert(msg && msg->b_datap);

	switch(msg->b_datap->db_type)
	{
	case P_M_DATA:
		status = tcpdev_wput_data(q, msg);
		break;
	case P_M_IOCTL:
	case P_M_CTL:
		status = tcpdev_wput_ctl(q, msg);
		break;
	default:
		;/*TODO*/
	}

	if(status == P_STREAMS_FAILURE)
	{
		pstreams_relmsg(q, msg);
	}

	return status;
}

/******************************************************************************
Name: tcpdev_wput_data
Purpose: 
Parameters:
Caveats: msg is freed/consumed only on successful calls to this function
******************************************************************************/
int
tcpdev_wput_data(P_QUEUE *q, P_MSGB *msg)
{
	TCPDEVAREA *area=NULL;
	char buf[MAXTCPDGRAMSIZE] = {0};
	P_BUF pbuf={0};
	int sockstatus=0;

	assert(msg);

	area = (TCPDEVAREA *)q->q_ptr;

	/*process*/
	pbuf.maxlen = MAXTCPDGRAMSIZE;
	pbuf.len = 0;
	pbuf.buf = buf;
	pbuf.len = pstreams_msgread(&pbuf, msg);/*doesn't modify msg*/
	assert(pbuf.len <= MAXTCPDGRAMSIZE);

	if(pstreams_ltfilter(q, PSTREAMS_LTINFO))
	{
		uchar hexbuf[MAXTCPDGRAMSIZE*2]={0};

		bintohex(hexbuf, (uchar *)buf, pbuf.len);

		pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_wput_data: sending %d bytes\n%s",
			pbuf.len, hexbuf);
	}

	sockstatus = send(area->sock, buf, pbuf.len, 0);
	if(sockstatus == SOCKET_ERROR)
	{
		pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_wput_data: send failed. error %d",
			errno);
		return P_STREAMS_FAILURE;
	}

	/*sockstatus holds bytes sent by send()*/
	assert(sockstatus <= pstreams_msgsize(msg));

	pstreams_log(q, PSTREAMS_LTINFO, "TCP socket send succeeded."
			" Device State: 0x%x", area->state);

	pstreams_msgconsume(msg, sockstatus);/*consume number of bytes sent*/

	if(pstreams_msgsize(msg) == 0)
	{
		/*done sending msg*/
		pstreams_relmsg(q, msg);
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
Caveats: msg is freed/consumed only on successful calls to this function
******************************************************************************/
int
tcpdev_wput_ctl(P_QUEUE *q, P_MSGB *ctl)
{
	TCPDEVAREA *area=NULL;
	int sockstatus=0;
	P_IOCTL *tcpdevctl=NULL;
	struct sockaddr_in *sockaddr;

	assert(ctl && ctl->b_datap);

	assert(ctl->b_datap->db_type == P_M_IOCTL);

	area = (TCPDEVAREA *)q->q_ptr;

	tcpdevctl = (P_IOCTL *)ctl->b_rptr;

	switch(tcpdevctl->ic_cmd)
	{
		case TCPDEV_LADDR:
			if(tcpdevctl->ic_len < sizeof(struct sockaddr_in))
			{
				pstreams_log(q, PSTREAMS_LTERROR, "tcpdev_wput_ctl: ctl msg has "
					"invalid payload for TCPDEV_LADDR command");
				return P_STREAMS_FAILURE;
			}
			
			sockaddr = (struct sockaddr_in *)tcpdevctl->ic_dp;
			area->raddr.sin_port = sockaddr->sin_port;/*has to be in network form*/
			area->raddr.sin_addr = sockaddr->sin_addr;

			area->flag = (TCPDEVFLAG)(area->flag | TCPDEVFLAG_LADDRSET);
			break;

		case TCPDEV_RADDR:
			if(tcpdevctl->ic_len < sizeof(struct sockaddr_in))
			{
				pstreams_log(q, PSTREAMS_LTERROR, "tcpdev_wput_ctl: ctl msg has "
					"invalid payload for TCPDEV_RADDR command");
				return P_STREAMS_FAILURE;
			}
			
			sockaddr = (struct sockaddr_in *)tcpdevctl->ic_dp;
			area->raddr.sin_port = sockaddr->sin_port;/*has to be in network form*/
			area->raddr.sin_addr = sockaddr->sin_addr;

			area->flag = (TCPDEVFLAG)(area->flag | TCPDEVFLAG_RADDRSET);
			break;

		case TCPDEV_BIND:
			if(area->state != TCPDEVSTATE_BIND)
			{
				pstreams_log(q, PSTREAMS_LTERROR, "TCP device: bind command"
					" failed. Wrong state. State=0x%x",
					area->state);
				return P_STREAMS_FAILURE;
			}
			if(tcpdevctl->ic_len >= sizeof(struct sockaddr_in))
			{			
				sockaddr = (struct sockaddr_in *)tcpdevctl->ic_dp;
				area->laddr.sin_port = sockaddr->sin_port;
				area->laddr.sin_addr = sockaddr->sin_addr;
				area->flag = (TCPDEVFLAG)(area->flag | TCPDEVFLAG_LADDRSET);
			}
			else if(!(area->flag & TCPDEVFLAG_LADDRSET))
			{
				pstreams_log(q, PSTREAMS_LTERROR, 
					"TCPDEV_BIND command received with no LocalAddress");
				return P_STREAMS_FAILURE;
			}
#if(1)
			/*associate with given address*/
			sockstatus = 
				bind(area->sock, (struct sockaddr *)&area->laddr, 
					sizeof(struct sockaddr_in));
			if(sockstatus == SOCKET_ERROR)
			{
				pstreams_log(q, PSTREAMS_LTERROR, "TCP device: bind failed."
					" retval %d. lasterror %d",
					sockstatus, errno);
				return P_STREAMS_FAILURE;
			}
#endif
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
				return P_STREAMS_FAILURE;
			}

			if(tcpdevctl->ic_len >= sizeof(struct sockaddr_in))
			{
				sockaddr = (struct sockaddr_in *)tcpdevctl->ic_dp;
				area->raddr.sin_port = sockaddr->sin_port;
				area->raddr.sin_addr = sockaddr->sin_addr;
				area->flag = (TCPDEVFLAG)(area->flag | TCPDEVFLAG_RADDRSET);
			}
			else if(!(area->flag & TCPDEVFLAG_RADDRSET))
			{
				pstreams_log(q, PSTREAMS_LTERROR, 
					"TCPDEV_CONNECT received with no RemoteAddress");
				return P_STREAMS_FAILURE;
			}

			/*connect to given address*/
			sockstatus = 
				connect(area->sock, (struct sockaddr *)&area->raddr, 
					sizeof(struct sockaddr_in));
			if(sockstatus == SOCKET_ERROR)
			{
				pstreams_log(q, PSTREAMS_LTERROR, "TCP device: connect failed."
					" retval %d. lasterror %d",
					sockstatus, errno);
				return P_STREAMS_FAILURE;
			}

			area->state = TCPDEVSTATE_DATA;/*ready to connect*/
			pstreams_log(q, PSTREAMS_LTINFO, "TCP socket connect succeeded."
				" Device State: 0x%x", area->state);
			break;
			
		case TCPDEV_DISCONNECT:
#if(0)	
			sockstatus = shutdown(area->sock, SD_BOTH);
#else
			sockstatus = close(area->sock);
			area->sock = socket(AF_INET, SOCK_STREAM, 0);

			if(area->sock == INVALID_SOCKET)
			{
				pstreams_log(q, PSTREAMS_LTINFO, "socket() failed. error %d",
					errno);
				return P_STREAMS_SUCCESS;
			}

			ioctl(area->sock, SO_REUSEADDR, NULL);
#endif
			if(sockstatus == SOCKET_ERROR)
			{
				pstreams_log(q, PSTREAMS_LTERROR, "TCP device: shutdown failed."
					" retval %d. lasterror %d",
					sockstatus, errno);
				return P_STREAMS_FAILURE;
			}
			area->state = TCPDEVSTATE_CONNECT;/*back to pre-connect state*/
			pstreams_log(q, PSTREAMS_LTINFO, "TCP socket sent disconnect"
				" Device State: 0x%x", area->state);
			break;


		case TCPDEV_CLOSE:
			sockstatus = close(area->sock);
			if(sockstatus == SOCKET_ERROR)
			{
				pstreams_log(q, PSTREAMS_LTERROR, "TCP device: closesocket failed."
					" retval %d. lasterror %d",
					sockstatus, errno);
				return P_STREAMS_FAILURE;
			}
			area->state = TCPDEVSTATE_INIT;
			area->sock = 0;
			pstreams_log(q, PSTREAMS_LTINFO, "TCPDEV socket closed"
				" Device State: 0x%x", area->state);
			break;

		default:
			pstreams_log(q, PSTREAMS_LTERROR, "tcpdev_wput_ctl: unknown command %d"
				" Device State: 0x%x", 
				tcpdevctl->ic_cmd, area->state);
	}

	pstreams_relmsg(q, ctl);


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
	char buf[MAXTCPDGRAMSIZE]={0};
	int len = 0;
	struct timeval timeout={0};

	area = (TCPDEVAREA *)q->q_ptr;
	FD_ZERO(&sockfds);
	FD_SET(area->sock, &sockfds);

	activesockets =
		select(1, &sockfds, NULL, NULL, &timeout);/*last argument => non-blocking*/

	if(activesockets == SOCKET_ERROR)
	{
		pstreams_log(q, PSTREAMS_LTERROR, "rsrvp: select failed."
					" retval %d. lasterror %d",
					activesockets, errno);
				return P_STREAMS_FAILURE;
	}

	while(activesockets > 0)
	{
		struct sockaddr_in faddr={0}; /*from address*/
		int faddrlen=sizeof(faddr);/*length of data returned in faddr*/

		if(!FD_ISSET(area->sock, &sockfds))
		{
			continue;
		}

		activesockets--; /*this socket is set*/

		/*assuming only a read event - though other events are possible
		 * if socket is non-blocking
		 */
		len = recv(area->sock, buf, MAXTCPDGRAMSIZE, 0);
		if(len != SOCKET_ERROR)
		{
			if(pstreams_ltfilter(q, PSTREAMS_LTINFO))
			{
				uchar hexbuf[MAXTCPDGRAMSIZE*2]={0};

				bintohex(hexbuf, (uchar *)buf, len);

				pstreams_log(q, PSTREAMS_LTINFO, "tcpdev_rsrvp: rx %d bytes\n%s",
					len, hexbuf);
			}
			
			/*len = 0 could mean a shutdown request not handled here - TODO*/
			msg = pstreams_allocmsgb((P_STREAMHEAD *)q->strmhead, len, 0);
			if(!msg)
			{
				pstreams_log(q, PSTREAMS_LTINFO, "rsrvp: pstreams_allocmsgb failed."
					" bytes %d", len);
				return P_STREAMS_FAILURE;
			}
			memcpy(msg->b_wptr, buf, len);
			msg->b_wptr += len;
			pstreams_putnext(q, msg);
			msg = NULL;
		}
		else
		{
			pstreams_log(q, PSTREAMS_LTINFO, "rsrvp: recv failed."
					" error %d", errno);
				return P_STREAMS_FAILURE;
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
	/*can never be called*/
	assert(0);

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
	TCPDEVAREA *tcpdevarea= (TCPDEVAREA *)malloc(sizeof(TCPDEVAREA));

	if(tcpdevarea)
	{
		memset(tcpdevarea, 0, sizeof(TCPDEVAREA));
	}

	tcpdevarea->state = TCPDEVSTATE_INIT;

	return tcpdevarea;
}
