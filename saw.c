/*===========================================================================
FILE: saw.c

Description: SAW - Stop-And-Wait protocol implementation module.
    A partial implementation of the SAW protocol - it doesn't retransmit
    and thus avoids the use of timers. It also doesn't attempt to piggyback
    acknowledgements.

Activity:
 Date         Author           Comments
 Oct.09,2003  tgeorge          Created.

===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include "options.h"
#include "env.h"
#include "assert.h"
#include "listop.h"
#include "pstreams.h"
#include "saw.h"

#define PSTREAMS_LT

/*
 * Declare memory for SAW, as required by PSTREAMS framework
 */
P_STREAMTAB saw_streamtab={0}; /*SAW module*/
P_MODINFO saw_wrmodinfo={0}; /*SAW module info for writer*/
P_MODINFO saw_rdmodinfo={0}; /*SAW module infor for reader*/
P_QINIT saw_wrinit={0}; /*SAW reader queue*/
P_QINIT saw_rdinit={0}; /*SAW writer queue*/

/******************************************************************************
Name: saw_init
Purpose: initialises echo module. This is to be called before pushing this module
    into the streamhead
Parameters:
Caveats:
******************************************************************************/
int
saw_init()
{
    /*first initialize SAW modinfo structures*/
    saw_wrmodinfo.mi_idnum = 10;
    saw_wrmodinfo.mi_idname = "SAW WR";
    saw_wrmodinfo.mi_minpsz = 0;
    saw_wrmodinfo.mi_maxpsz = 128;
    saw_wrmodinfo.mi_hiwat = 64; //flow-control cut-off
    saw_wrmodinfo.mi_lowat = 32;

    saw_rdmodinfo.mi_idnum = 10;
    saw_rdmodinfo.mi_idname = "SAW RD";
    saw_rdmodinfo.mi_minpsz = 0;
    saw_rdmodinfo.mi_maxpsz = 128;
    saw_rdmodinfo.mi_hiwat = 1024; //flow-control cut-off
    saw_rdmodinfo.mi_lowat = 256;

    /*init saw_streamtab*/
#ifdef M2STRICTTYPES
    saw_wrinit.qi_qopen = saw_open;
    saw_wrinit.qi_putp = saw_wput;
    saw_wrinit.qi_srvp = saw_wsrvp;
    saw_rdinit.qi_qopen = saw_open;
    saw_rdinit.qi_putp = saw_rput;
    saw_rdinit.qi_srvp = saw_rsrvp;
#else
    saw_wrinit.qi_qopen = (int (*)())saw_open;
    saw_wrinit.qi_putp = (int (*)())saw_wput;
    saw_wrinit.qi_srvp = (int (*)())saw_wsrvp;
    saw_rdinit.qi_qopen = (int (*)())saw_open;
    saw_rdinit.qi_putp = (int (*)())saw_rput;
    saw_rdinit.qi_srvp = (int (*)())saw_rsrvp;
#endif

    saw_wrinit.qi_minfo = &saw_wrmodinfo;
    saw_rdinit.qi_minfo = &saw_rdmodinfo;

    saw_streamtab.st_wrinit = &saw_wrinit;
    saw_streamtab.st_rdinit = &saw_rdinit;

    return 0;
}

