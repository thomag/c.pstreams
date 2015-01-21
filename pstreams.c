/*===========================================================================
FILE: pstreams.c

Description: 
   Portable and Pure streams. Routines implementing a 'unix streams' inspired
design for layered protocols

Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h> /*ansi C style variable args*/
#include "pstreams.h"
#include "stdmod.h"
#include "udpdev.h"
#include "tcpdev.h"

/*
 * The streamhead is an object exposed to applications.
 * It provides 2 interfaces, one to the applications and
 * one to the device below. Each of these interfaces are
 * themselves modelled as modules. The declarations below
 * are the streamtabs for these modules.
 */
extern P_STREAMTAB stddev_streamtab; /*module interfacing to a NULL device*/
extern P_STREAMTAB udpdev_streamtab; /*module interfacing to UDP device*/
extern P_STREAMTAB tcpdev_streamtab; /*module interfacing to TCP device*/
extern P_STREAMTAB stdapp_streamtab; /*module interfacing to application*/

/******************************************************************************
Name: pstreams_open
Purpose: creates and returns a streamhead representing a direct connection to
	the given device.
Parameters:
	in: devid is a enum of type P_STREAMS_DEVID for the device to connect to
Caveats: memory allocated here
******************************************************************************/
P_STREAMHEAD *
pstreams_open(int devid/*of type P_STREAMS_DEVID*/)
{
	P_STREAMHEAD *strmhead=NULL;

	/*get memory from heap*/
	strmhead = (P_STREAMHEAD *)malloc(sizeof(P_STREAMHEAD));
	memset(strmhead, 0, sizeof(P_STREAMHEAD));

	/*set log trace file as soon as possible*/
	strmhead->ltfname[0] = '\0'; /*set log/trace file name to NULL*/

	strmhead->ltfile=stderr;


	switch(devid)
	{
		case P_NULL:
			/*The NULL device, this device drops all messages to it*/
			stddev_init(); /*for now stddev is a NULL device*/
			strmhead->devmod = stddev_streamtab;/*structure copy*/
			break;

		case P_UDP:
			/*UDP device*/
			udpdev_init();
			strmhead->devmod = udpdev_streamtab;/*structure copy*/
			break;

		case P_TCP:
			/*TCP device*/
			tcpdev_init();
			strmhead->devmod = tcpdev_streamtab;/*structure copy*/
			break;

		default:
			printf("pstreams_open Unknown device id : %d\n", devid);
			free(strmhead);
			return NULL;
	}

	strmhead->devid	= (P_STREAMS_DEVID) devid;

	stdapp_init();
	strmhead->appmod = stdapp_streamtab;/*structure copy*/

	/*the top and bottom modules(appmod and devmod resp.) have been created*/

	/*allocate memory pools for 'queue's to be used when modules are pushed in*/
	strmhead->qpool = lop_allocpool(sizeof(P_QUEUE), MAXQUEUES, NULL);

	/*allocate memory pools for passing messages internally*/
	strmhead->msgpool = lop_allocpool(sizeof(P_MSGB), MAXMSGBS, NULL);
	strmhead->datapool = lop_allocpool(sizeof(P_DATAB), MAXDATABS, NULL);

	/*queue pairs in each module know their peer*/
	strmhead->apprdq.q_peer = &strmhead->appwrq;
	strmhead->appwrq.q_peer = &strmhead->apprdq;

	strmhead->devrdq.q_peer = &strmhead->devwrq;
	strmhead->devwrq.q_peer = &strmhead->devrdq;

	pstreams_init_queue(strmhead, &strmhead->appwrq, strmhead->appmod.st_wrinit);
	pstreams_init_queue(strmhead, &strmhead->apprdq, strmhead->appmod.st_rdinit);

	pstreams_init_queue(strmhead, &strmhead->devwrq, strmhead->devmod.st_wrinit);
	pstreams_init_queue(strmhead, &strmhead->devrdq, strmhead->devmod.st_rdinit);

	/*connect the application interface queues to the device interface queues*/
	pstreams_connect_queue(&strmhead->appwrq, &strmhead->devwrq);
	pstreams_connect_queue(&strmhead->devrdq, &strmhead->apprdq);

	if(strmhead->appwrq.q_qinfo.qi_qopen)
	{
		strmhead->appwrq.q_qinfo.qi_qopen(&strmhead->appwrq);
	}
	if(strmhead->apprdq.q_qinfo.qi_qopen)
	{
		strmhead->apprdq.q_qinfo.qi_qopen(&strmhead->apprdq);
	}

	if(strmhead->devwrq.q_qinfo.qi_qopen)
	{
		strmhead->devwrq.q_qinfo.qi_qopen(&strmhead->devwrq);
	}
	if(strmhead->devrdq.q_qinfo.qi_qopen)
	{
		strmhead->devrdq.q_qinfo.qi_qopen(&strmhead->devrdq);
	}

	return strmhead;
}

/******************************************************************************
Name: pstreams_setltfile
Purpose: designates a file for log and trace messages output to supplant the
         the default STDERR
Parameters:
Caveats: 
******************************************************************************/
FILE *
pstreams_setltfile(P_STREAMHEAD *strmhead, char *ltfilename)
{
	strmhead->ltfile = fopen(ltfilename, "w");

	return strmhead->ltfile;
}
	
