/*===========================================================================
FILE: stdmod.c

   Portable and Pure streams. Routines implementing a 'unix streams' inspired
design for layered protocols. 
   Default implementation for the stream's head and tail as modules. While
the stream head is always the stdapp module, the tail could be any module
varying with the actual device

 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/
#include "stdio.h"
#include "pstreams.h"
#include "stdmod.h"

P_QINIT stdapp_wrinit={0};
P_QINIT stdapp_rdinit={0};
P_QINIT stddev_wrinit={0};
P_QINIT stddev_rdinit={0};
P_STREAMTAB stdapp_streamtab={0};
P_STREAMTAB stddev_streamtab={0};
P_MODINFO stdappmodinfo={0};
P_MODINFO stddevmodinfo={0};

/*default stream head routines*/
int
stdapp_wput(P_QUEUE *q, P_MSGB *msg)
{
	/*process*/

	/*done processing send it on*/
	if(pstreams_canput(q->q_next))
	{
		pstreams_putnext(q, msg);
	}
	else
	{
		if(pstreams_canput(q))
		{
			pstreams_putq(q, msg);
		}
		else
		{
			return P_STREAMS_FAILURE;
		}
	}

	return P_STREAMS_SUCCESS;
}

int
stdapp_rput(P_QUEUE *q, P_MSGB *msg)
{
	/*process*/

	/*end module - don't send it on - put in my queue for getmsg() to find*/
	
	if(pstreams_msgsize(msg) == 0)
	{
		pstreams_relmsg(q, msg);
	}
	else if(pstreams_canput(q))
	{
		pstreams_putq(q, msg);
	}
	else
	{
		return P_STREAMS_FAILURE;
	}

	return P_STREAMS_SUCCESS;
}

	
int
stddev_wput(P_QUEUE *q, P_MSGB *msg)
{
	/*end module - don't send it on - just print it out*/
	pstreams_relmsg(q, msg);

	return 0;
}
	
int
stddev_rput(P_QUEUE *q, P_MSGB *msg)
{
	/*process*/
	/*done processing send it on*/
	if(pstreams_canput(q->q_next))
	{
		pstreams_putnext(q, msg);
	}
	else
	{
		pstreams_putq(q, msg);
	}

	return P_STREAMS_SUCCESS;
}

int
stdapp_init()
{
	/*first initialize stdappmodinfo*/
	stdappmodinfo.mi_idnum = 1;
	stdappmodinfo.mi_idname = "STDAPP_RW";
	stdappmodinfo.mi_minpsz = 0;
	stdappmodinfo.mi_maxpsz = 100;
	stdappmodinfo.mi_hiwat = 1024;
	stdappmodinfo.mi_lowat = 256;


	/*init stdapp_streamtab*/
#ifdef M2STRICTTYPES
	stdapp_wrinit.qi_qopen = stdapp_open;
	stdapp_wrinit.qi_putp = stdapp_wput;
	stdapp_rdinit.qi_putp = stdapp_rput;
	stdapp_rdinit.qi_qopen = stdapp_open;
#else
	stdapp_wrinit.qi_qopen = (int (*)())stdapp_open;
	stdapp_wrinit.qi_putp = (int (*)())stdapp_wput;
	stdapp_rdinit.qi_putp = (int (*)())stdapp_rput;
	stdapp_rdinit.qi_qopen = (int (*)())stdapp_open;
#endif

	stdapp_wrinit.qi_minfo = &stdappmodinfo;
	stdapp_rdinit.qi_minfo = &stdappmodinfo;

	stdapp_streamtab.st_wrinit = &stdapp_wrinit;
	stdapp_streamtab.st_rdinit = &stdapp_rdinit;

	return 0;
}
	
int
stdapp_open(P_QUEUE *q)
{
	q->ltfilter = PSTREAMS_LTDEBUG;

	return P_STREAMS_SUCCESS;
}

int
stddev_init()
{
	/*first initialize stddevmodinfo*/
	stddevmodinfo.mi_idnum = 2;
	stddevmodinfo.mi_idname = "STDDEV_RW";
	stddevmodinfo.mi_minpsz = 0;
	stddevmodinfo.mi_maxpsz = 100;
	stddevmodinfo.mi_hiwat = 1024;
	stddevmodinfo.mi_lowat = 256;

	/*init stddev_streamtab*/
#ifdef M2STRICTTYPES
	stddev_wrinit.qi_putp = stddev_wput;
	stddev_rdinit.qi_putp = stddev_rput;
#else
	stddev_wrinit.qi_putp = (int (*)())stddev_wput;
	stddev_rdinit.qi_putp = (int (*)())stddev_rput;
#endif

	stddev_wrinit.qi_minfo = &stddevmodinfo;
	stddev_rdinit.qi_minfo = &stddevmodinfo;

	stddev_streamtab.st_wrinit = &stddev_wrinit;
	stddev_streamtab.st_rdinit = &stddev_rdinit;

	return 0;
}
