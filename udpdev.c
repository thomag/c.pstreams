/*===========================================================================
FILE: udpdev.c

	streams device module for UDP

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
#include "udpdev.h"

P_QINIT udpdev_wrinit={0};
P_QINIT udpdev_rdinit={0};
P_STREAMTAB udpdev_streamtab={0};
P_MODINFO udpdev_wrmodinfo={0};
P_MODINFO udpdev_rdmodinfo={0};


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
#else
	udpdev_wrinit.qi_qopen = (int (*)())udpdev_open;
	udpdev_wrinit.qi_putp = (int (*)())udpdev_wput;
	udpdev_wrinit.qi_srvp = NULL;
	udpdev_rdinit.qi_qopen = (int (*)())udpdev_open;
	udpdev_rdinit.qi_putp = (int (*)())udpdev_rput;
	udpdev_rdinit.qi_srvp = (int (*)())udpdev_rsrvp;
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

	q->ltfilter = PSTREAMS_LT4;

	/*udpdevarea is shared with the peer queue*/
	if(q->q_peer && q->q_peer->q_ptr)
	{
		q->q_ptr = q->q_peer->q_ptr;
	}
	else
	{
		area = udpdev_getarea(NULL);

		assert(area); /*TODO - handle*/

		area->sock = socket(AF_INET, SOCK_DGRAM, 0);

		if(area->sock == INVALID_SOCKET)
		{
			pstreams_log(q, PSTREAMS_LTERROR, "udpdev_open: socket() failed. error %d",
				errno);
			return P_STREAMS_SUCCESS;
		}

		ioctl(area->sock, SO_REUSEADDR, NULL);
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
Caveats:
******************************************************************************/
int
udpdev_wput(P_QUEUE *q, P_MSGB *msg)
{
	int status=P_STREAMS_FAILURE;

	assert(msg && msg->b_datap);

	switch(msg->b_datap->db_type)
	{
	case P_M_DATA:
		status = udpdev_wput_data(q, msg);
		break;
	case P_M_IOCTL:
		status = udpdev_wput_ctl(q, msg);
		break;
	default:
		;/*TODO*/
	}

	if(status != P_STREAMS_SUCCESS)
	{
		pstreams_relmsg(q, msg);
	}

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
	char buf[MAXUDPDGRAMSIZE] = {0};
	P_BUF pbuf={0};
	int sockstatus=0;

	assert(msg);

	area = (UDPDEVAREA *)q->q_ptr;

	/*process*/
	pbuf.maxlen = MAXUDPDGRAMSIZE;
	pbuf.len = 0;
	pbuf.buf = buf;
	pbuf.len = pstreams_msgread(&pbuf, msg);/*doesn't modify msg*/
	assert(pbuf.len <= MAXUDPDGRAMSIZE);

	if(pstreams_ltfilter(q, PSTREAMS_LTINFO))
	{
		uchar hexbuf[MAXUDPDGRAMSIZE*2]={0};

		bintohex(hexbuf, (uchar *)buf, pbuf.len);

		pstreams_log(q, PSTREAMS_LTINFO, "udpdev_wput_data: sending %d bytes\n%s",
			pbuf.len, hexbuf);
	}

	sockstatus = sendto(area->sock, buf, pbuf.len, 0,
						(struct sockaddr *)&area->raddr, sizeof(area->raddr));

	if(sockstatus == SOCKET_ERROR)
	{
		pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_data: send failed. error %d", errno);
		return P_STREAMS_FAILURE;
	}

	/*sockstatus holds bytes sent by send()*/
	assert(sockstatus <= pstreams_msgsize(msg));

	pstreams_msgconsume(msg, sockstatus);/*consume number of bytes sent*/

	if ( pstreams_msgsize(msg) == 0 )
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
Name: udpdev_wput_ctl
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
udpdev_wput_ctl(P_QUEUE *q, P_MSGB *ctl)
{
	UDPDEVAREA *area=NULL;
	int sockstatus=0;
	P_IOCTL *udpdevctl=NULL;
	struct sockaddr_in *sockaddr;

	assert(ctl && ctl->b_datap);

	assert(ctl->b_datap->db_type == P_M_IOCTL);

	udpdevctl = (P_IOCTL *)ctl->b_rptr;

	switch(udpdevctl->ic_cmd)
	{
		case UDPDEV_RADDR:
			if(udpdevctl->ic_len < sizeof(struct sockaddr_in))
			{
				pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: ctl msg has "
					"invalid payload for UDPDEV_RADDR command");
				return P_STREAMS_FAILURE;
			}
			area = (UDPDEVAREA *)q->q_ptr;
			
			sockaddr = (struct sockaddr_in *)udpdevctl->ic_dp;
			area->raddr.sin_port = sockaddr->sin_port;/*has to be in network form*/
			area->raddr.sin_addr = sockaddr->sin_addr;

#if(0)
			/*first disassociate from existing associations*/
			{
				struct sockaddr_in nulladdr={0};

				nulladdr.sin_family = AF_INET;
				nulladdr.sin_port = htons(5150);
				nulladdr.sin_addr.s_addr = htonl(INADDR_ANY);
				sockstatus = 
					connect(area->sock, (SOCKADDR *)&nulladdr, sizeof(nulladdr));
			}
			if(sockstatus == SOCKET_ERROR)
			{
				pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: connect failed."
					" error %d", errno);
				return P_STREAMS_FAILURE;
			}
#endif
#if(0)
			/*associate with given address*/
			sockstatus = 
				connect(area->sock, (struct sockaddr *)&area->raddr, 
					sizeof(struct sockaddr_in));
			if(sockstatus == SOCKET_ERROR)
			{
				pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: connect failed."
					" retval %d. lasterror %d",
					sockstatus, errno);
				return P_STREAMS_FAILURE;
			}
#endif
			break;

		case UDPDEV_LADDR:
			if(udpdevctl->ic_len < sizeof(struct sockaddr_in))
			{
				pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: ctl msg has "
					"invalid payload for UDPDEV_LADDR command");
				return P_STREAMS_FAILURE;
			}
			area = (UDPDEVAREA *)q->q_ptr;
			sockaddr = (struct sockaddr_in *)udpdevctl->ic_dp;
			area->laddr.sin_port = sockaddr->sin_port;
			area->laddr.sin_addr = sockaddr->sin_addr;

			/*associate with given address*/
			sockstatus = 
				bind(area->sock, (struct sockaddr *)&area->laddr, 
					sizeof(struct sockaddr_in));
			if(sockstatus == SOCKET_ERROR)
			{
				pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: bind failed."
					" retval %d. lasterror %d",
					sockstatus, errno);
				return P_STREAMS_FAILURE;
			}
			break;

		default:
			pstreams_log(q, PSTREAMS_LTERROR, "udpdev_wput_ctl: unknown command %d",
				udpdevctl->ic_cmd);
	}

	pstreams_relmsg(q, ctl);

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
	int activesockets=0;
	char buf[MAXUDPDGRAMSIZE]={0};
	int len = 0;
	struct timeval timeout={0};

	area = (UDPDEVAREA *)q->q_ptr;
	FD_ZERO(&sockfds);
	FD_SET(area->sock, &sockfds);

	activesockets =
		select(1, &sockfds, NULL, NULL, &timeout);/*last argument => non-blocking*/

	if(activesockets == SOCKET_ERROR)
	{
		pstreams_log(q, PSTREAMS_LTERROR, "udpdev_rsrvp: select failed."
					" retval %d. lasterror %d",
					activesockets, errno);
				return P_STREAMS_FAILURE;
	}

	while ( activesockets > 0 )
	{
		struct sockaddr_in faddr={0}; /*from address*/
		int faddrlen=sizeof(faddr);/*length of data returned in faddr*/

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

		if ( len != SOCKET_ERROR )
		{
			/*char __gc *p = new char[len];*/

			len = recvfrom(area->sock, buf, MAXUDPDGRAMSIZE, 0, (struct sockaddr *)&faddr, &faddrlen);

			if ( len != SOCKET_ERROR )
			{
				if ( pstreams_ltfilter(q, PSTREAMS_LTINFO))
				{
					uchar hexbuf[MAXUDPDGRAMSIZE*2] = { 0 };

					bintohex(hexbuf, (uchar *)buf, len);

					pstreams_log(q, PSTREAMS_LTINFO, "udpdev_rsrvp: rx %d bytes\n%s", len, hexbuf);
				}
				msg = pstreams_allocmsgb((P_STREAMHEAD *)q->strmhead, len, 0);

				if ( !msg )
				{
					pstreams_log(q, PSTREAMS_LTERROR, "udpdev_rsrvp: pstreams_allocmsgb failed." " bytes %d", len);
					return P_STREAMS_FAILURE;
				}

				memcpy(msg->b_wptr, buf, len);
				msg->b_wptr += len;
				pstreams_putnext(q, msg);
				msg = NULL;
			}
			else
			{
				pstreams_log(q, PSTREAMS_LTERROR, "udpdev_rsrvp: recvfrom failed." " error %d", errno);
					return P_STREAMS_FAILURE;
			}
		}
		else
		{
			pstreams_log(q, PSTREAMS_LTERROR, "udpdev_rsrvp: recvfrom failed." " error %d", errno);
				return P_STREAMS_FAILURE;
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
	assert(0);

	return 0;
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
	UDPDEVAREA *udpdevarea= (UDPDEVAREA *)malloc(sizeof(UDPDEVAREA));

	if(udpdevarea)
	{
		memset(udpdevarea, 0, sizeof(UDPDEVAREA));
	}

	return udpdevarea;
}