/******************************************************************************
Name: pstreams_close
Purpose: the reverse of pstreams_open
Parameters:
Caveats: memory release. Can't be called twice on same memory.
******************************************************************************/
int
pstreams_close(P_STREAMHEAD *strmhead)
{

	/*TODO verify - free allocated pools and strmhead*/

	/*empty the stream*/
	while(pstreams_pop(strmhead) != 0) ;

	free(strmhead);

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_push
Purpose: push a module onto the stream. The module is linked just below the
	streamhead.
Parameters:
Caveats:
******************************************************************************/
int
pstreams_push(P_STREAMHEAD *strmhead, const P_STREAMTAB *mod)
{
	P_QUEUE *wrq, *rdq;
	P_QUEUE *q;/*loop variable*/

	wrq = (P_QUEUE *)lop_alloc(strmhead->qpool);
	rdq = (P_QUEUE *)lop_alloc(strmhead->qpool);

	/*special - to satisfy the condition 
	 *"that the routines associated with 
	 *one half of a stream module may find 
	 *the queue associated with the other half"
	 */
	wrq->q_peer = rdq;
	rdq->q_peer = wrq;

	pstreams_init_queue(strmhead, wrq, mod->st_wrinit);
	pstreams_init_queue(strmhead, rdq, mod->st_rdinit);

	wrq->q_next = strmhead->appwrq.q_next;
	strmhead->appwrq.q_next = wrq;

	/*topmost modules Q doesn't have other Q's above it*/
	assert(strmhead->apprdq.q_next == NULL); 
	
	/*get the Q below apprdq*/
	for(q=&strmhead->devrdq; q && q->q_next; q=q->q_next )
	{
		if(q->q_next == &strmhead->apprdq)
		{
			break;
		}
	}

	assert(q->q_next == &strmhead->apprdq);
		
	rdq->q_next = q->q_next;
	q->q_next = rdq;

	/*this module is now linked. Call open for each queue*/
	if(wrq->q_qinfo.qi_qopen)
	{
		wrq->q_qinfo.qi_qopen(wrq);
	}
	if(rdq->q_qinfo.qi_qopen)
	{
		rdq->q_qinfo.qi_qopen(rdq);
	}

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_pop
Purpose: the complement of pstreams_push. returns the module 
        id number if successful; 0 otherwise
Parameters:
Caveats:
******************************************************************************/
int
pstreams_pop(P_STREAMHEAD *strmhead)
{
	P_QUEUE *wrq, *rdq;
	ushort mi_idnum=0;

	if((strmhead->appwrq.q_next == &strmhead->devwrq) ||
	   (strmhead->devrdq.q_next == &strmhead->apprdq))
	{
		/*the stream is empty save for the app and dev modules*/
		return 0;
	}

	/*close each q's module specifics before unlinking*/

	wrq = strmhead->appwrq.q_next;
	mi_idnum = wrq->q_qinfo.qi_minfo->mi_idnum;

	if(wrq->q_qinfo.qi_qclose)
	{
		wrq->q_qinfo.qi_qclose(wrq);
	}
	strmhead->appwrq.q_next = wrq->q_next;
	wrq->q_next = NULL; /*for safety*/
	lop_release(strmhead->qpool, wrq);

	rdq = strmhead->apprdq.q_next;
	assert(mi_idnum == rdq->q_qinfo.qi_minfo->mi_idnum);

	if(rdq->q_qinfo.qi_qclose)
	{
		rdq->q_qinfo.qi_qclose(rdq);
	}
	strmhead->apprdq.q_next = rdq->q_next;
	rdq->q_next = NULL; /*for safety*/
	lop_release(strmhead->qpool, rdq);
	
	return mi_idnum;/*return the ID number of the module popped*/
}

#ifdef DEADCODE
P_MDBBLOCK *
pstreams_allocb(P_STREAMHEAD *strmhead, int size, unsigned int priority)
{
	P_MSGB *msg=NULL;
	P_DATAB *data=NULL;
	P_MDBBLOCK *mdbblock = (P_MDBBLOCK *)lop_alloc(strmhead->mdbpool);
	if(!mdbblock)
	{
		return NULL;
	}
	msg = mdbblock->msgblock.m_mblock = 
			(P_MSGB *)mdbblock->databuf; /*mblk first*/
	data = mdbblock->datablock.d_dblock = 
			(P_DATAB *)mdbblock->databuf + sizeof(P_MSGB); /*dblk next*/

	msg->b_datap = data;
	msg->b_rptr = msg->b_wptr = (unsigned char *)data;
	
	data->db_base = (unsigned char *)data;
	data->db_lim = (unsigned char *)(data + sizeof(*mdbblock->databuf));

	return mdbblock;
}
#endif
	
/******************************************************************************
Name: pstreams_allocmsgb
Purpose: allocate a P_MSGB structure(message block). This also allocates a
	P_DATAB structure - we may consider seperating the two later.
Parameters: strmhead  - to access the pool from which to allocate
            size - size of databuffer in allocate message block(ignored for now)
            priority - priority of databuffer(ignored for now)
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_allocmsgb(P_STREAMHEAD *strmhead, int size, unsigned int priority)
{
	P_MSGB *msgb = NULL;

	/*
	 * size and priority parameters are unused for now
	 */

	msgb = (P_MSGB *)lop_alloc(strmhead->msgpool);
	if(!msgb)
	{
		return NULL;
	}

	msgb->b_datap = (P_DATAB *)lop_alloc(strmhead->datapool);
	if(!msgb->b_datap)
	{
		return NULL;
	}

	/*DEBUG mode*/
	lop_checkpool(strmhead->datapool);

	memset(msgb->b_datap, 0, sizeof(P_DATAB));
	msgb->b_datap->db_base = msgb->b_datap->data; /*first byte in buffer*/
	msgb->b_datap->db_lim = msgb->b_datap->data + MAXDATABSIZE; /*TODO - replace MAXDATABSIZE with sizeof(msgb->b_datap->data)*/
								/*last byte plus one in buffer*/
	msgb->b_datap->db_ref = 1; /*reference count is 1*/
	
	/*read-ptr and write-ptr of message block point to start of data block*/
	msgb->b_rptr = msgb->b_wptr = msgb->b_datap->db_base;

	pstreams_log(&strmhead->appwrq, PSTREAMS_LTDEBUG, 
		"pstreams_allocmsgb: bytes %d/%d.",
			size, pstreams_unwritbytes(msgb));

	return msgb;
}

/******************************************************************************
Name: pstreams_putmsg
Purpose: Apps use this function to send messages down the stream.
Parameters: ctlbuf for control part of message. msgbuf for data part of message.
Caveats: memory allocated
******************************************************************************/
int
pstreams_putmsg(P_STREAMHEAD *strmhead, P_BUF *ctlbuf, P_BUF *msgbuf, int flags)
{
	P_MSGB *ctl=NULL; /*control part of msg*/
	P_MSGB *msg=NULL; /*data part of msg */
	P_MSGB *tmsg=NULL;/*total msg*/

	/*
	 * ctlbuf holds the control part of the message. For eg. that this message
	 * is a continuation from the last message for this session(like T_MORE set).
	 * This can also be used to send a command to a particular module
	 */
	if(ctlbuf && ctlbuf->len > 0)
	{
		ctl = pstreams_allocmsgb(strmhead, ctlbuf->len, 0);
		ctl->b_datap->db_type = P_M_IOCTL;

		memcpy(ctl->b_wptr, ctlbuf->buf, ctlbuf->len);
		ctl->b_wptr += ctlbuf->len;

		pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_putmsg: ctl bytes %d.", 
			pstreams_msgsize(ctl));
	}

	/*next check for and send data messages*/
	if(msgbuf && msgbuf->len > 0)
	{
		msg = pstreams_allocmsgb(strmhead, msgbuf->len, 0);
		msg->b_datap->db_type = P_M_DATA;

		memcpy(msg->b_wptr, msgbuf->buf, msgbuf->len);
		msg->b_wptr += msgbuf->len;

		pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_putmsg: msg bytes %d.", 
			pstreams_msgsize(msg));
	}

	if(ctl)
	{
		tmsg = ctl;
	}
	if(msg)
	{
		if(tmsg)
		{
			tmsg->b_cont = msg;
		}
		else
		{
			tmsg = msg;
		}
	}

	if(!pstreams_canput(&strmhead->appwrq))
	{
		pstreams_log(&strmhead->appwrq, PSTREAMS_LTERROR, 
			"putmsg on STREAMHEAD failed with flow control restrictions");
		pstreams_relmsg(&strmhead->appwrq, tmsg);

		return P_STREAMS_FAILURE;
	}

	(void) strmhead->appwrq.q_qinfo.qi_putp(&strmhead->appwrq, tmsg);

	return P_STREAMS_SUCCESS;
}


