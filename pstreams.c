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
#include <stdlib.h>
#include <stdarg.h> /*ansi C style variable args*/
#include "options.h"
#include "env.h"
#include "assert.h"
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
extern P_STREAMTAB stdapp_streamtab; /*module interfacing to application*/
#ifdef PSTREAMS_UDP
extern P_STREAMTAB udpdev_streamtab; /*module interfacing to UDP device*/
#endif
#ifdef PSTREAMS_TCP
extern P_STREAMTAB tcpdev_streamtab; /*module interfacing to TCP device*/
#endif

/*
 * Global P_FREE_RTNs ! - until P_STREAMHEAD gets a pool of these
 * These are used as P_FREE_RTNs but do not use free_arg portion, as they
 * are global and can be overwritten
 */
#ifdef PSTREAMS_H8
const P_FREE_RTN FREEDATA = {pstreams_mem_free, NULL};
#else
const P_FREE_RTN FREEDATA = {(void (*)(void))pstreams_mem_free, NULL};
#endif

/*DEBUG mode*/
PDBG(P_STREAMHEAD *dbgstrm=NULL;)

/******************************************************************************
Name: pstreams_open
Purpose: creates and returns a streamhead representing a direct connection to
    the given device.
Parameters:
    in: devid is a enum of type P_STREAMS_DEVID for the device to connect to
    in: mem - memory for use within PSTREAMS
    in: pmem -persistent memory for use within PSTREAMS. Memory
        contents persist across PSTREAMS creations. Could be memory-mapped
        and shared across processes too.
  Caveats: memory allocated here
******************************************************************************/
P_STREAMHEAD *
pstreams_open(int devid, P_MEM *mem, P_MEM *pmem)
{
    P_STREAMHEAD *strmhead=NULL;
    void *mptr=NULL;

    /*get memory for streamhead*/
    if(!mem || mem->buf == NULL)
    {
        pstreams_console("ERROR: given buffer for local memory is empty");
    }

    pstreams_console("pstreams_open: memory at start. vmem avail = %lu bytes. pmem avail = %lu bytes.\r\n" 
            " vmem=(0x%lX, 0x%lX). pmem=(0x%lX, 0x%lX)",
        (unsigned long)(mem->limit-mem->base),
        (unsigned long)(pmem->limit-pmem->base),
        mem->base, mem->limit,
        pmem->base, pmem->limit);

    /*TODO : validate mem and pmem*/

    if((mem->limit - (char *)WORDALIGN(mem->base)) < sizeof(P_STREAMHEAD))
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. Required atleast: %d+memory for alignment",
            mem->limit-mem->base, sizeof(P_STREAMHEAD));
        return NULL;
    }

    strmhead = (P_STREAMHEAD *)pstreams_memassign(mem, sizeof(P_STREAMHEAD));
    ASSERT(strmhead);

    /*debug mode*/
    PDBG(dbgstrm = strmhead);
        
    strmhead->mem = mem;
    strmhead->pmem = pmem;

    /*set log trace file as soon as possible*/
    strmhead->ltfname[0] = '\0'; /*set log/trace file name to NULL*/

    strmhead->ltfile=stderr;

    strmhead->perrno = P_NOERROR; /*no errors at start*/

    switch(devid)
    {
        case P_NULL:
            /*The NULL device, this device drops all messages to it*/
            stddev_init(); /*for now stddev is a NULL device*/
            strmhead->devmod = stddev_streamtab;/*structure copy*/
            break;
#ifdef PSTREAMS_UDP
        case P_UDP:
            /*UDP device*/
            udpdev_init();
            strmhead->devmod = udpdev_streamtab;/*structure copy*/
            break;
#endif
#ifdef PSTREAMS_TCP
        case P_TCP:
            /*TCP device*/
            tcpdev_init();
            strmhead->devmod = tcpdev_streamtab;/*structure copy*/
            break;
#endif
        default:
            pstreams_console("pstreams_open: Unknown device id : %d\n", devid);
            free(strmhead);
            return NULL;
    }

    strmhead->devid    = (P_STREAMS_DEVID) devid;

    stdapp_init();
    strmhead->appmod = stdapp_streamtab;/*structure copy*/

    /*the top and bottom modules(appmod and devmod resp.) have been created*/

    /*allocate memory pools for 'queue's to be used when modules are pushed in*/
    mptr = pstreams_memassign(strmhead->mem, lop_getpoolsize(sizeof(P_QUEUE), MAXQUEUES));
    if(!mptr)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. P_QUEUEs require: %d+memory for alignment",
            mem->limit-mem->base, lop_getpoolsize(sizeof(P_QUEUE), MAXQUEUES));
        strmhead->perrno = P_OUTOFMEMORY;
        return NULL;
    }
    strmhead->qpool = lop_allocpool(sizeof(P_QUEUE), MAXQUEUES, mptr);

    /*allocate memory pools for passing messages internally, P_MSGBs and P_DATABs*/
    mptr = pstreams_memassign(strmhead->mem, lop_getpoolsize(sizeof(P_MSGB), MAXMSGBS));
    if(!mptr)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. P_MSGBs require: %d+memory for alignment",
            mem->limit-mem->base, lop_getpoolsize(sizeof(P_MSGB), MAXMSGBS));
        strmhead->perrno = P_OUTOFMEMORY;
        return NULL;
    }
    strmhead->msgpool = lop_allocpool(sizeof(P_MSGB), MAXMSGBS, mptr);

    mptr = pstreams_memassign(strmhead->mem, lop_getpoolsize(sizeof(P_DATAB), MAXDATABS));
    if(!mptr)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. P_DATABs require: %d+memory for alignment",
            mem->limit-mem->base, lop_getpoolsize(sizeof(P_DATAB), MAXDATABS));
        strmhead->perrno = P_OUTOFMEMORY;
        return NULL;
    }
    strmhead->datapool = lop_allocpool(sizeof(P_DATAB), MAXDATABS, mptr);

#if(POOL16SIZE > 0)
    mptr = pstreams_memassign(strmhead->mem, lop_getpoolsize(16, POOL16SIZE));
    if(!mptr)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. POOL16 requires: %d+memory for alignment",
            mem->limit-mem->base, lop_getpoolsize(16, POOL16SIZE));
        strmhead->perrno = P_OUTOFMEMORY;
        return NULL;
    }
    strmhead->pool16 = lop_allocpool(16, POOL16SIZE, mptr);
#endif

#if(POOL64SIZE > 0)
    mptr = pstreams_memassign(strmhead->mem, lop_getpoolsize(64, POOL64SIZE));
    if(!mptr)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. POOL64 requires: %d+memory for alignment",
            mem->limit-mem->base, lop_getpoolsize(64, POOL64SIZE));
        strmhead->perrno = P_OUTOFMEMORY;
        return NULL;
    }
    strmhead->pool64 = lop_allocpool(64, POOL64SIZE, mptr);
#endif

#if(POOL256SIZE > 0)
    mptr = pstreams_memassign(strmhead->mem, lop_getpoolsize(256, POOL256SIZE));
    if(!mptr)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. POOL256 requires: %d+memory for alignment",
            mem->limit-mem->base, lop_getpoolsize(256, POOL256SIZE));
        strmhead->perrno = P_OUTOFMEMORY;
        return NULL;
    }
    strmhead->pool256 = lop_allocpool(256, POOL256SIZE, mptr);
#endif

#if(POOL512SIZE > 0)
    mptr = pstreams_memassign(strmhead->mem, lop_getpoolsize(512, POOL512SIZE));
    if(!mptr)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. POOL512 requires: %d+memory for alignment",
            mem->limit-mem->base, lop_getpoolsize(512, POOL512SIZE));
        strmhead->perrno = P_OUTOFMEMORY;
        return NULL;
    }
    strmhead->pool512 = lop_allocpool(512, POOL512SIZE, mptr);
#endif

#if(POOL1792SIZE > 0)
    mptr = pstreams_memassign(strmhead->mem, lop_getpoolsize(1792, POOL1792SIZE));
    if(!mptr)
    {
        pstreams_console("ERROR: given buffer insufficient for local memory. "
            "buffer size: %d. POOL1792 requires: %d+memory for alignment",
            mem->limit-mem->base, lop_getpoolsize(1792, POOL1792SIZE));
        strmhead->perrno = P_OUTOFMEMORY;
        return NULL;
    }
    strmhead->pool1792 = lop_allocpool(1792, POOL1792SIZE, mptr);
