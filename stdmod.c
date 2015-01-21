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
#include <stdio.h>
#include "options.h"
#include "env.h"
#include "pstreams.h"
#include "stdmod.h"

#if(1)
/*disables function entry-exit trace log*/
#undef PDBGTRACE
#define PDBGTRACE(x) ((void)(x))
#endif

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
    PDBGTRACE("stdapp_wput: entered");

#ifdef PSTREAMS_LT
    pstreams_log(q, (P_LTCODE)(PSTREAMS_LTINFO-1), "wput: entered msg %d bytes",
        pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/

    /*process*/

    /*
     * done processing ... 
     * put in my queue first - this acts as a buffer, 
     * and a flow-control mechanism, in case module downstream
     * doesn't
     */
    if(msg->b_band == 0)
    {
        pstreams_putq(q, msg);
    }
    else
    {
        pstreams_putnext(q, msg);
    }

    PDBGTRACE("stdapp_wput: exit");

    return P_STREAMS_SUCCESS;
}

int
stdapp_rput(P_QUEUE *q, P_MSGB *msg)
{
    PDBGTRACE("stdapp_rput: entered");

    /*process*/

    /*end module - don't send it on - put in my queue for getmsg() to find*/
    
    if(pstreams_msgsize(msg) == 0)
    {
        pstreams_freemsg(PSTRMHEAD(q), msg);
    }
    else if(msg->b_datap->db_type == P_M_ERROR)
    {
        /*set perrno to first data byte of message - as in man page*/
        PSTRMHEAD(q)->perrno = (int)msg->b_rptr[0];
        pstreams_freemsg(PSTRMHEAD(q), msg);
    }
    else
    {
        pstreams_putq(q, msg);
    }

    PDBGTRACE("stdapp_rput: exit");

    return P_STREAMS_SUCCESS;
}

int
stddev_wput(P_QUEUE *q, P_MSGB *msg)
{
    PDBGTRACE("stddev_wput: entered");

    /*end module - don't send it on - just print it out*/
    pstreams_freemsg(PSTRMHEAD(q), msg);

    PDBGTRACE("stddev_wput: exit");

    return 0;
}
    
int
stddev_rput(P_QUEUE *q, P_MSGB *msg)
{
    PDBGTRACE("stddev_rput: entered");

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

    PDBGTRACE("stddev_rput: exit");

    return P_STREAMS_SUCCESS;
}

int
stdapp_init()
{
    PDBGTRACE("stdapp_init: entered");

    /*first initialize stdappmodinfo*/
    stdappmodinfo.mi_idnum = 1;
    stdappmodinfo.mi_idname = "STDAPP_RW";
    stdappmodinfo.mi_minpsz = 0;
    stdappmodinfo.mi_maxpsz = 100;
    stdappmodinfo.mi_hiwat = 128;
    stdappmodinfo.mi_lowat = 128;


    /*init stdapp_streamtab*/
#ifdef PSTREAMS_STRICTTYPES
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

    PDBGTRACE("stdapp_init: exit");

    return 0;
}
    
int
stdapp_open(P_QUEUE *q)
{
    PDBGTRACE("stdapp_open: entered");

    /*Default log trace filter value*/
    q->ltfilter = PSTREAMS_LT8; /*LTWARNING and up*/

    PDBGTRACE("stdapp_open: exit");

    return P_STREAMS_SUCCESS;
}

int
stddev_init()
{
    PDBGTRACE("stddev_init: entered");

    /*first initialize stddevmodinfo*/
    stddevmodinfo.mi_idnum = 2;
    stddevmodinfo.mi_idname = "STDDEV_RW";
    stddevmodinfo.mi_minpsz = 0;
    stddevmodinfo.mi_maxpsz = 100;
    stddevmodinfo.mi_hiwat = 1024;
    stddevmodinfo.mi_lowat = 256;

    /*init stddev_streamtab*/
#ifdef M2STRICTTYPES
    stddev_wrinit.qi_qopen = NULL;
    stddev_wrinit.qi_putp = stddev_wput;
    stddev_rdinit.qi_putp = stddev_rput;
    stddev_rdinit.qi_qopen = NULL;
#else
    stddev_wrinit.qi_qopen = (int (*)())NULL;
    stddev_wrinit.qi_putp = (int (*)())stddev_wput;
    stddev_rdinit.qi_putp = (int (*)())stddev_rput;
    stddev_rdinit.qi_qopen = (int (*)())NULL;
#endif

    stddev_wrinit.qi_minfo = &stddevmodinfo;
    stddev_rdinit.qi_minfo = &stddevmodinfo;

    stddev_streamtab.st_wrinit = &stddev_wrinit;
    stddev_streamtab.st_rdinit = &stddev_rdinit;

    PDBGTRACE("stddev_init: entered");

    return 0;
}