/******************************************************************************
Name: pstreams_getmsg
Purpose: Apps use this function to get messages of the streamhead
Parameters: on return ctlbuf holds control messages if any. msgbuf holds 
	data messages if any.
Caveats:
******************************************************************************/
int
pstreams_getmsg(P_STREAMHEAD *strmhead, P_BUF *ctlbuf, P_BUF *msgbuf, int *pflags)
{
	P_MSGB *msg;
	
	if(ctlbuf) /*unused*/
	{
		ctlbuf->len = 0;
	}
	if(msgbuf) /*init to empty*/
	{
		msgbuf->len = 0;

/*DEBUG MODE*/
memset(msgbuf->buf, 0, msgbuf->maxlen);

	}

	msg = pstreams_getq(&strmhead->apprdq);
	if(!msg)
	{
		/*no message*/
		return P_STREAMS_SUCCESS;
	}

	pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_getmsg: msg bytes %d.",
			pstreams_msgsize(msg));

	msgbuf->len = (int)pstreams_msgsize(msg);
	if(msgbuf->maxlen >= msgbuf->len)
	{
		msgbuf->len = pstreams_msgread(msgbuf, msg);
		pstreams_msgconsume(msg, msgbuf->len);
		pstreams_relmsg(&strmhead->apprdq, msg);
	}
	else
	{
		/*not enough memory - inform user*/
		msgbuf->len = -1;
		memset(msgbuf->buf, 0, msgbuf->maxlen);
		/*msg goes back in*/
		lop_push(&strmhead->apprdq.q_msglist, msg);
	}
	
	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_msgread
Purpose: reads from msg onto wbuf. *msg is not modified. Steps thru message 
	continuations via msg->b_cont
Parameters:
Caveats: 
******************************************************************************/
uint
pstreams_msgread(P_BUF *wbuf, P_MSGB *msg)
{
	int size=0;
	uint chunksize=0;
	char *wptr = (char *)wbuf->buf;/*pointer to next byte to write*/

	while(msg)
	{
		if(chunksize = pstreams_msg1size(msg))
		{
			if(wbuf->maxlen < size)
			{
				/*assign and return 'read' length*/
				return (wbuf->len = size);
			}
			memcpy(wptr, msg->b_rptr, chunksize);
			/*msg->b_rptr += chunksize; don't consume*/
			wptr += chunksize;/*advance pointer for next memcpy*/
			size += chunksize;
		}
		
		msg = msg->b_cont;
	}

	return (wbuf->len = size);
}