#endif

    /*DEBUG messages*/
    pstreams_console("pstreams_open: memory at close. vmem avail = %lu bytes. "
        "pmem avail = %lu bytes\n",
        (unsigned long)(strmhead->mem->limit-strmhead->mem->base),
        (unsigned long)(strmhead->pmem->limit-strmhead->pmem->base));

    pstreams_init_queue(strmhead, &strmhead->appwrq, strmhead->appmod.st_wrinit);
    pstreams_init_queue(strmhead, &strmhead->apprdq, strmhead->appmod.st_rdinit);
    strmhead->apprdq.q_flag |= QREADR; /*read side get a distinuishing flag*/

    pstreams_init_queue(strmhead, &strmhead->devwrq, strmhead->devmod.st_wrinit);
    pstreams_init_queue(strmhead, &strmhead->devrdq, strmhead->devmod.st_rdinit);
    strmhead->devrdq.q_flag |= QREADR; /*read side get a distinuishing flag*/

    /*queue pairs in each module know their peer*/
    strmhead->apprdq.q_peer = &strmhead->appwrq;
    strmhead->appwrq.q_peer = &strmhead->apprdq;

    strmhead->devrdq.q_peer = &strmhead->devwrq;
    strmhead->devwrq.q_peer = &strmhead->devrdq;

    /*connect the application interface queues to the device interface queues*/
    pstreams_connect_queue(&strmhead->appwrq, &strmhead->devwrq);
    pstreams_connect_queue(&strmhead->devrdq, &strmhead->apprdq);

    if(strmhead && strmhead->appwrq.q_qinfo.qi_qopen)
    {
        if(strmhead->appwrq.q_qinfo.qi_qopen(&strmhead->appwrq) != P_STREAMS_SUCCESS)
        {
            pstreams_close(strmhead);
            strmhead=NULL;
        }
    }
    if(strmhead && strmhead->apprdq.q_qinfo.qi_qopen)
    {
        if(strmhead->apprdq.q_qinfo.qi_qopen(&strmhead->apprdq) != P_STREAMS_SUCCESS)
        {
            pstreams_close(strmhead);
            strmhead=NULL;
        }
    }

    if(strmhead && strmhead->devwrq.q_qinfo.qi_qopen)
    {
        if(strmhead->devwrq.q_qinfo.qi_qopen(&strmhead->devwrq) != P_STREAMS_SUCCESS)
        {
            pstreams_close(strmhead);
            strmhead=NULL;
        }
    }

    if(strmhead && strmhead->devrdq.q_qinfo.qi_qopen)
    {
        if(strmhead->devrdq.q_qinfo.qi_qopen(&strmhead->devrdq) != P_STREAMS_SUCCESS)
        {
            pstreams_close(strmhead);
            strmhead=NULL;
        }
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
LOGFILE *
pstreams_setltfile(P_STREAMHEAD *strmhead, char *ltfilename)
{   
    strmhead->ltfile = (LOGFILE *)LOGOPEN(ltfilename, "w");/*TODO:remove the cast*/

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
    P_QUEUE *wrq=NULL;
    P_QUEUE *rdq=NULL;

    /*TODO verify - free allocated pools and strmhead*/

    /*empty the stream*/
    while(pstreams_pop(strmhead) != 0) ;

    /*close device end*/
    wrq = strmhead->appwrq.q_next;
    rdq = RD(wrq);

    if(wrq->q_qinfo.qi_qclose)
    {
        wrq->q_qinfo.qi_qclose(wrq);
    }

    if(rdq->q_qinfo.qi_qclose)
    {
        rdq->q_qinfo.qi_qclose(rdq);
    }
    
    /*not closing app end*/
    
    /*TODO - release local and persistent memory*/

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

    if(pstreams_init_queue(strmhead, wrq, mod->st_wrinit) != P_STREAMS_SUCCESS)
    {
        return P_STREAMS_FAILURE;
    }
    if(pstreams_init_queue(strmhead, rdq, mod->st_rdinit) != P_STREAMS_SUCCESS)
    {
        return P_STREAMS_FAILURE;
    }

    rdq->q_flag |= QREADR; /*read side get a distinuishing flag*/

    /*special - to satisfy the condition 
     *"that the routines associated with 
     *one half of a stream module may find 
     *the queue associated with the other half"
     *Note: with unix streams queue's are allocated in pairs
     *in contiguous memory, with the readq first.
     *so WR(q) = RD(q)+1 holds
     *Here q_peer pointers are used - TODO use queue pair pools
     */
    wrq->q_peer = rdq;
    rdq->q_peer = wrq;

    wrq->q_next = strmhead->appwrq.q_next;
    strmhead->appwrq.q_next = wrq;

    /*topmost modules Q doesn't have other Q's above it*/
    ASSERT(strmhead->apprdq.q_next == NULL); 
    
    /*get the Q below apprdq*/
    for(q=&strmhead->devrdq; q && q->q_next; q=q->q_next )
    {
        if(q->q_next == &strmhead->apprdq)
        {
            break;
        }
    }

    ASSERT(q->q_next == &strmhead->apprdq);
        
    rdq->q_next = q->q_next;
    q->q_next = rdq;

    /*this module is now linked. Call open for each queue*/
    if(wrq->q_qinfo.qi_qopen)
    {
        if(wrq->q_qinfo.qi_qopen(wrq) != P_STREAMS_SUCCESS)
        {
            return P_STREAMS_FAILURE;
        }
    }
    if(rdq->q_qinfo.qi_qopen)
    {
        if(rdq->q_qinfo.qi_qopen(rdq) != P_STREAMS_SUCCESS)
        {
            return P_STREAMS_FAILURE;
        }
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
    rdq = RD(wrq);

    mi_idnum = wrq->q_qinfo.qi_minfo->mi_idnum;

    ASSERT(mi_idnum == rdq->q_qinfo.qi_minfo->mi_idnum);

    if(wrq->q_qinfo.qi_qclose)
    {
        wrq->q_qinfo.qi_qclose(wrq);
    }
    strmhead->appwrq.q_next = wrq->q_next;
    wrq->q_next = NULL; /*for safety*/
    lop_release(strmhead->qpool, wrq);


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
    
#ifdef DEADCODE
/******************************************************************************
                            ***DEPRECATED***
Name: pstreams_allocmsgb
Purpose: allocate a P_MSGB structure(message block). This also allocates a
    P_DATAB structure - we may consider seperating the two later.
Parameters: strmhead  - to access the pool from which to allocate
            size - size of databuffer in allocate message block(ignored for now)
            priority - priority of databuffer(ignored for now)
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_allocmsgb(P_STREAMHEAD *strmhead, int32 size, unsigned int priority)
{
    P_MSGB *msgb = NULL;

    /*
     * size and priority parameters are unused for now
     */

    ASSERT(lop_checkpool(strmhead->msgpool) == LISTOP_SUCCESS);

    msgb = (P_MSGB *)lop_alloc(strmhead->msgpool);
    if(!msgb)
    {
        return NULL;
    }
    memset(msgb, 0, sizeof(P_MSGB)); /*not init'd in lop_alloc*/

    ASSERT(lop_checkpool(strmhead->datapool) == LISTOP_SUCCESS);

    msgb->b_datap = (P_DATAB *)lop_alloc(strmhead->datapool);
    if(!msgb->b_datap)
    {
        return NULL;
    }

    memset(msgb->b_datap, 0, sizeof(P_DATAB));
    msgb->b_datap->db_base = msgb->b_datap->data; /*first byte in buffer*/
    msgb->b_datap->db_lim = msgb->b_datap->data + MAXDATABSIZE; /*TODO - replace MAXDATABSIZE with sizeof(msgb->b_datap->data)*/
                                /*last byte plus one in buffer*/
    msgb->b_datap->db_ref = 1; /*reference count is 1*/
    
    /*read-ptr and write-ptr of message block point to start of data block*/
    msgb->b_rptr = msgb->b_wptr = msgb->b_datap->db_base;

#ifdef PSTREAMS_LT
    pstreams_log(&strmhead->appwrq, PSTREAMS_LTDEBUG, 
        "pstreams_allocmsgb: bytes %d/%d.",
            size, pstreams_unwritbytes(msgb));
#endif /*PSTREAMS_LT*/

    return msgb;
}
#endif

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

    if((flags != RS_HIPRI) && !pstreams_canput(&strmhead->appwrq))
    {
#ifdef PSTREAMS_LT
        pstreams_log(&strmhead->appwrq, PSTREAMS_LTERROR, 
            "putmsg on STREAMHEAD failed with flow control restrictions");
#endif /*PSTREAMS_LT*/

        strmhead->perrno = P_BUSY; /*for now, only putmsg()'s canput() failure causes P_BUSY*/
        return P_STREAMS_FAILURE;
    }

    /*
     * ctlbuf holds the control part of the message. For eg. that this message
     * is a continuation from the last message for this session(like T_MORE set).
     * This can also be used to send a command to a particular module
     */
    if(ctlbuf && ctlbuf->len > 0)
    {
        ASSERT(ctlbuf->maxlen >= ctlbuf->len); /*I'm using maxlen!! Maybe needed for release*/

        ctl = pstreams_allocb(strmhead, ctlbuf->len, 0);
        if(!ctl)
        {
            strmhead->perrno = P_OUTOFMEMORY;
            return P_STREAMS_FAILURE;
        }
        ctl->b_datap->db_type = P_M_PROTO;
        if(flags == RS_HIPRI)
        {
            /*high priority message*/
            ctl->b_band = 1; /*higher the value, higher the pri - max 255*/
        }

        memcpy(ctl->b_wptr, ctlbuf->buf, ctlbuf->len);
        ctl->b_wptr += ctlbuf->len;

#ifdef PSTREAMS_LT
        pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_putmsg: ctl bytes %d.", 
            pstreams_msgsize(ctl));
#endif /*PSTREAMS_LT*/
    }

    /*next check for and send data messages*/
    if(msgbuf && msgbuf->len > 0)
    {
        ASSERT(msgbuf->maxlen >= msgbuf->len); /*I'm using maxlen!! Maybe needed for release*/

        msg = pstreams_allocb(strmhead, msgbuf->len, 0);
        if(!msg)
        {
            strmhead->perrno = P_OUTOFMEMORY;
            pstreams_freemsg(strmhead, ctl);
            return P_STREAMS_FAILURE;
        }
        msg->b_datap->db_type = P_M_DATA;

        memcpy(msg->b_wptr, msgbuf->buf, msgbuf->len);
        msg->b_wptr += msgbuf->len;

#ifdef PSTREAMS_LT
        pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_putmsg: msg bytes %d.", 
            pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/
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

    (void) strmhead->appwrq.q_qinfo.qi_putp(&strmhead->appwrq, tmsg);

    PDBG(flags=0); /*keep compiler happy*/

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_esmsgput - extended streams putmsg
Purpose: Apps use this function to send messages down the stream. Similar to 
    pstreams_putmsg - the relationship parallels the one between pstreams_allocb()
    and pstreams_esballoc(). Thus, application supplied buffer is used by modules.
    Avoids a copy. But assumes, memeory pointers are valid in PSTREAMS context.
Parameters: ctlbuf for control part of message. msgbuf for data part of message.
Caveats: memory allocated
******************************************************************************/
int
pstreams_esmsgput(P_STREAMHEAD *strmhead, P_ESBUF *ctlbuf, P_ESBUF *msgbuf, int flags)
{
    P_MSGB *ctl=NULL; /*control part of msg*/
    P_MSGB *msg=NULL; /*data part of msg */
    P_MSGB *tmsg=NULL;/*total msg*/

    if((flags != RS_HIPRI) && !pstreams_canput(&strmhead->appwrq))
    {
#ifdef PSTREAMS_LT
        pstreams_log(&strmhead->appwrq, PSTREAMS_LTERROR, 
            "esmsgput on STREAMHEAD failed with flow control restrictions");
#endif /*PSTREAMS_LT*/

        strmhead->perrno = P_BUSY;
        return P_STREAMS_FAILURE;
    }

    /*
     * ctlbuf holds the control part of the message. For eg. that this message
     * is a continuation from the last message for this session(like T_MORE set).
     * This can also be used to send a command to a particular module
     */
    if(ctlbuf && ctlbuf->len > 0)
    {
        ASSERT(ctlbuf->maxlen >= ctlbuf->len); /*I'm using maxlen!! Maybe needed for release*/

        /*
         *control portion is copied instead of being just referenced
         */
        ctl = pstreams_allocb(strmhead, ctlbuf->len, 0);
        if(!ctl)
        {
            strmhead->perrno = P_OUTOFMEMORY;
            return P_STREAMS_FAILURE;
        }
        ctl->b_datap->db_type = P_M_PROTO;
        if(flags == RS_HIPRI)
        {
            /*high priority message*/
            ctl->b_band = 1; /*higher the value, higher the pri - max 255*/
        }

        memcpy(ctl->b_wptr, ctlbuf->buf, ctlbuf->len);
        ctl->b_wptr += ctlbuf->len;

#ifdef PSTREAMS_LT
        pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_putmsg: ctl bytes %d.", 
            pstreams_msgsize(ctl));
#endif /*PSTREAMS_LT*/
    }

    /*next check for and send data messages*/
    if(msgbuf && msgbuf->len > 0)
    {
        ASSERT(msgbuf->maxlen >= msgbuf->len); /*I'm using maxlen!! Maybe needed for release*/

        if(!msgbuf->fr_rtnp)
        {
            /*
             * as per current design externally allocated memory need non-null frtnp
             * else, pstreams_freeb assumes it is pstreams internally allocated!!
             */
            pstreams_console("ERROR - pstreams_esmsgput: externally allocated "
                                "memory in data buffer has NULL frtnp");
            pstreams_freemsg(strmhead, ctl);
            return P_STREAMS_FAILURE;
        }

        msg = pstreams_esballoc(strmhead, (unsigned char *)msgbuf->buf, 
                                msgbuf->maxlen, 0, msgbuf->fr_rtnp);
        if(!msg)
        {
            strmhead->perrno = P_OUTOFMEMORY;
            pstreams_freemsg(strmhead, ctl);
            return P_STREAMS_FAILURE;
        }
        msg->b_datap->db_type = P_M_DATA;

        /*no need to copy - just advance write pointer*/
        msg->b_wptr += msgbuf->len;

#ifdef PSTREAMS_LT
        pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_putmsg: msg bytes %d.", 
            pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/
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

    (void) strmhead->appwrq.q_qinfo.qi_putp(&strmhead->appwrq, tmsg);

    PDBG(flags=0); /*keep compiler happy*/

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
    P_MSGB *msg=NULL;
    P_MSGB *ctlmsg=NULL;
    P_MSGB *datmsg=NULL;
    int all_is_well=P_TRUE;
    
    if(pflags)  /*unused for now*/
    {
        *pflags = 0;
    }

    if(ctlbuf)
    {
        ctlbuf->len = 0;

        PDBG(memset(ctlbuf->buf, 0, ctlbuf->maxlen));
    }
    if(msgbuf) /*init to empty*/
    {
        msgbuf->len = 0;

        PDBG(memset(msgbuf->buf, 0, msgbuf->maxlen));
    }

    if(strmhead->perrno != 0)
    {
        return P_STREAMS_FAILURE;
    }

    msg = pstreams_getq(&strmhead->apprdq);
    if(!msg)
    {
        /*no message*/
        return P_STREAMS_SUCCESS;
    }

#ifdef PSTREAMS_LT
    pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_getmsg: msg bytes %d.",
            pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/

    pstreams_sift(&strmhead->apprdq, msg, ctl_or_data,
        &datmsg, &ctlmsg);

    PDBG(msg = NULL);

    if(datmsg)
    {
        msgbuf->len = (int)pstreams_msgsize(datmsg);
        if(msgbuf->maxlen >= msgbuf->len)
        {
            msgbuf->len = pstreams_msgread(msgbuf, datmsg);
            pstreams_msgconsume(datmsg, msgbuf->len);
            pstreams_freemsg(strmhead, datmsg);
        }
        else
        {
            /*not enough memory - inform user*/
            msgbuf->len = -1;
            memset(msgbuf->buf, 0, msgbuf->maxlen);
            all_is_well = P_FALSE;
        }
    }

    if(ctlmsg)
    {
        /*
         * the only ctlmsg sent up is P_M_PROTO
         */
        ASSERT(ctlmsg->b_datap->db_type == P_M_PROTO);

        ctlbuf->len = (int)pstreams_msgsize(ctlmsg);
        if(ctlbuf->maxlen >= ctlbuf->len)
        {
            ctlbuf->len = pstreams_msgread(ctlbuf, ctlmsg);
            pstreams_msgconsume(ctlmsg, ctlbuf->len);
            pstreams_freemsg(strmhead, ctlmsg);
        }
        else
        {
            /*not enough memory - inform user*/
            ctlbuf->len = -1;
            memset(ctlbuf->buf, 0, ctlbuf->maxlen);
            all_is_well = P_FALSE;
        }
    }

    if(!all_is_well)
    {
        /*msg goes back in*/
        if(ctlmsg)
        {
            pstreams_linkb(ctlmsg, datmsg); /*null datmsg is OK*/
            msg = ctlmsg;
        }
        else if(datmsg)
        {
            msg = datmsg;
        }
        if(msg)
        {
            lop_push(&strmhead->apprdq.q_msglist, msg);
        }

        strmhead->perrno = P_READBUF_TOOSMALL;

        return P_STREAMS_FAILURE;
    }
    
    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_msgcount
Purpose: Apps use this function to get count of messages waiting to be read
Parameters: 
Caveats:
******************************************************************************/
int
pstreams_msgcount(P_STREAMHEAD *strmhead)
{
    return pstreams_qsize(&strmhead->apprdq);
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
    int32 size=0;
    uint32 chunksize=0;
    char *wptr = (char *)wbuf->buf;/*pointer to next byte to write*/

    while(msg)
    {
        chunksize = pstreams_msg1size(msg);
        if(chunksize > 0)
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
    int32 bytestocopy=rbuf->len;
    char *rbufptr = (char *)rbuf->buf;
    int32 chunksize=0;

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
         * the below ASSERT checks that once we start the copy subsequent
         * msgs(until the end hence pstreams_msgsize and not pstreams_msg1size)
         * are empty
         */
        ASSERT(bytestocopy == rbuf->len || pstreams_msgsize(msg) == 0);
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
pstreams_msgconsume(P_MSGB *msg, uint32 bytes)
{
    uint32 chunksize=0;

    while(msg && bytes>0)
    {
        chunksize = MIN(pstreams_msg1size(msg), bytes);
        if(chunksize > 0)
        {
            msg->b_rptr += chunksize;
            ASSERT(msg->b_rptr <= msg->b_wptr);
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
pstreams_msgerase(P_MSGB *msg, uint32 bytes)
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
pstreams_msg1erase(P_MSGB *msg, uint32 bytes)
{
    uint32 chunksize=0;

    if(msg && bytes>0)
    {
        chunksize = MIN(bytes, pstreams_msg1size(msg));
        msg->b_wptr -= chunksize;
        ASSERT(msg->b_rptr <= msg->b_wptr);
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
//pstreams_checkmem(strmhead);

    /*step thru downstream queues calling srvp() on each*/
    for(dq=&strmhead->appwrq;
        dq;
        dq = dq->q_next)
    {
#ifdef PSTREAMS_LT
        pstreams_log(dq, PSTREAMS_LTDEBUG, "pstreams_callsrvp");
#endif /*PSTREAMS_LT*/

        if(dq->q_qinfo.qi_srvp)
        {
            if(dq->q_qinfo.qi_srvp(dq) != P_STREAMS_SUCCESS)
            {
                return P_STREAMS_FAILURE;
            }
        }
        else
        {
            /*default*/
            if(pstreams_srvp(dq) != P_STREAMS_SUCCESS)
            {
                return P_STREAMS_FAILURE;
            }
        }
    }

    /*step thru upstream queues calling srvp() on each*/
    for(uq=&strmhead->devrdq;
        uq;
        uq = uq->q_next)
    {
#ifdef PSTREAMS_LT
        pstreams_log(uq, PSTREAMS_LTDEBUG, "pstreams_callsrvp");
#endif /*PSTREAMS_LT*/

        if(uq->q_qinfo.qi_srvp)
        {
            if(uq->q_qinfo.qi_srvp(uq) != P_STREAMS_SUCCESS)
            {
                return P_STREAMS_FAILURE;
            }
        }
        else
        {
            /*default*/
            if(pstreams_srvp(uq) != P_STREAMS_SUCCESS)
            {
                return P_STREAMS_FAILURE;
            }
        }
    }

/*DEBUG mode*/
//pstreams_checkmem(strmhead);

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

    while(pstreams_canput(q->q_next))
    {
        msg = pstreams_getq(q);
        if(!msg)
        {
            break; /*queue empty*/
        }

        /*module specific processing go here*/

        /*send this msg onward*/
        if(pstreams_putnext(q, msg) != P_STREAMS_SUCCESS)
        {
            return P_STREAMS_FAILURE;
        }
    }

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_qsize
Purpose: counts number of messages in queue, but not the number of bytes in those
         messages
Parameters: 
Caveats:
******************************************************************************/
int pstreams_qsize(P_QUEUE *q)
{
    return lop_listlen(q->q_msglist);

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

#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTINFO, "gettq: removed %d bytes from q",
                pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/

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
#ifdef PSTREAMS_LT
    pstreams_log(wrq, PSTREAMS_LTINFO, "pstreams_putnext: transferred %d bytes to %s.",
            pstreams_msgsize(msg), wrq->q_next->q_qinfo.qi_minfo->mi_idname);
#endif /*PSTREAMS_LT*/

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

#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTINFO, "putq: added %d bytes to q",
                pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/

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

#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTINFO, "putbq: added %d bytes back to q",
                pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/

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

    ASSERT(q);
    
    switch(ctlflag)
    {
    case P_M_DELAY:
        msg = pstreams_allocb(PSTRMHEAD(q), 0,0);
        msg->b_datap->db_type = P_M_DELAY;
        break;
    case P_M_BREAK:
        msg = pstreams_allocb(PSTRMHEAD(q), 0,0);
        msg->b_datap->db_type = P_M_BREAK;
        break;
    case P_M_DELIM:
        msg = pstreams_allocb(PSTRMHEAD(q), 0,0);
        msg->b_datap->db_type = P_M_DELIM;
        break;
    default:
        ASSERT(0);
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

    /*
     *beware q_hiwat must be below max capacity by atleast max message size
     *otherwise the next could be large enough to swamp us
     */

    /*clear QFULL if below low water mark*/
    if(q->q_count < q->q_lowat)
    {
        q->q_flag &= ~QFULL;
    }

    if(q->q_flag & QFULL)
    {
        q->q_flag |= QWANTW; /*indicates that a function wants to put
                            data in this queue; but is not being allowed
                            to do so*/
        return P_FALSE;
    }

    return P_TRUE;
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
    q->strmhead = strmhead;

    ASSERT(q->strmhead); /*for debug - in some cases q->strmhead is NULL!*/

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

    /*populate q->q_info structure - 
     * note qi->qi_minfo and qi->qi_mstat are only shallow copied
     */
    /*q->q_qinfo = *qi; - compiler flaky on struct copies...so*/
    q->q_qinfo.qi_mchk = qi->qi_mchk;
    q->q_qinfo.qi_minfo = qi->qi_minfo;
    q->q_qinfo.qi_mstat = qi->qi_mstat;
    q->q_qinfo.qi_putp = qi->qi_putp;
    q->q_qinfo.qi_qclose = qi->qi_qclose;
    q->q_qinfo.qi_qopen = qi->qi_qopen;
    q->q_qinfo.qi_srvp = qi->qi_srvp;

    q->q_msglist = NULL;
    q->q_ptr = NULL;
    q->q_enabled = P_TRUE;
    q->q_next = NULL;


    /*get some defaults from qi*/
    q->q_count = 0;
    q->q_flag = QRESET;
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

#ifdef DEADCODE
/******************************************************************************
                            ***DEPRECATED***
Name: pstreams_relmsg - deprecated
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
pstreams_relmsg(P_QUEUE *q, P_MSGB *msg)
{
    P_STREAMHEAD *strmhead = PSTRMHEAD(q);

    if(msg)
    {
        if(msg->b_cont)
        {
            /*recursively release*/
            pstreams_relmsg(q, msg->b_cont);
        }


        ASSERT(msg->b_datap->db_ref > 0);
        
        /*decrement datablocks reference count*/
        msg->b_datap->db_ref--;

        if(msg->b_datap->db_ref == 0)
        {
            /*can free P_DATAB and associated data now*/
            if(msg->b_datap->db_frtnp)
            {
                void (*free_func)() = msg->b_datap->db_frtnp->free_func;
                char *arg=msg->b_datap->db_frtnp->free_arg;

                ASSERT(free_func);

                if(arg)
                {
                    free_func(arg);
                }
                else
                {
                    free_func(msg->b_datap->db_base, 
                        (int)(msg->b_datap->db_lim - msg->b_datap->db_base));
                }
            }
            else
            {
                if(msg->b_datap->db_base)
                {
                    int32 poolsize = 
                        pstreams_mpool((int)(msg->b_datap->db_lim - msg->b_datap->db_base));
                    pstreams_mem_free(strmhead, msg->b_datap->db_base, poolsize);
                }
            }

            PDBG(memset(msg->b_datap, 0, sizeof(P_DATAB)));
            lop_release(strmhead->datapool, msg->b_datap);
        }

        PDBG(memset(msg, 0, sizeof(P_MSGB)));
        lop_release(strmhead->msgpool, msg);

    }

#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTDEBUG, "pstreams_relmsg: bytes released %d.",
            pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/

    return P_STREAMS_SUCCESS;
}
#endif

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

    msgsiz = msg->b_wptr - msg->b_rptr;/*could be call to pstreams_msg1size()*/
    ASSERT(msgsiz < MAXDATABSIZE);

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

    ASSERT(msg);

    msgsiz = msg->b_wptr - msg->b_rptr;
    ASSERT(msgsiz < MAXDATABSIZE);

    return msgsiz;
}

/******************************************************************************
Name: pstreams_unwrit1bytes
Purpose: Calculate the free space available on this message block, for writing.
    Do not consider continuations.
Parameters:
Caveats:
******************************************************************************/
uint32 pstreams_unwrit1bytes(P_MSGB *msg)
{
    int32 unwrit1bytes=0;

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
uint32 pstreams_unwritbytes(P_MSGB *msg)
{
    int32 unwritbytes=0;

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
pstreams_msg1copy(P_MSGB *to, P_MSGB *from, uint32 bytestocopy)
{
    ASSERT(to && (pstreams_unwrit1bytes(to) >= bytestocopy));
    ASSERT(from && (pstreams_msg1size(from) >= bytestocopy));

    memcpy(to->b_wptr, from->b_rptr, bytestocopy);
    to->b_wptr += bytestocopy;
    ASSERT(to->b_wptr <= to->b_datap->db_lim);/*don't overrun the edge*/
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
pstreams_msgcpy(P_MSGB *to, P_MSGB *from, uint32 bytestocopy)
{
    uint32 msgchunk=0;
    uint32 bitesize=0;

    ASSERT(to);
    ASSERT(pstreams_msgsize(from) >= bytestocopy);
    ASSERT(pstreams_unwritbytes(to) >= bytestocopy);
    
    while(bytestocopy && from)
    {
        msgchunk = pstreams_msg1size(from);

        bitesize = MIN(msgchunk, bytestocopy);

        if(pstreams_unwrit1bytes(to) == 0)
        {
            ASSERT(to->b_cont);
            to = to->b_cont;
        }

        pstreams_msg1copy(to, from, bitesize);

        bytestocopy -= bitesize;

        from = from->b_cont;/*go to continuation*/
    }

    return (bytestocopy == 0 ? P_STREAMS_SUCCESS : P_STREAMS_FAILURE);
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

    ASSERT(!*msg || (pstreams_msg1size(*msg)>0));

    msgiter = (*msg ? (*msg)->b_cont : NULL); /*skip one because previous loop implies that*/

    while(msgiter && msgiter->b_cont)
    {
        if(pstreams_msg1size(msgiter->b_cont) == 0)
        {
            if(!garbage) /*first time*/
            {
                garbageiter = garbage = *msg;
            }
            garbageiter->b_cont = msgiter->b_cont;
            garbageiter = garbageiter->b_cont;
            msgiter->b_cont = msgiter->b_cont->b_cont;
            garbageiter->b_cont = NULL;
        }

        msgiter = msgiter->b_cont;
    }
        
#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTDEBUG, "pstreams_garbagecollect: garbage bytes %d.",
            pstreams_msgsize(garbage));
#endif /*PSTREAMS_LT*/

    pstreams_freemsg(PSTRMHEAD(q), garbage);

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
Name: ctl_or_data
Purpose:  
Parameters:
Caveats: 
******************************************************************************/
int
ctl_or_data(P_QUEUE *q, P_MSGB *msg)
{
    PDBG(q=NULL); /*keep compiler happy*/

    switch(msg->b_datap->db_type)
    {
    case P_M_DATA:
        return 1;
    case P_M_PROTO:
    case P_M_CTL:
    case P_M_ERROR:
        return 2;
    default:
        break;
    }

    return -1;
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

    ASSERT(*msglist1 == NULL);
    ASSERT(*msglist2 == NULL);

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
#ifdef PSTREAMS_LT
            pstreams_log(q, PSTREAMS_LTDEBUG, "pstreams_sift: sift() returned %d", siftval);
#endif /*PSTREAMS_LT*/
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
pstreams_ctlexpress(P_QUEUE *q, P_MSGB *msg, P_BOOL (*myctl)(P_QUEUE *, P_MSGB *),
                    P_MSGB **ctlmsgs, P_MSGB **datmsgs)
{
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
        case P_M_PROTO:
        case P_M_CTL:
            if(myctl(q, msg))
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
            ASSERT(0);
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
    int32 q_count=-1;
    LOGFILE *ltfile=NULL;
    P_STREAMHEAD *strmhead=NULL;
    int32 msg_count=-1;

    if(!pstreams_ltfilter(q, ltcode))
    {
        return P_STREAMS_SUCCESS;
    }

    strmhead = PSTRMHEAD(q);
    if(strmhead)
    {
        ltfile = strmhead->ltfile;
    }
    else
    {
        ltfile = stderr;
    }

    if(q)
    {
        modulename = q->q_qinfo.qi_minfo->mi_idname;
        q_count = q->q_count;
        msg_count = pstreams_countmsg(q->q_msglist);
    }

    va_start(ap, fmt);

    if(ltcode >= PSTREAMS_LTERROR-1)
    {
        LOGWRITE(ltfile, 
            "HIPRI %s qcount=%d/%d ", modulename, q_count, msg_count);
    }
    else 
    {
        LOGWRITE(ltfile, 
            "%dPRI %s qcount=%d/%d ",(int)ltcode, modulename, q_count, msg_count);
    }

    VLOGWRITE(ltfile, fmt, ap);

    /*va_end(ap);*/

    PDBG(pstreams_flushlog(strmhead));     /*flush in debug mode*/

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
Name: pstreams_flushlog
Purpose: flushes log file
Parameters:
Caveats: 
******************************************************************************/
void
pstreams_flushlog(P_STREAMHEAD *strm)
{
    if(strm)
    {
        LOGFLUSH(strm->ltfile);
    }

    return;
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
    int32 count=0;
    
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
    int32 msgcount=0;

    ASSERT(q);
    ASSERT(get);

    ASSERT(get->type == GETMSGCNT);

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
    P_GETVAL get={GETMSGCNT, 0};
    int32 mod_msgcount=0;
    int32 lop_msgcount=0;

#ifdef PSTREAMS_LT
    pstreams_log(&strmhead->apprdq, PSTREAMS_LTDEBUG, "pstreams_checkmem:");
#endif /*PSTREAMS_LT*/

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

#ifdef PSTREAMS_LT
        pstreams_log(dq, (P_LTCODE)(PSTREAMS_LTDEBUG-1), "\tmodule says MBLKs in use=%d",
            get.val.msgcount);
#endif /*PSTREAMS_LT*/
    }

#ifdef PSTREAMS_LT
    pstreams_log(&strmhead->apprdq, (P_LTCODE)(PSTREAMS_LTDEBUG-1),
                    "\t\tmodules say Sum of MBLKs in use=%d", mod_msgcount);
#endif /*PSTREAMS_LT*/

    lop_msgcount = strmhead->msgpool->count - strmhead->msgpool->freecount;

#ifdef PSTREAMS_LT
    pstreams_log(&strmhead->apprdq, (P_LTCODE)(PSTREAMS_LTDEBUG-1), 
                "\t\tLISTOP says Sum of MBLKs in use=%d(total=%d,free=%d)",
                    lop_msgcount, strmhead->msgpool->count, strmhead->msgpool->freecount );
#endif /*PSTREAMS_LT*/

    if(lop_msgcount != mod_msgcount)
    {
#ifdef PSTREAMS_LT
        pstreams_log(&strmhead->apprdq, (P_LTCODE)(PSTREAMS_LTERROR), "MBLK COUNTS MISMATCH - NOTOK");
#endif /*PSTREAMS_LT*/

        ASSERT(0);
    }
    else
    {
#ifdef PSTREAMS_LT
        pstreams_log(&strmhead->apprdq, (P_LTCODE)(PSTREAMS_LTDEBUG-1), "MBLK COUNTS MATCH - OK");
#endif /*PSTREAMS_LT*/
    }

    return P_STREAMS_SUCCESS;
}

int
pstreams_console(const char *fmt,...)
{
    va_list ap;
    
    va_start(ap, fmt);
    VCONSOLEWRITE(fmt, ap);
    /*va_end(ap)*/

    return P_STREAMS_SUCCESS;
}

/******************************************************************************
Name: pstreams_memassign
Purpose: allocate and return memory of given size from given mem chunk
Parameters: 
Caveats: 
******************************************************************************/
void *
pstreams_memassign(P_MEM *mem, int32 size)
{
    void *rptr=NULL; /*returned ptr*/

    /*DEBUG mode*/
    {
        unsigned long end = (unsigned long)mem->limit;
        unsigned long start = (unsigned long)mem->base;
        unsigned long diff = end - start;
        pstreams_console("pstreams_memassign: start=0x%lX, end=0x%lX, diff=%lu bytes."
            " assigning %ld bytes", start, end, diff, size);
        if((int32)diff < size)
        {
            return rptr;
        }
    }

    ASSERT((mem->limit - (char *)WALIGN((unsigned long)mem->base)) >= size);

    if(size > 1) /*align for multi-byte structures*/
    {
        mem->base = (char *)WALIGN((unsigned long)mem->base);
    }

    if((mem->base + size) <= mem->limit)
    {
        /*All OK*/
        rptr = mem->base;
        mem->base += size;
        /*memset(rptr, 0, size); - disabled - think persistent memory*/
    }

    return rptr;
}

/******************************************************************************
Name: pstreams_allocb
Purpose: allocate a P_MSGB structure(message block). This also allocates a
    P_DATAB structure - we may consider seperating the two later.
    The priority of the allocated P_DATAB is always P_M_DATA.
Parameters: strmhead  - to access the pool from which to allocate - implementation private
            size - size of databuffer in allocate message block
            priority - priority of databuffer(no longer used as in man allocb)
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_allocb(P_STREAMHEAD *strmhead, int32 size, uint priority)
{
    P_MSGB *msgb = NULL;

    /*
     * size parameter is used only to determine if a data buffer needs to
     * be allocated for P_DATAB
     * priority, like in man allocb, is no longer used
     */

    msgb = (P_MSGB *)lop_alloc(strmhead->msgpool);
    if(!msgb)
    {
        return NULL;
    }
    memset(msgb, 0, sizeof(P_MSGB)); /*not init'd in lop_alloc*/

    ASSERT(lop_checkpool(strmhead->datapool) == LISTOP_SUCCESS);

    msgb->b_datap = (P_DATAB *)lop_alloc(strmhead->datapool);
    if(!msgb->b_datap)
    {
        return NULL;
    }

    memset(msgb->b_datap, 0, sizeof(P_DATAB));

    msgb->b_datap->db_ref = 1; /*reference count is 1, on creation*/

    if(size > 0)
    {
        /*need to allocate a data buffer*/
        unsigned char *data = NULL;

        /*roundup size to pre-defined buffer sizes*/
        size = pstreams_mpool(size);

        if(size <= FASTBUFSIZE) 
        {
            ASSERT(size == FASTBUFSIZE); /*s'posed to be rounded up*/

            /*use built-in small buffer instead of allocating from pool*/
            data = msgb->b_datap->FASTBUF;
        }
        else
        {
            /*need to allocate a data buffer*/
            data = pstreams_mem_alloc(strmhead, size, PSTREAMS_BLOCK);
            if(!data)
            {
                lop_release(strmhead->datapool, msgb->b_datap);
                msgb->b_datap=NULL;
                lop_release(strmhead->msgpool, msgb);
                return NULL;
            }
        }

        msgb->b_datap->db_base = data; /*first byte in buffer*/
        msgb->b_datap->db_lim = msgb->b_datap->db_base + size; /*last byte plus one in buffer*/
    
        /*read-ptr and write-ptr of message block point to start of data block*/
        msgb->b_rptr = msgb->b_wptr = msgb->b_datap->db_base;

        /*Note:
         * db_frtnp for b_datap is fixed at FREEDATA for now, with no args.
         * FREEDATA is a global!
         */
        msgb->b_datap->db_frtnp = (P_FREE_RTN *)NULL;
    }

#ifdef PSTREAMS_LT
    pstreams_log(&strmhead->appwrq, PSTREAMS_LTDEBUG, 
        "pstreams_allocb: bytes %d/%d.",
            size, pstreams_unwritbytes(msgb));
#endif /*PSTREAMS_LT*/

    PDBG(priority=0); /*unused*/

    return msgb;
}

/******************************************************************************
Name: pstreams_esballoc
Purpose: allocates P_MSGB structure(message block) with a P_DATAB using a caller 
    supplied buffer. 
    This uses pstreams_alloc(). Though it is lesser code if pstreams_alloc() used
    this routine, this way we make pstreams_alloc() independent of pstreams_esballoc().
    This order follows the documentation in man esballoc.
Parameters:
    strmhead  - to access the pool from which to allocate - implementation private
    base - address of user supplied data buffer
    size - number of bytes in above buffer
    priority - passed on to pstreams_allocb
    free_rtn - data structure used to free given data buffer, later
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_esballoc(P_STREAMHEAD *strmhead, unsigned char *base, int32 size, int pri, P_FREE_RTN *free_rtn)
{
    P_MSGB *msgb = NULL;

    ASSERT(base);
    ASSERT(size!=0);

    msgb = pstreams_allocb(strmhead, 0, pri);

    if(!msgb)
    {
        return NULL;
    }

    msgb->b_datap->db_base = base; /*first byte in buffer*/
    msgb->b_datap->db_lim = msgb->b_datap->db_base + size; /*last byte plus one in buffer*/
    msgb->b_datap->db_ref = 1; /*reference count is 1*/
    
    /*read-ptr and write-ptr of message block point to start of data block*/
    msgb->b_rptr = msgb->b_wptr = msgb->b_datap->db_base;

    msgb->b_datap->db_frtnp = free_rtn;

#ifdef PSTREAMS_LT
    pstreams_log(&strmhead->appwrq, PSTREAMS_LTDEBUG, 
        "pstreams_esballoc: bytes %d/%d.",
            size, pstreams_unwritbytes(msgb));
#endif /*PSTREAMS_LT*/

    return msgb;
}
    
int32
pstreams_mpool(int32 size)
{
    int32 adjsize=0; /*adjusted size*/

    if(size == 0)
    {
        adjsize = size;
    }
    else if(size <= FASTBUFSIZE)
    {
        adjsize = FASTBUFSIZE;
    }
#if(POOL16SIZE > 0)
    else if(size <= 16)
    {
        adjsize = 16;
    }
#endif
#if(POOL64SIZE > 0)
    else if(size <= 64)
    {
        adjsize = 64;
    }
#endif
#if(POOL256SIZE > 0)
    else if(size <= 256)
    {
        adjsize = 256;
    }
#endif
#if(POOL512SIZE > 0)
    else if(size <= 512)
    {
        adjsize = 512;
    }
#endif
#if(POOL1792SIZE > 0)
    else if(size <= 1792)
    {
        adjsize = 1792;
    }
#endif

    return adjsize;
}

unsigned char *
pstreams_mem_alloc(P_STREAMHEAD *strmhead, int32 size, int flag)
{
    switch(size)
    {
#if(POOL16SIZE > 0)
    case 16:
        return lop_alloc(strmhead->pool16);
#endif
#if(POOL64SIZE > 0)
    case 64:
        return lop_alloc(strmhead->pool64);
#endif
#if(POOL256SIZE > 0)
    case 256:
        return lop_alloc(strmhead->pool256);
#endif
#if(POOL512SIZE > 0)
    case 512:
        return lop_alloc(strmhead->pool512);
#endif
#if(POOL1792SIZE > 0)
    case 1792:
        return lop_alloc(strmhead->pool1792);
#endif
    default:
        return NULL;
    }

    PDBG(flag=0); /*unused*/

    return NULL; /*never here*/
}

/*
 * Used only on pool64,...type pools
 */
void
pstreams_mem_free(P_STREAMHEAD *strmhead, void *buf, int32 size)
{
    switch(size)
    {
    case 0:
        break;
    case FASTBUFSIZE:
        /*nothing to release as this buffer is built in to every P_DATAB*/
        break;
#if(POOL16SIZE > 0)
    case 16:
        lop_release(strmhead->pool16, buf);
        break;
#endif
#if(POOL64SIZE > 0)
    case 64:
        lop_release(strmhead->pool64, buf);
        break;
#endif
#if(POOL256SIZE > 0)
    case 256:
        lop_release(strmhead->pool256, buf);
        break;
#endif
#if(POOL512SIZE > 0)
    case 512:
        lop_release(strmhead->pool512, buf);
        break;
#endif
#if(POOL1792SIZE > 0)
    case 1792:
        lop_release(strmhead->pool1792, buf);
        break;
#endif
    default:
        return ;
    }

    return; /*never here*/
}

/******************************************************************************
Name: pstreams_dupmsg
Purpose: duplicate a message. Data blocks (P_DATABs) are re-used.
    Follows man dupmsg.
Parameters:
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_dupmsg(P_STREAMHEAD *strmhead, P_MSGB *initmsg)
{
    P_MSGB *msg=NULL; /*new message to be constructed*/
    P_MSGB *msgiter=NULL;
    P_MSGB *msgiter_prev=NULL;

    PDBG(P_MSGB *original_msg=initmsg);

    while(initmsg)
    {
        ASSERT(msgiter == NULL);

        msgiter = pstreams_dupb(strmhead, initmsg);
        if(!msgiter)
        {
            pstreams_freemsg(strmhead, msg);
#ifdef PSTREAMS_LT
            pstreams_log(&(strmhead->apprdq), PSTREAMS_LTWARNING, 
                "pstreams_dupmsg: pstreams_dupb failed");
#endif /*PSTREAMS_LT*/
            return NULL;
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
        
        initmsg = initmsg->b_cont;
        msgiter_prev = msgiter;
        msgiter = msgiter->b_cont;
    }

    PDBG(ASSERT(pstreams_msgsize(msg) == pstreams_msgsize(original_msg)));

    return msg;
}

/******************************************************************************
Name: pstreams_dupb
Purpose: duplicate a message block (P_MSGB) - P_DATAB part is reused
    Follows man dupb.
Parameters:
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_dupb(P_STREAMHEAD *strmhead, P_MSGB *initmsg)
{
    P_MSGB *msgb=NULL; /*new message to be constructed*/

    if(initmsg)
    {
        if(initmsg->b_datap->db_ref >= 255)/*255 max for db_ref, a uchar*/
        {
            /*can't reference it further*/
            return NULL;
        }

        msgb = (P_MSGB *)lop_alloc(strmhead->msgpool);
        if(!msgb)
        {
            return NULL;
        }
        memset(msgb, 0, sizeof(P_MSGB)); /*not init'd in lop_alloc*/

        /*increment reference count*/
        initmsg->b_datap->db_ref++; 

        msgb->b_datap = initmsg->b_datap;
        msgb->b_rptr = initmsg->b_rptr;
        msgb->b_wptr = initmsg->b_wptr;
        msgb->b_band = initmsg->b_band;
    }
    
    return msgb;
}

/******************************************************************************
Name: pstreams_dupnmsg
Purpose: duplicate first n bytes of a message. Data blocks (P_DATABs) are re-used.
    This is a variant of pstreams_dupmsg(), except that it only duplicates 
    the first n bytes.
Parameters:
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_dupnmsg(P_STREAMHEAD *strmhead, P_MSGB *initmsg, int32 len)
{
    P_MSGB *msg=NULL; /*new message to be constructed*/
    P_MSGB *msgiter=NULL;
    P_MSGB *msgiter_prev=NULL;

    PDBG(P_MSGB *original_msg=initmsg);
    PDBG(int32 original_len=len);

    if(pstreams_msgsize(initmsg) < len)
    {
        len = pstreams_msgsize(initmsg);
    }

    while(initmsg && (len>0))
    {
        ASSERT(msgiter == NULL);

        msgiter = pstreams_dupb(strmhead, initmsg); /*duplicate first block*/
        if(!msgiter)
        {
            pstreams_freemsg(strmhead, msg);
#ifdef PSTREAMS_LT
            pstreams_log(&(strmhead->apprdq), PSTREAMS_LTWARNING, 
                "pstreams_dupmsg: pstreams_dupb failed");
#endif /*PSTREAMS_LT*/
            return NULL;
        }

        /*adjust msgiter's block length*/
        if(len < pstreams_msg1size(msgiter))
        {
            pstreams_msg1erase(msgiter, pstreams_msg1size(msgiter) - len);
        }

        ASSERT(len >= pstreams_msg1size(msgiter));

        len -= pstreams_msg1size(msgiter);

        if(msgiter_prev)
        {
            msgiter_prev->b_cont = msgiter;
        }
        else
        {
            /*this is the first time thru this loop*/
            msg = msgiter_prev = msgiter;
        }
        
        initmsg = initmsg->b_cont;
        msgiter_prev = msgiter;
        msgiter = msgiter->b_cont;
    }

    PDBG(ASSERT(pstreams_msgsize(msg) <= pstreams_msgsize(original_msg)));

    return msg;
}

/******************************************************************************
Name: pstreams_copymsg
Purpose: this is like the copy constructor for MSGB. Allocates a new message block
    and copies from 'initmsg' onto the new block
Parameters:
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_copymsg(P_STREAMHEAD *strmhead, P_MSGB *initmsg)
{
    P_MSGB *msg=NULL;
    P_MSGB *msgiter=NULL;
    P_MSGB *msgiter_prev=NULL;

    while(initmsg)
    {
        ASSERT(msgiter == NULL);
        msgiter = pstreams_copyb(strmhead, initmsg);
        if(!msgiter)
        {
            pstreams_freemsg(strmhead, msg);
#ifdef PSTREAMS_LT
            pstreams_log(&(strmhead->apprdq), PSTREAMS_LTWARNING, 
                "pstreams_copymsg: pstreams_copyb failed");
#endif /*PSTREAMS_LT*/
            return NULL;
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
        
        initmsg = initmsg->b_cont;
        msgiter_prev = msgiter;
        msgiter = msgiter->b_cont;
    }

#ifdef PSTREAMS_LT
    pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_copymsg: bytes copied %d.",
            pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/

    return msg;
}

/******************************************************************************
Name: pstreams_copyb
Purpose: make a copy of a message block (P_MSGB) - P_DATAB part is re-allocated
    Follows man copyb.
Parameters:
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_copyb(P_STREAMHEAD *strmhead, P_MSGB *initmsg)
{
    P_MSGB *msgb=NULL; /*new message to be constructed*/

    if(initmsg)
    {
        int32 size = pstreams_msg1size(initmsg);
        msgb = pstreams_allocb(strmhead, size, 0);
        if(!msgb)
        {
            return NULL;
        }
        msgb->b_datap->db_type = initmsg->b_datap->db_type;

        msgb->b_band = initmsg->b_band;

        pstreams_msg1copy(msgb, initmsg, size);
    }
    
    return msgb;
}

/******************************************************************************
Name: pstreams_msgpullup
Purpose: concatenates bytes in a message - see man msgpullup
    concatenates the first len data bytes of initmsg, copying the data into
    a new message. Any remaining bytes in initmsg will be copied and linked
    onto the new message. initmsg is unaltered. if len equals -1 all data are
    concatenated. returns NULL on failure, i.e., if len bytes of same data type
    cannot be found.
Parameters:
Caveats:  currently only works for P_M_DATA blocks
******************************************************************************/
P_MSGB *
pstreams_msgpullup(P_STREAMHEAD *strmhead, P_MSGB *initmsg, int32 len)
{
    P_MSGB *msg=NULL;
    P_MSGB *remmsg=NULL;
    int32 remlen=0;
    unsigned char *rptr = NULL; /*current read pointer into initmsg*/

    if(len == -1)
    {
        len = pstreams_msgsize(initmsg);
    }
    else if(len == 0 || len > pstreams_msgsize(initmsg))
    {
        return NULL; /*invalid usage case*/
    }

    msg = pstreams_allocb(strmhead, len, 0);

    if(!msg)
    {
        return NULL; /*can not get a data block of this size*/
    }

    msg->b_band = initmsg->b_band;

    while(len)
    {
        int32 copylen = MIN(pstreams_msg1size(initmsg), len);

        ASSERT(initmsg);

        rptr = initmsg->b_rptr;
        memcpy(msg->b_wptr, rptr, copylen);
        msg->b_wptr += copylen;
        rptr += copylen;
        len -= copylen;

        initmsg = initmsg->b_cont;
    }

    if(initmsg)
    {
        /*rptr is within read and write boundary of current initmsg block*/
        ASSERT((rptr >= initmsg->b_rptr) && (rptr <= initmsg->b_wptr));

        /*remaining uncopied len of message*/
        remlen = pstreams_msgsize(initmsg) - (rptr - initmsg->b_rptr);
    }
    else
    {
        remlen = 0;
    }

    ASSERT(remlen >= 0);

    if(remlen) /*remaining message to be copied*/
    {
        unsigned char *orig_rptr = initmsg->b_rptr;
        initmsg->b_rptr = rptr;
        remmsg = pstreams_copymsg(strmhead, initmsg);
        initmsg->b_rptr = orig_rptr;
        if(!remmsg)
        {
            pstreams_freemsg(strmhead, msg);
            return NULL;
        }
        pstreams_linkb(msg, remmsg);
    }

#ifdef PSTREAMS_LT
    pstreams_log(NULL, PSTREAMS_LTDEBUG, "pstreams_msgpullup: bytes copied %d.",
            pstreams_msgsize(msg));
#endif /*PSTREAMS_LT*/

    return msg;
}

/******************************************************************************
Name: pstreams_linkb
Purpose: add given tailmsg to msg as a continuation
Parameters:
Caveats: 
******************************************************************************/
int
pstreams_linkb(P_MSGB *msg, P_MSGB *tailmsg)
{
    P_MSGB *msgiter=NULL;

    if(!msg)
    {
        return P_STREAMS_FAILURE;
    }

    msgiter = msg;
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
Name: pstreams_unlinkb
Purpose: unlink the first message block pointed to by msg; and return the rest.
Parameters:
Caveats: 
******************************************************************************/
P_MSGB *
pstreams_unlinkb(P_MSGB *msg)
{
    P_MSGB *head=NULL;
    P_MSGB *tail=NULL;

    if(msg)
    {
        tail = msg->b_cont;
        head = msg;
        head->b_cont = NULL; /*unlink the head*/
    }

    return tail;
}

/******************************************************************************
Name: pstreams_freemsg
Purpose: 
Parameters:
Caveats:
******************************************************************************/
void
pstreams_freemsg(P_STREAMHEAD *strmhead, P_MSGB *msg)
{

    if(msg)
    {
        ASSERT(msg != msg->b_cont);

        if(msg->b_cont)
        {
            /*recursively release*/
            pstreams_freemsg(strmhead, msg->b_cont);
        }

        PDBG(msg->b_cont = NULL);

        pstreams_freeb(strmhead, msg);
    }

    return;
}

/******************************************************************************
Name: pstreams_freeb
Purpose: 
Parameters:
Caveats:
******************************************************************************/
void
pstreams_freeb(P_STREAMHEAD *strmhead, P_MSGB *msg)
{
    ASSERT(msg->b_datap->db_ref > 0);
        
    /*decrement datablocks reference count*/
    msg->b_datap->db_ref--;

    if(msg->b_datap->db_ref == 0)
    {
        /*can free P_DATAB and associated data now*/
        if(msg->b_datap->db_frtnp)
        {
            /*external memory - all external memory needs to have frtnp set!*/
            void (*free_func)() = msg->b_datap->db_frtnp->free_func;
            char *arg=msg->b_datap->db_frtnp->free_arg;

            ASSERT(free_func);

            if(arg)
            {
                ((void (*)(void *))free_func)(arg);
            }
            else
            {
                ((void (*)(void *,int))free_func)(msg->b_datap->db_base, 
                    (int)(msg->b_datap->db_lim - msg->b_datap->db_base));
            }
        }
        else
        {
            /*this is pstreams allocated memory*/
            if(msg->b_datap->db_base)
            {
                if(msg->b_datap->db_base != msg->b_datap->FASTBUF) /*skip forFASTBUF*/
                {
                    int32 poolsize = 
                        pstreams_mpool((int)(msg->b_datap->db_lim - msg->b_datap->db_base));
                    pstreams_mem_free(strmhead, msg->b_datap->db_base, poolsize);
                }
                else
                {
                    ASSERT((int)(msg->b_datap->db_lim - msg->b_datap->db_base) == FASTBUFSIZE);
                }
            }
        }

        PDBG(memset(msg->b_datap, 0, sizeof(P_DATAB)));
        lop_release(strmhead->datapool, msg->b_datap);
    }

    PDBG(memset(msg, 0, sizeof(P_MSGB)));
    lop_release(strmhead->msgpool, msg);

    return;
}

/*for DEBUG only*/
P_BOOL
pstreams_comparemsg(P_STREAMHEAD *strm, P_MSGB *msg1, P_MSGB *msg2)
{
    P_MSGB *msg1_pullup=NULL;
    P_MSGB *msg2_pullup=NULL;
    int32 msg1_len = pstreams_msgsize(msg1);
    int32 msg2_len = pstreams_msgsize(msg2);
    P_BOOL cmpval=P_FALSE;

    if((msg1_len == msg2_len) && (msg1->b_band == msg2->b_band))
    {
        msg1_pullup = pstreams_msgpullup(strm, msg1, msg1_len);
        msg2_pullup = pstreams_msgpullup(strm, msg2, msg2_len);

        if(memcmp(msg1_pullup->b_rptr, msg2_pullup->b_rptr, msg1_len) == 0) /*remember only debugmode*/
        {
            cmpval = P_TRUE;
        }
    }

    pstreams_freemsg(strm, msg1_pullup);
    pstreams_freemsg(strm, msg2_pullup);

    return cmpval;
}

/*for DEBUG only*/
#define PGETLISTHDR(pobj)  ((LISTHDR *)((char *)(pobj) - sizeof(LISTHDR)))
int
pstreams_checkmsg(P_MSGB *msg)
{
    P_MSGB *mblk=NULL;

    for(mblk = msg; mblk; mblk = mblk->b_cont)
    {
        LISTHDR *plhdr = PGETLISTHDR(mblk);

        if(plhdr->pnext != NULL)
        {
            CONSOLEWRITE("pstreams_checkmsg: msgb=0x%lx, plhdr=0x%lx, plhdr->pnext=0x%lx", 
        mblk, plhdr, plhdr->pnext);
            CONSOLEWRITE("pstreams_checkmsg: msgpool's lowat = %ld", dbgstrm->msgpool->lowat);
            return P_STREAMS_FAILURE;
        }
        {
            P_DATAB *datab = mblk->b_datap;
            LISTHDR *plhdr = PGETLISTHDR(datab);
            if(plhdr->pnext != NULL)
            {
                CONSOLEWRITE("pstreams_checkmsg: datab=0x%lx, plhdr=0x%lx, plhdr->pnext=0x%lx", 
                datab, plhdr, plhdr->pnext);
                CONSOLEWRITE("pstreams_checkmsg: datapool's lowat = %ld", dbgstrm->datapool->lowat);
                return P_STREAMS_FAILURE;
            }
    }
    }
    return P_STREAMS_SUCCESS;
}

    /*DEBUGMODE*/
void
pstreams_memstats(P_STREAMHEAD *strm)
{
    CONSOLEWRITE("PSTREAMS MEMORY STATS:");
    CONSOLEWRITE("MEMORY: msgpool - lowat=%ld,freecount=%ld,count=%ld", 
    strm->msgpool->lowat, strm->msgpool->freecount, strm->msgpool->count);
    CONSOLEWRITE("MEMORY: datapool - lowat=%ld,freecount=%ld,count=%ld",
    strm->datapool->lowat, strm->datapool->freecount, strm->datapool->count);
#if(POOL16SIZE > 0)
    CONSOLEWRITE("MEMORY: pool16 - lowat=%ld,freecount=%ld,count=%ld", 
    strm->pool16->lowat, strm->pool16->freecount, strm->pool16->count);
#endif
#if(POOL64SIZE > 0)
     CONSOLEWRITE("MEMORY: pool64 - lowat=%ld,freecount=%ld,count=%ld", 
     strm->pool64->lowat, strm->pool64->freecount, strm->pool64->count);
#endif
#if(POOL256SIZE > 0)
     CONSOLEWRITE("MEMORY: pool256 - lowat=%ld,freecount=%ld,count=%ld", 
     strm->pool256->lowat, strm->pool256->freecount, strm->pool256->count);
#endif
#if(POOL512SIZE > 0)
     CONSOLEWRITE("MEMORY: pool512 - lowat=%ld,freecount=%ld,count=%ld", 
     strm->pool512->lowat, strm->pool512->freecount, strm->pool512->count);
#endif
#if(POOL1792SIZE > 0)
     CONSOLEWRITE("MEMORY: pool1792 - lowat=%ld,freecount=%ld,count=%ld", 
     strm->pool1792->lowat, strm->pool1792->freecount, strm->pool1792->count);
#endif
}

/*END pstreams.c*/