/******************************************************************************
Name: saw_open
Purpose: queue initialization. q_peer pointers are set.
Parameters:
Caveats:
******************************************************************************/
int
saw_open(P_QUEUE *q)
{
    if(q->q_peer && q->q_peer->q_ptr)
    {
        q->q_ptr = q->q_peer->q_ptr;
    }
    else
    {
        SAWAREA *sawArea = saw_getarea(q);
        if(!sawArea)
        {
            PSTRMHEAD(q)->perrno = P_OUTOFMEMORY;
            return P_STREAMS_FAILURE;
        }

        sawArea->SeqNo=0;
        sawArea->AckNo=0;
        sawArea->AckWaitTimer = 0;
        sawArea->SendAckTimer = 0;

        q->q_ptr = sawArea;
    }

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: saw_wput
Purpose: put procedure for the write queue. Just put msg in my queue.
Parameters:
Caveats:
******************************************************************************/
int
saw_wput(P_QUEUE *wq, P_MSGB *msg)
{
    P_MSGB *ctlmsg=NULL;
    P_MSGB *datmsg=NULL;
    P_MSGB *new_datmsg=NULL;
    P_STREAMHEAD *strm = PSTRMHEAD(wq);

    pstreams_ctlexpress(wq, msg, saw_myctl, &ctlmsg, &datmsg);
    msg=NULL;

    if(ctlmsg)
    {
        /*
         * No control messages for me, so 
         * pass it on
         */
        pstreams_linkb(ctlmsg, datmsg);
        pstreams_putnext(wq, ctlmsg);
        return P_STREAMS_SUCCESS;
    }

    pstreams_putq(wq, datmsg); /*data-only messages alone get in my queue*/

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: saw_wsrvp
Purpose: Service procedure for the write queue. Transmit-side logic is
        encapsulated here.
Parameters:
Caveats:
******************************************************************************/
int
saw_wsrvp(P_QUEUE *wq)
{
    P_MSGB *msg=NULL;
    SAWAREA *sawArea = (SAWAREA *)wq->q_ptr;
    boolean transmitNow = P_FALSE;

    //Are we Idling now?
    if(sawArea->AckWaitTimer == 0)
    {
        //if yes, then get a fresh message if available
        if(msg = pstreams_getq(wq))
        {
            transmitNow = P_TRUE;
        }
    }

    //Are we waiting for an Ack?
    if(sawArea->AckWaitTimer > 0)
    {
        //has the wait time expired?
        if(my_time() > sawArea->AckWaitTimer)
        {
            //let's retransmit if we have not exceeded limits...
            if(sawArea->CurrentReTxCount > sawArea->MaxReTXCount)
            {
                transmitNow = P_TRUE;
            }
            else
            {
                //... else, abort
                saw_abort();
            }
        }
    }

    //Do we have transmit pending?
    if(transmitNow)
    {
        ASSERT(msg);
        if(pstreams_canput(wq->q_next))
        {
            P_MSGB *sawHdrMsg = saw_gethdr(wq);

            if(!sawHdrMsg)
            {
                //TODO:
                ASSERT(sawHdrMsg);
            }
            sawHdrMsg->b_cont = msg;
            pstreams_putnext(wq, sawHdrMsg);

            sawArea->SendAckTimer = 0;
            sawArea->AckWaitTimer = my_time() + sawArea->AckWaitTimeout;
        }
    }

    if(sawArea->SendAckTimer > 0)
    {
        if(my_time() > sawArea->SendAckTimer)
        {
            P_MSGB *sawHdrMsg = saw_gethdr(wq);

            if(!sawHdrMsg)
            {
                //TODO:
                ASSERT(sawHdrMsg);
            }
            sawHdrMsg->b_cont = msg;
            pstreams_putnext(wq, sawHdrMsg);

            sawArea->SendAckTimer = 0;
        }
    }

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: saw_rsrvp
Purpose: service procedure for the read queue
Parameters:
Caveats:
******************************************************************************/
int
saw_rsrvp(P_QUEUE *rq)
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
Name: saw_rput
Purpose: put() procedure for the read queue. Receive-side logic is encapsulated
        here.
Parameters:
Caveats:
******************************************************************************/    
int
saw_rput(P_QUEUE *q, P_MSGB *msg)
{
    SAWAREA *sawArea = (SAWAREA *)q->q_ptr;
    SAWHDR *hdr = NULL;

    ASSERT(msg);

    if(pstreams_msgsize(msg) >= sizeof(SAWHDR))
    {
        hdr = (SAWHDR *)msg->b_rptr;
        pstreams_msgconsume(msg, sizeof(SAWHDR));
    }
    else
    {
        //TODO:
        return P_STREAMS_SUCCESS;
    }

    if((hdr->SeqNo == 0) ||
        (hdr->AckNo == 0))
    {
        //peer had a reset or I had a reset, respectively...
        //...accept this by re-sync'ing the RX side
        sawArea->AckNo = hdr->SeqNo;
    }

    //Are we expecting an ACK? And, is this such an ACK?
    if(sawArea->AckWaitTimer > 0)
    {
        /*
         * Note: below, '==' instead of '>' because of rollover
         */
        if(hdr->AckNo == ((sawArea->SeqNo %255) +1))
        {
            sawArea->SeqNo = hdr->AckNo;
            pstreams_log(q, PSTREAMS_LT6, "Advanced SeqNo: SeqNo=%d, AckNo=%d",
                sawArea->SeqNo, sawArea->AckNo);
            sawArea->AckWaitTimer = 0;
        }
    }

    /*
     * Is this a fresh message?
     */
    if(sawArea->AckNo == hdr->SeqNo)
    {
        /*we have received a fresh message*/

        /*increment AckNo - but in range 1 to 255*/
        sawArea->AckNo = (sawArea->AckNo % 255) +1;

        pstreams_log(q, PSTREAMS_LT6, "Advanced AckNo: SeqNo=%d, AckNo=%d",
                sawArea->SeqNo, sawArea->AckNo);

        sawArea->SendAckTimer =
            my_time() + sawArea->SendAckTimeout;
        if(pstreams_msgsize(msg) > 0)
        {
            pstreams_putq(q, msg);
        }
        else
        {
            /*
             *uhmm...why would peer send us an empty
             *message (not ACK)?
             */
        }
    }
    else /*not a fresh message, i.e., didn't increase our AckNo*/
    {
        if(pstreams_msgsize(msg) > 0)
        {
            //TODO: drop msg
            return P_STREAMS_SUCCESS;
        }
    }

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: saw_myctl
Purpose: discriminant - determines if msg is a ctl msg for this module
Parameters:
Caveats:
******************************************************************************/
P_BOOL
saw_myctl(P_QUEUE *q, P_MSGB *msg)
{
    PDBG(q=NULL);/*keep compiler happy*/
    PDBG(msg=NULL);/*keep compiler happy*/

    return P_TRUE;/*all control messages are consumed here*/
}

SAWAREA *
saw_getarea(P_QUEUE *q)
{
    SAWAREA *sawArea = NULL;

    sawArea = (SAWAREA *)pstreams_memassign(PSTRMHEAD(q)->mem, sizeof(SAWAREA));
    if(!sawArea)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
        "buffer size: %d. SAWAREA requires: %d+memory for alignment",
        PSTRMHEAD(q)->mem->limit-PSTRMHEAD(q)->mem->base, sizeof(SAWAREA));
        return NULL;
    }

    memset(sawArea, 0, sizeof(*sawArea));
    sawArea->MaxReTXCount = 1;
    sawArea->AckWaitTimeout = 2000;
    sawArea->SendAckTimeout = 0;

    return sawArea;
}

P_MSGB *
saw_gethdr(P_QUEUE *q)
{
    SAWAREA *sawArea = (SAWAREA *)q->q_ptr;
    P_MSGB *hdrmsg = NULL;
    SAWHDR *hdr = NULL;

    hdrmsg = pstreams_allocb((P_STREAMHEAD *)q->strmhead, sizeof(SAWHDR), 0);
    if(!hdrmsg)
    {
        PSTRMHEAD(q)->perrno = P_OUTOFMEMORY;
        return NULL;
    }

    hdrmsg->b_datap->db_type = P_M_DATA;

    /*fill header fields*/
    hdr = (SAWHDR *)hdrmsg->b_wptr;
    fieldassign(&hdr->SeqNo, sawArea->SeqNo, 1);
    fieldassign(&hdr->AckNo, sawArea->AckNo, 1);

    hdrmsg->b_wptr += sizeof(SAWHDR);

    return hdrmsg;
}

void
saw_abort()
{
    /*TODO:*/
}