/******************************************************************************
Name: pstreams_msgwrite
Purpose:  complement of pstreams_msgread() - writes data from rbuf into msg.
	rbuf is not modified.
Parameters:
Caveats: 
	Note: msg could be a link list of msgs - after we start the
	copy onto a msg, subsequent msgs should be empty, else we
	we end up writing in the middle portion
******************************************************************************/
uint
pstreams_msgwrite(P_MSGB *msg, P_BUF *rbuf)
{
	int bytestocopy=rbuf->len;
	char *rbufptr = (char *)rbuf->buf;
	int chunksize=0;

	while((bytestocopy > 0) && msg)
	{
		chunksize = MIN((int)pstreams_unwrit1bytes(msg), bytestocopy);

		if(chunksize > 0) 
		{
			memcpy(msg->b_wptr, rbufptr, chunksize);
			msg->b_wptr += chunksize;
			rbufptr += chunksize;
			
			bytestocopy -= chunksize;
		}
		
		msg = msg->b_cont;
		/*
		 * the below assert checks that once we start the copy subsequent
		 * msgs(until the end hence pstreams_msgsize and not pstreams_msg1size)
		 * are empty
		 */
		assert(bytestocopy == rbuf->len || pstreams_msgsize(msg) == 0);
	}

	return (rbuf->len - bytestocopy);
}

/******************************************************************************
Name: pstreams_msgconsume
Purpose: advances the read pointer of "msg" by "bytes"
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_msgconsume(P_MSGB *msg, uint bytes)
{
	uint chunksize=0;

	while(msg && bytes>0)
	{
		if(chunksize = MIN(pstreams_msg1size(msg), bytes))
		{
			msg->b_rptr += chunksize;
			assert(msg->b_rptr <= msg->b_wptr);
			bytes -= chunksize;
		}
		
		msg = msg->b_cont;
	}

	return (bytes == 0 ? P_STREAMS_SUCCESS : P_STREAMS_FAILURE);
}

/******************************************************************************
Name: pstreams_msgerase
Purpose: erases by retreating the write pointer. Returns the number of bytes
		remaining to be erased out of the requested number of bytes. If zero
		was returned that means all requested bytes were erased and the operation
		was a complete success.
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_msgerase(P_MSGB *msg, uint bytes)
{

	if(msg->b_cont)
	{
		bytes -= pstreams_msgerase(msg->b_cont, bytes);/*recursive call*/
	}
	if(bytes > 0)
	{
		bytes -= pstreams_msg1erase(msg, bytes);
	}

	return bytes; /*represents bytes yet to be erased*/
}

