/*===========================================================================
FILE: pstreams_echo.c

Description: pstreams echo module. Loopback functionality.
			All messages sent to this module are echoed back.

Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "listop.h"
#include "pstreams.h"
#include "pstreams_echo.h"

P_QINIT echo_wrinit={0};
P_QINIT echo_rdinit={0};
P_STREAMTAB echo_streamtab={0};
P_MODINFO echo_wrmodinfo={0};
P_MODINFO echo_rdmodinfo={0};

/******************************************************************************
Name: echo_init
Purpose: initialises echo module. This is to be called before pushing this module
	into the streamhead
Parameters:
Caveats:
******************************************************************************/
int
echo_init()
{
	/*first initialize echomodinfo*/
	echo_wrmodinfo.mi_idnum = 1;
	echo_wrmodinfo.mi_idname = "LOOPBACK WR";
	echo_wrmodinfo.mi_minpsz = 0;
	echo_wrmodinfo.mi_maxpsz = 128;
	echo_wrmodinfo.mi_hiwat = 1024;
	echo_wrmodinfo.mi_lowat = 256;

	echo_rdmodinfo.mi_idnum = 1;
	echo_rdmodinfo.mi_idname = "LOOPBACK_RD";
	echo_rdmodinfo.mi_minpsz = 0;
	echo_rdmodinfo.mi_maxpsz = 128;
	echo_rdmodinfo.mi_hiwat = 1024;
	echo_rdmodinfo.mi_lowat = 256;

	/*init echo_streamtab*/
#ifdef M2STRICTTYPES
	echo_wrinit.qi_qopen = echo_open;
	echo_wrinit.qi_putp = echo_wput;
	echo_wrinit.qi_srvp = echo_wsrvp;
	echo_rdinit.qi_qopen = echo_open;
	echo_rdinit.qi_putp = echo_rput;
	echo_rdinit.qi_srvp = echo_rsrvp;
#else
	echo_wrinit.qi_qopen = (int (*)())echo_open;
	echo_wrinit.qi_putp = (int (*)())echo_wput;
	echo_wrinit.qi_srvp = (int (*)())echo_wsrvp;
	echo_rdinit.qi_qopen = (int (*)())echo_open;
	echo_rdinit.qi_putp = (int (*)())echo_rput;
	echo_rdinit.qi_srvp = (int (*)())echo_rsrvp;
#endif

	echo_wrinit.qi_minfo = &echo_wrmodinfo;
	echo_rdinit.qi_minfo = &echo_rdmodinfo;

	echo_streamtab.st_wrinit = &echo_wrinit;
	echo_streamtab.st_rdinit = &echo_rdinit;

	return 0;
}

/******************************************************************************
Name: echo_open
Purpose: queue initialization. q_peer pointers are set.
Parameters:
Caveats:
******************************************************************************/
int
echo_open(P_QUEUE *q)
{
	if(q->q_peer && q->q_peer->q_ptr)
	{
		q->q_ptr = q->q_peer->q_ptr;
	}
	else
	{
		q->q_ptr = NULL;
	}

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: echo_wput
Purpose: write procedure for the write queue
Parameters:
Caveats:
******************************************************************************/
int
echo_wput(P_QUEUE *wq, P_MSGB *msg)
{

	pstreams_putq(wq, msg); /*put it in my queue*/

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: echo_wsrvp
Purpose: service procedure for the write queue
Parameters:
Caveats:
******************************************************************************/
int
echo_wsrvp(P_QUEUE *wq)
{
	P_MSGB *msg;

	while(msg = pstreams_getq(wq))
	{
		if(!pstreams_canput(wq->q_peer))
		{
			pstreams_putbq(wq, msg); /*put back in my queue*/
			break;
		}
			
		pstreams_putq(wq->q_peer, msg);

	}

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: echo_rsrvp
Purpose: service procedure for the read queue
Parameters:
Caveats:
******************************************************************************/
int
echo_rsrvp(P_QUEUE *rq)
{
	P_MSGB *msg;
	
	while(msg = pstreams_getq(rq))
	{
		if(!pstreams_canput(rq->q_next))
		{
			pstreams_putbq(rq, msg); /*put back in my queue*/
			break;
		}
			
		pstreams_putnext(rq, msg);
	}

	return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: echo_rput
Purpose: put() procedure for the read queue
Parameters:
Caveats:
******************************************************************************/	
int
echo_rput(P_QUEUE *q, P_MSGB *msg)
{
	/*never called*/

	assert(0);
	
	return P_STREAMS_SUCCESS;
}