/******************************************************************************
Name: pstreams_msg1erase
Purpose: erases by retreating the write pointer - no continuations
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_msg1erase(P_MSGB *msg, uint bytes)
{
	uint chunksize=0;

	if(msg && bytes>0)
	{
		chunksize = MIN(bytes, pstreams_msg1size(msg));
		msg->b_wptr -= chunksize;
		assert(msg->b_rptr <= msg->b_wptr);
	}

	return chunksize;
}

/******************************************************************************
Name: pstreams_callsrvp
Purpose: public function to be called periodically to enable queues. This will
	not needed in a multi-threaded environment.
Parameters:
Caveats:
******************************************************************************/
int
pstreams_callsrvp(P_STREAMHEAD *strmhead)
{
	P_QUEUE *dq; /*downstream queues*/
	P_QUEUE *uq; /*upstream queues*/

/*DEBUG mode*/
pstreams_checkmem(strmhead);

	/*step thru downstream queues calling srvp() on each*/
	for(dq=&strmhead->appwrq;
	    dq;
	    dq = dq->q_next)
	{
		pstreams_log(dq, PSTREAMS_LTDEBUG, "pstreams_callsrvp");

		if(dq->q_qinfo.qi_srvp)
		{
			dq->q_qinfo.qi_srvp(dq);
		}
		else
		{
			/*default*/
			pstreams_srvp(dq);
		}
	}

	/*step thru upstream queues calling srvp() on each*/
	for(uq=&strmhead->devrdq;
	    uq;
	    uq = uq->q_next)
	{
		pstreams_log(uq, PSTREAMS_LTDEBUG, "pstreams_callsrvp");

		if(uq->q_qinfo.qi_srvp)
		{
			uq->q_qinfo.qi_srvp(uq);
		}
		else
		{
			/*default*/
			pstreams_srvp(uq);
		}
	}

/*DEBUG mode*/
pstreams_checkmem(strmhead);

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_srvp
Purpose: default service procedure. 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_srvp(P_QUEUE *q)
{
	P_MSGB *msg=NULL;

	while((q->q_count > 0) && pstreams_canput(q->q_next))
	{
		msg = pstreams_getq(q);
		assert(msg);/*we checked non-zero q->q_count earlier*/

		/*module specific processing go here*/

		/*send this msg upstream*/
		pstreams_putnext(q, msg);
	}

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_getq
Purpose: get oldest message of a queue
Parameters:
Caveats:
******************************************************************************/
P_MSGB *
pstreams_getq(P_QUEUE *q)
{
	P_MSGB *msg = NULL;

	if ( (msg = (P_MSGB *)lop_dequeue(&q->q_msglist)) == NULL )
	{
		q->q_flag |= QWANTR; /*want to read from this q*/
		/*need to back-enable*/
	}
	else
	{
		q->q_count -= pstreams_msgsize(msg);

		if(q->q_count < q->q_hiwat)
		{
			/*allow previous queue to give us data*/
			q->q_flag &= ~QFULL; /*clear QFULL bit*/
		}

		if(q->q_count < q->q_lowat)
		{
			/*need to back-enable*/
		}

	}

	pstreams_log(q, PSTREAMS_LTINFO, "gettq: removed %d bytes from q",
				pstreams_msgsize(msg));

	return msg;
}

/******************************************************************************
Name: pstreams_putnext
Purpose: send message downstream
Parameters:
Caveats:
******************************************************************************/
int
pstreams_putnext(P_QUEUE *wrq, P_MSGB *msg)
{
	pstreams_log(wrq, PSTREAMS_LTINFO, "pstreams_putnext: transferred %d bytes to %s.",
			pstreams_msgsize(msg), wrq->q_next->q_qinfo.qi_minfo->mi_idname);

	return wrq->q_next->q_qinfo.qi_putp(wrq->q_next, msg);
}

/******************************************************************************
Name: pstreams_putq
Purpose: inserts message in queue's message list
Parameters:
Caveats:
******************************************************************************/
int 
pstreams_putq(P_QUEUE *q, P_MSGB *msg)
{
	/*
	 * TODO : find correct place in queue to insert msg. Think
	 * high priority msgs
	 */
	if((q->q_msglist=lop_queue(&q->q_msglist, msg)) == NULL)
	{
		return P_STREAMS_FAILURE;
	}

	q->q_count += pstreams_msgsize(msg);

	if(q->q_count >= q->q_hiwat)
	{
		q->q_flag |= QFULL;
	}

	/*if msg is a high priority msg q->q_enabled = P_TRUE*/

	if(q->q_flag & QWANTR) /*someone wants to read from me*/
	{
		if(!(q->q_flag & QNOENB))
		{
			q->q_enabled = P_TRUE;
		}
	}

	pstreams_log(q, PSTREAMS_LTINFO, "putq: added %d bytes to q",
				pstreams_msgsize(msg));

	return P_STREAMS_SUCCESS; 
}

/******************************************************************************
Name: pstreams_putbq
Purpose: inserts message in queue's message list but adds it to the front
	of the queue. 
	sample usage:
		while(msg=pstreams_getq(q))
		{
			if(!pstreams_canput(q->q_next))
			{
				pstreams_putbq(q);
				break;
			}

			pstreams_putnext(q->q_next, msg);
		}
	Note that for the above logic to work q has to be locked
	from the call to pstreams_getq() thru pstreams_putbq()

Parameters:
Caveats:
******************************************************************************/
int 
pstreams_putbq(P_QUEUE *q, P_MSGB *msg)
{
	/*
	 * Note the usage of lop_push() in a queue! This
	 * depends on the implementation of listop, in which
	 * both queue and stack are the same physically. In
	 * this case lop_push() adds the msg to the front of the
	 * queue; the place from which it presumably was just dequeued
	 */
	if((q->q_msglist=lop_push(&q->q_msglist, msg)) == NULL)
	{
		return P_STREAMS_FAILURE;
	}

	q->q_count += pstreams_msgsize(msg);

	if(q->q_count >= q->q_hiwat)
	{
		q->q_flag |= QFULL;
	}

	/*if msg is a high priority msg q->q_enabled = P_TRUE*/

	if(q->q_flag & QWANTR) /*someone wants to read from me*/
	{
		if(!(q->q_flag & QNOENB))
		{
			q->q_enabled = P_TRUE;
		}
	}

	pstreams_log(q, PSTREAMS_LTINFO, "putbq: added %d bytes back to q",
				pstreams_msgsize(msg));

	return P_STREAMS_SUCCESS; 
}

/******************************************************************************
Name: pstreams_putctl
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_putctl(P_QUEUE *q, int ctlflag)
{
	P_MSGB *msg=NULL;

	assert(q);
	
	switch(ctlflag)
	{
	case P_M_DELAY:
		msg = pstreams_allocmsgb(pstreams_get_strmhead(q), 0,0);
		msg->b_datap->db_type = P_M_DELAY;
		break;
	case P_M_BREAK:
		msg = pstreams_allocmsgb(pstreams_get_strmhead(q), 0,0);
		msg->b_datap->db_type = P_M_BREAK;
		break;
	case P_M_DELIM:
		msg = pstreams_allocmsgb(pstreams_get_strmhead(q), 0,0);
		msg->b_datap->db_type = P_M_DELIM;
		break;
	default:
		assert(0);
		break;
	}
	
	if(msg)
	{
		pstreams_putq(q,msg);
	}

	return P_STREAMS_SUCCESS;
}
	
/******************************************************************************
Name: pstreams_canput
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_canput(P_QUEUE *q)
{
	if(!q)
	{
		return P_FALSE;
	}

	/*beware q_hiwat must be below max capacity by atleast max message size*
	/*otherwise the next could be large enough to swamp us*/

	if(q->q_flag & QFULL)
	{
		q->q_flag |= QWANTW; /*indicates that a function wants to put
							data in this queue; but is not being allowed
							to do so*/
		return P_FALSE;
	}

	/*clear QFULL if below low water mark*/
	if(q->q_count < q->q_lowat)
	{
		q->q_flag &= ~QFULL;
	}

	return P_TRUE;
}

/******************************************************************************
Name: pstreams_get_strmhead
Purpose: 
Parameters:
Caveats:
******************************************************************************/
P_STREAMHEAD *
pstreams_get_strmhead(P_QUEUE *q)
{
	return (P_STREAMHEAD *)q->strmhead;
}

/******************************************************************************
Name: pstreams_put_strmhead
Purpose: 
Parameters:
Caveats:
******************************************************************************/
void
pstreams_put_strmhead(P_QUEUE *q, P_STREAMHEAD *strmhead)
{
	q->strmhead = (P_STREAMHEAD *)strmhead;

	return;
}

/******************************************************************************
Name: pstreams_init_queue
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int 
pstreams_init_queue(P_STREAMHEAD *strmhead, P_QUEUE *q, P_QINIT *qi)
{
	pstreams_put_strmhead(q, strmhead);

	q->q_qinfo = *qi;/*note qi->qi_minfo and qi->qi_mstat are only shallow copied*/
	q->q_msglist = NULL;
	q->q_ptr = NULL;
	q->q_enabled = (P_BOOL)true;
	q->q_next = NULL;


	/*get some defaults from qi*/
	q->q_count = 0;
	q->q_flag = QRESET;/*unused*/
	q->q_ptr = NULL;/*to be set by the module's open()proc*/
	q->q_minpsz = qi->qi_minfo->mi_minpsz;
	q->q_maxpsz = qi->qi_minfo->mi_maxpsz;
	q->q_hiwat = qi->qi_minfo->mi_hiwat;
	q->q_lowat = qi->qi_minfo->mi_lowat;

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_connect_queue
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_connect_queue(P_QUEUE *inq, P_QUEUE *outq)
{
	inq->q_next = outq;

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_relmsg
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_relmsg(P_QUEUE *q, P_MSGB *msg)
{
	P_STREAMHEAD *strmhead = pstreams_get_strmhead(q);

	if(msg)
	{
		if(msg->b_cont)
		{
			/*recursively release*/
			pstreams_relmsg(q, msg->b_cont);
		}


		/*DEBUG MODE*/
		memset(msg->b_datap, 0, sizeof(P_DATAB));
		lop_release(strmhead->datapool, msg->b_datap);

		memset(msg, 0, sizeof(P_MSGB));
		lop_release(strmhead->msgpool, msg);

	}

	pstreams_log(q, PSTREAMS_LTDEBUG, "pstreams_relmsg: bytes released %d.",
			pstreams_msgsize(msg));

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_msgsize
Purpose: Calculate the size of "msg", including continuations
Parameters:
Caveats:
******************************************************************************/
/*uses tail recursion*/
ushort pstreams_msgsize(P_MSGB *msg)
{
	ushort msgsiz=0;

	if(!msg)
	{
		return 0;
	}

	assert(msg->b_next == NULL);/*unused*/
	assert(msg->b_prev == NULL);/*unused*/

	msgsiz = msg->b_wptr - msg->b_rptr;/*could be call to pstreams_msg1size()*/
	assert(msgsiz < MAXDATABSIZE);

	/*recursively add msgsize until end of list b_cont*/
	return msgsiz + pstreams_msgsize(msg->b_cont); 
}

/******************************************************************************
Name: pstreams_msg1size
Purpose: Calculate the size of this message block. Do not consider continuations
Parameters:
Caveats:
******************************************************************************/
ushort pstreams_msg1size(P_MSGB *msg)
{
	ushort msgsiz=0;

	assert(msg);

	msgsiz = msg->b_wptr - msg->b_rptr;
	assert(msgsiz < MAXDATABSIZE);

	return msgsiz;
}

/******************************************************************************
Name: pstreams_unwrit1bytes
Purpose: Calculate the free space available on this message block, for writing.
	Do not consider continuations.
Parameters:
Caveats:
******************************************************************************/
uint pstreams_unwrit1bytes(P_MSGB *msg)
{
	int unwrit1bytes=0;

	if(!msg)
	{
		return 0;
	}

	unwrit1bytes = (char *)msg->b_datap->db_lim - (char *)msg->b_wptr;

	return unwrit1bytes;
}

/******************************************************************************
Name: pstreams_unwritbytes
Purpose: Calculate the unused space in this message.
	Step thru continuations also.
Parameters:
Caveats: Not very useful as the calculated byte count includes "holes".
******************************************************************************/
uint pstreams_unwritbytes(P_MSGB *msg)
{
	int unwritbytes=0;

	if(!msg)
	{
		return 0;
	}

	unwritbytes = pstreams_unwrit1bytes(msg);

	return unwritbytes + pstreams_unwritbytes(msg->b_cont);
}

/******************************************************************************
Name: pstreams_msg1copy
Purpose: copy "bytetocopy" bytes from "from" messageblock to "to" messageblock
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_msg1copy(P_MSGB *to, P_MSGB *from, uint bytestocopy)
{
	assert(to && (pstreams_unwrit1bytes(to) >= bytestocopy));
	assert(from && (pstreams_msg1size(from) >= bytestocopy));

	memcpy(to->b_wptr, from->b_rptr, bytestocopy);
	to->b_wptr += bytestocopy;
	assert(to->b_wptr <= to->b_datap->db_lim);/*don't overrun the edge*/
	/*from->b_rptr += bytestocopy; - note: copying doesn't consume */

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_msgcpy
Purpose: copy 'bytestocopy' bytes 'from' -> 'to'
	assumes there is enough space in 'to', taking
	into account continuations in from->b_cont pointers
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_msgcpy(P_MSGB *to, P_MSGB *from, uint bytestocopy)
{
	uint msgchunk=0;
	uint bitesize=0;

	assert(to);
	assert(pstreams_msgsize(from) >= bytestocopy);
	assert(pstreams_unwritbytes(to) >= bytestocopy);
	
	while(bytestocopy && from)
	{
		msgchunk = pstreams_msg1size(from);

		bitesize = MIN(msgchunk, bytestocopy);

		if(pstreams_unwrit1bytes(to) == 0)
		{
			assert(to->b_cont);
			to = to->b_cont;
		}

		pstreams_msg1copy(to, from, bitesize);

		bytestocopy -= bitesize;

		from = from->b_cont;/*go to continuation*/
	}

	return (bytestocopy == 0 ? P_STREAMS_SUCCESS : P_STREAMS_FAILURE);
}

/******************************************************************************
Name: pstreams_allocmsgb_copy
Purpose: this is like the copy constructor for MSGB
Parameters:
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_allocmsgb_copy(P_STREAMHEAD *strmhead, P_MSGB *initmsg)
{
	P_MSGB *msg=NULL;
	P_MSGB *msgiter=NULL;
	P_MSGB *msgiter_prev=NULL;
	uint size;

	while(initmsg)
	{
		size = pstreams_msg1size(initmsg);

		if(!msgiter)
		{
			msgiter = pstreams_allocmsgb(strmhead, size, 0);
			if(!msgiter)
			{
				/*TODO*/ assert(0);
			}
			if(msgiter_prev)
			{
				msgiter_prev->b_cont = msgiter;
			}
			else
			{
				/*this is the first time thru this loop*/
				msg = msgiter_prev = msgiter;
			}
		}
		
		pstreams_msg1copy(msgiter, initmsg, size);

		initmsg = initmsg->b_cont;
		msgiter_prev = msgiter;
		msgiter = msgiter->b_cont;
	}

	pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_allocmsgb_copy: bytes copied %d.",
			pstreams_msgsize(msg));

	return msg;
}

/******************************************************************************
Name: pstreams_garbagecollect
Purpose: delink and free empty message blocks
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_garbagecollect(P_QUEUE *q, P_MSGB **msg)
{
	P_MSGB *garbage = NULL;
	P_MSGB *garbageiter = NULL;
	P_MSGB *msgiter = NULL;

	/*step thru the list of (*msg)s by following (*msg)->b_cont
	 *and release empty ones
	 */
	if(!msg || !*msg)
	{
		return P_STREAMS_SUCCESS;
	}

	/*release initial garbage message blocks*/
	while(*msg && pstreams_msg1size(*msg) == 0)
	{
		if(!garbage) /*first time*/
		{
			garbageiter = garbage = *msg;
		}
		else
		{
			garbageiter->b_cont = *msg;
			garbageiter = garbageiter->b_cont;
		}

		*msg = (*msg)->b_cont;

		garbageiter->b_cont = NULL;
	}

	assert(!*msg || (pstreams_msg1size(*msg)>0));

	/*
	msgiter = (*msg ? (*msg)->b_cont : NULL);
	replaced with statement below - why skip one?*/
	msgiter = *msg;

	while(msgiter && msgiter->b_cont)
	{
		if(pstreams_msg1size(msgiter->b_cont) == 0)
		{
			garbageiter->b_cont = msgiter->b_cont;
			garbageiter = garbageiter->b_cont;
			msgiter->b_cont = msgiter->b_cont->b_cont;
		}

		msgiter = msgiter->b_cont;

		garbageiter->b_cont = NULL;
	}
		
	pstreams_log(q, PSTREAMS_LTDEBUG, "pstreams_garbagecollect: garbage bytes %d.",
			pstreams_msgsize(garbage));

	pstreams_relmsg(q, garbage);

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_addmsg
Purpose: add given tailmsg to msg as a continuation
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_addmsg(P_MSGB **msg, P_MSGB *tailmsg)
{
	P_MSGB *msgiter=NULL;

	if(!*msg)
	{
		*msg = tailmsg;
		return P_STREAMS_SUCCESS;
	}

	msgiter = *msg;
	while(msgiter)
	{
		if(!msgiter->b_cont)
		{
			msgiter->b_cont = tailmsg;
			break;
		}

		msgiter = msgiter->b_cont;
	}

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_sift
Purpose:  2-way sifter
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_sift(P_QUEUE *q, P_MSGB *msg, int (*sift)(P_QUEUE *q, P_MSGB *msg), 
		P_MSGB **msglist1, P_MSGB **msglist2) 
{
	int siftval=0;
	P_MSGB *msg_next=NULL;

	for(msg_next=msg->b_cont;
		msg;
		msg = msg_next)
	{
		msg_next = msg->b_cont;
		msg->b_cont = NULL; /*delink current msg*/

		switch(siftval=sift(q, msg))
		{
		case 1:
			pstreams_addmsg(msglist1, msg);
			break;
		case 2:
			pstreams_addmsg(msglist2, msg);
			break;
		default:
			pstreams_log(q, PSTREAMS_LTDEBUG, "pstreams_sift: sift() returned %d", siftval);
			return P_STREAMS_FAILURE;
		}
	}

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: 
Purpose:  express lane for handling control messages
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_ctlexpress(P_QUEUE *q, P_MSGB *msg, P_BOOL (*myioctl)(P_QUEUE *, P_MSGB *),
					P_MSGB **ctlmsgs, P_MSGB **datmsgs)
{
	int siftval=0;
	P_MSGB *msg_next=NULL;

	for(msg_next=msg->b_cont;
		msg;
		msg = msg_next)
	{
		msg_next = msg->b_cont;
		msg->b_cont = NULL; /*delink current msg*/

		switch(msg->b_datap->db_type)
		{
		case P_M_DATA:
			pstreams_addmsg(datmsgs, msg);
			break;

		case P_M_IOCTL:
		case P_M_DELIM:
			if(myioctl(q, msg))
			{
				pstreams_addmsg(ctlmsgs, msg);
			}
			else
			{
				pstreams_putnext(q, msg);
			}
			break;

		default:
			/*TODO - handle*/
			assert(0);
		}

	}

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_log
Purpose: 
Parameters: takes variable arguments
Caveats: 
******************************************************************************/
int
pstreams_log(P_QUEUE *q, P_LTCODE ltcode, const char *fmt,...)
{
	va_list ap;
	char *modulename="STRMHEAD";
	int q_count=-1;
	FILE *ltfile=NULL;
	P_STREAMHEAD *strmhead=NULL;
	int msg_count=-1;

	if(!pstreams_ltfilter(q, ltcode))
	{
		return P_STREAMS_SUCCESS;
	}

	if(strmhead = pstreams_get_strmhead(q))
	{
		ltfile = strmhead->ltfile;
	}
	else
	{
		ltfile = stderr;
	}

	assert(ltfile);

	if(q)
	{
		modulename = q->q_qinfo.qi_minfo->mi_idname;
		q_count = q->q_count;
		msg_count = pstreams_countmsg(q->q_msglist);
	}

	va_start(ap, fmt);

	if(ltcode >= PSTREAMS_LTERROR-1)
	{
		fprintf(ltfile, 
			"\nERROR %s qcount=%d/%d ", modulename, q_count, msg_count);
	}
	else if(ltcode >= PSTREAMS_LTWARNING-1)
	{
		fprintf(ltfile, 
			"\nW %s qcount=%d/%d ",modulename, q_count, msg_count);
	}
	else if(ltcode >= PSTREAMS_LTINFO-1)
	{
		fprintf(ltfile, 
			"\nI %s qcount=%d/%d ",modulename, q_count, msg_count);
	}
	else if(ltcode >= PSTREAMS_LTDEBUG-1)
	{
		fprintf(ltfile, 
			"\nD %s qcount=%d/%d ",modulename, q_count, msg_count);
	}

	vfprintf(ltfile, 
			fmt, ap);

	fprintf(ltfile, "\n");

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_ltfilter
Purpose: used mainly by pstreams_log to check of given ltcode is turned on for
	message logging and tracing
Parameters:
Caveats: 
******************************************************************************/
P_BOOL
pstreams_ltfilter(P_QUEUE *q, P_LTCODE ltcode)
{
	if(!q)
	{
		return P_FALSE;
	}

	return (ltcode >= q->ltfilter ? P_TRUE : P_FALSE);
}

/******************************************************************************
Name: pstreams_countmsgcont
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_countmsgcont(P_MSGB *msg)
{
	if(!msg)
	{
		return 0;
	}

	return 1 + pstreams_countmsgcont(msg->b_cont);
}

/******************************************************************************
Name: pstreams_countmsg
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_countmsg(LISTHDR *msglist)
{
	P_MSGB *msgiter=NULL;
	int count=0;
	
	for(msgiter=(P_MSGB *)lop_getnext(msglist, NULL);
	    msgiter!=NULL;
	    msgiter=(P_MSGB *)lop_getnext(msglist, msgiter))
	{
		count += pstreams_countmsgcont(msgiter);
	}

	return count;
}

/******************************************************************************
Name: pstreams_mchk
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_mchk(P_QUEUE *q, P_GETVAL *get)
{
	int msgcount=0;

	assert(q);
	assert(get);

	assert(get->type == GETMSGCNT);

	msgcount += pstreams_countmsg(q->q_msglist);
	msgcount += pstreams_countmsg(q->q_peer->q_msglist);

	get->val.msgcount += msgcount;

	return msgcount;
}

/******************************************************************************
Name: pstreams_checkmem
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_checkmem(P_STREAMHEAD *strmhead)
{
	P_QUEUE *dq; /*downstream queues*/
	P_GETVAL get={GETMSGCNT, {0}};
	int mod_msgcount=0;
	int lop_msgcount=0;

	pstreams_log(&strmhead->apprdq, PSTREAMS_LTDEBUG, "pstreams_checkmem:");

	/*step thru downstream queues calling srvp() on each*/
	for(dq=&strmhead->appwrq;
	    dq;
	    dq = dq->q_next)
	{
		get.val.msgcount = 0; 
		if(dq->q_qinfo.qi_mchk)
		{
			dq->q_qinfo.qi_mchk(dq, &get);
		}
		else
		{
			/*default*/
			pstreams_mchk(dq, &get);
		}
	
		mod_msgcount += get.val.msgcount;

		pstreams_log(dq, (P_LTCODE)(PSTREAMS_LTDEBUG-1), "\tmodule says MBLKs in use=%d",
			get.val.msgcount);
	}

	pstreams_log(&strmhead->apprdq, (P_LTCODE)(PSTREAMS_LTDEBUG-1),
					"\t\tmodules say Sum of MBLKs in use=%d", mod_msgcount);

	lop_msgcount = strmhead->msgpool->count - strmhead->msgpool->freecount;

	pstreams_log(&strmhead->apprdq, (P_LTCODE)(PSTREAMS_LTDEBUG-1), 
				"\t\tLISTOP says Sum of MBLKs in use=%d(total=%d,free=%d)",
					lop_msgcount, strmhead->msgpool->count, strmhead->msgpool->freecount );

	if(lop_msgcount != mod_msgcount)
	{
		pstreams_log(&strmhead->apprdq, (P_LTCODE)(PSTREAMS_LTWARNING+1), "MBLK COUNTS MISMATCH - NOTOK");

		exit(0);
	}
	else
	{
		pstreams_log(&strmhead->apprdq, (P_LTCODE)(PSTREAMS_LTDEBUG-1), "MBLK COUNTS MATCH - OK");
	}

	return P_STREAMS_SUCCESS;
}

void
bintohex(uchar *hexbuf, uchar *binbuf, uint32_t len)
{
	static char hexcodes[]="0123456789ABCDEF";  /* Hex to binary table */
	uint32_t i;                                                                        
	for(i = 0; i < len; i++ )
	{                                                                           
		*hexbuf++ = hexcodes[(*binbuf >> 4) & 15];
		*hexbuf++ = hexcodes[ *binbuf++ & 15 ];                           
	}
	return;
}
/*END pstreams.c*/
