/*===========================================================================
FILE: pstreams.h

   Portable and Pure streams. Routines implementing a 'unix streams' inspired
design for layered protocols


 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.

===========================================================================*/

#ifndef PSTREAMS_H
#define PSTREAMS_H
#include <stdio.h>
#include "listop.h" 
#include "options.h"

/*control to activate DEBUG mode statements*/
#ifdef PDBG_ON
#define PDBG(x) x
#else
#define PDBG(x) 
#endif

/*
 * by default DEBUG TRACE (entry and exit log of 
 * functions) is on - disable it in modules' *.c files
 */
#define PDBGTRACE(x) pstreams_console(x)

#define RD(q) (((q)->q_flag & QREADR) ? (q) : (q)->q_peer)
#define WR(q) RD(q)->q_peer

#define PSTRMHEAD(q) ((P_STREAMHEAD *)((q)->strmhead))


enum P_DEFINES
{
    MAXQUEUES=12,
    MAXMSGBS=352, 
    MAXDATABS=320,
    FASTBUFSIZE=4, /*4 bytes*/ 
    MAXDATABSIZE=2048, /*used for debugmode sanity checks*/
    MAXFILENAMESIZE=255
};

enum P_ERRORCODE
{
    P_NOERROR=0,
    P_OUTOFMEMORY,
    P_READBUF_TOOSMALL,
    P_BUSY,  /*flow control restriction - temporary*/
    P_GENERALERROR
};

/*log and trace codes passed to pstreams_log()*/
/*
 *Note on using the codes:
 * There are 4 cateogries, debug, info, warning and error.
 * Each of the 4 categories have a code set of 3 codes associated with it.
 * The the 3 codes in each code set are consecutive integers
 * The higher the integer value of the code the more critical it is
 * The mid value of the code set is assigned to that category, for ex.
 *    error code set has 10, 11 and 12 and the value of 
 *    PSTREAMS_LTERROR is 11
 * To vary criticality within a category use +1 or -1 to the mid value,
 * for ex. PSTREAMS_LTERROR-1, PSTREAMS_LTERROR and PSTREAMS_LTERROR+1
 */
typedef enum pstreamsltcode
{
    PSTREAMS_LTMIN,

    PSTREAMS_LT1,
    PSTREAMS_LT2, /*debug*/
    PSTREAMS_LT3,

    PSTREAMS_LT4,
    PSTREAMS_LT5, /*info*/
    PSTREAMS_LT6,

    PSTREAMS_LT7,
    PSTREAMS_LT8, /*warning*/
    PSTREAMS_LT9,

    PSTREAMS_LT10,
    PSTREAMS_LT11, /*error*/
    PSTREAMS_LT12,

    PSTREAMS_LTMAX
} P_LTCODE;
#define PSTREAMS_LTDEBUG PSTREAMS_LT2
#define PSTREAMS_LTINFO  PSTREAMS_LT5
#define PSTREAMS_LTWARNING PSTREAMS_LT8
#define PSTREAMS_LTERROR PSTREAMS_LT11

/*log and trace filter codes*/
#define PSTREAMS_LTOFF PSTREAMS_LTMAX
#define PSTREAMS_LTALL PSTREAMS_LTMIN

/*flag used as function parameters*/
enum
{
    PSTREAMS_BLOCK=0,
    PSTREAM_NOBLOCK=1
};

/*bit flags representing state of stream queue*/
enum P_QFLAG
{
    QRESET = 0x0000, 
    QENAB  = 0x0001, 
    QWANTR = 0x0002, 
    QWANTW = 0x0004, 
    QFULL  = 0x0008,
    QREADR = 0x0010, 
    QNOENB = 0x0040
};

/*recv/send (RS) priority message flags.
 *used in flags of pstreams_putmsg()/_esmsgput()
 */
typedef enum rs_band
{
    RS_HIPRI=0x01
} RS_BAND;

typedef enum p_boolean 
{
    P_FALSE=0, P_TRUE
} P_BOOL; 

enum P_GETVALCODE
{
    GETMSGCNT
};

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

/*module specific information*/
typedef struct pmodule_info
{
    ushort mi_idnum;    /*module ID number*/
    char *mi_idname;    /*module ID name*/
    short mi_minpsz;    /*default min pdu size in bytes*/
    short mi_maxpsz;    /*default max pdu size* in bytes*/
    ushort mi_hiwat;    /*default bytes for 'high water' level - flow control*/
    ushort mi_lowat;    /*default bytes for 'low water' level - flow control*/
} P_MODINFO;

/*module related statistics*/
typedef struct pmodule_stat
{
    int uuused;
} P_MODSTAT;

typedef struct p_getval
{
    int type;
    union
    {
        int msgcount;
    } val;
} P_GETVAL;

#ifdef M2STRICTTYPES
struct p_queue;
struct p_msgb;
#endif

/*initialization struct for queue - this is crucial*/
typedef struct p_qinit
{
#ifdef M2STRICTTYPES
    int (*qi_putp)(struct p_queue *, struct p_msgb *); /*ptr to put proc*/
    int (*qi_srvp)(struct p_queue *); /*ptr to service proc*/
    int (*qi_qopen)(struct p_queue *); /*ptr to proc. called when module is opened or pushed*/
    int (*qi_qclose)(struct p_queue *); /*ptr to proc. called when module is closed or popped*/
    int (*qi_mchk)(struct p_queue *, struct p_getval *); /*special - debug mode hook*/
#else
    int (*qi_putp)(); /*ptr to put proc*/
    int (*qi_srvp)(); /*ptr to service proc*/
    int (*qi_qopen)(); /*ptr to proc. called when module is opened or pushed*/
    int (*qi_qclose)(); /*ptr to proc. called when module is closed or popped*/
    int (*qi_mchk)(); /*special - debug mode hook*/
#endif
    P_MODINFO *qi_minfo; /*module specific default values*/
    P_MODSTAT *qi_mstat; /*collect statistics for this queue's module*/
} P_QINIT;

/*this is what represents a module*/
typedef struct p_streamtab 
{
    P_QINIT *st_rdinit;    /* read QUEUE */
    P_QINIT *st_wrinit;    /* write QUEUE */
    P_QINIT *st_muxrinit;  /* lower read QUEUE for MUX - unused*/
    P_QINIT *st_muxwinit;  /* lower write QUEUE for MUX - unused*/
} P_STREAMTAB;


enum P_STREAMS_STATUS
{
    P_STREAMS_SUCCESS=0,
    P_STREAMS_FAILURE,
    P_STREAMS_ERROR,
    P_STREAMS_INVALID
};

/*the devices this stream can interface to*/    
typedef enum pstreamsdevid 
{
    P_NULL, P_TCP, P_UDP
} P_STREAMS_DEVID;

/*message types*/
typedef enum pmtypes
{
    P_M_DATA, P_M_PROTO, P_M_BREAK, P_M_CTL, P_M_DELAY, 
    P_M_IOCTL, P_M_PASSFP, P_M_RSE, P_M_SETOPTS, P_M_SIG,
    P_M_ERROR,
    /*...my add-ons*/
    P_M_DELIM /*DELIM == BREAK ???*/
    /*...priority message types*/
} P_M_TYPES;

/*
 * CTL commands - all values less than 3 digits are reserved
 * for pstreams devices.
 * While this looks like it doesn't belong here, I couldn't
 * find any other place for it. Not the UDP* stuff; hence
 * o2k.h is out. Also since the values need to be distinct
 * they cannot be split
 */
typedef enum p_ctl_cmds
{
    UDPDEV_RADDR=01, 
    UDPDEV_LADDR, 
    UDPDEV_SHAREFADDR,

    TCPDEV_RADDR,
    TCPDEV_LADDR,
    TCPDEV_BIND,
    TCPDEV_CONNECT,
    TCPDEV_DISCONNECT,
    TCPDEV_CLOSE
} P_CTLCODE;


/*should hold a function used to free related memory*/
typedef struct p_free_rtn
{
    void (*free_func)();
    char *free_arg;
} P_FREE_RTN;

/*
 *DATA blocks - blocks of different sizes, 
 *each in its own pool, would be available.
 *Data blocks are associated with one or more
 *MESSAGE blocks.
 */
typedef struct p_datab
{
    P_FREE_RTN    *db_frtnp;
    unsigned char    *db_base;
    unsigned char    *db_lim;
    unsigned char    db_ref; /*number of MESSAGE blocks referencing this*/
#ifndef PSTREAMS_LEAN
    unsigned char    db_flags; /*unused*/
#endif
    unsigned char    db_type; /*of type P_M_TYPES*/
#ifndef PSTREAMS_LEAN
    struct msgb    *db_msgaddr; /*unused - backptr to MSGB*/
#endif
    unsigned char FASTBUF[FASTBUFSIZE]; /*small built-in buffer*/
} P_DATAB;

/*MESSAGE block - each block is associated with one DATA block. 
 *Message blocks can be linked together to denote a logical connection -
 *for example, protocol header in one block followed by the message
 *body in another
 */
typedef struct p_msgb
{
#ifndef PSTREAMS_LEAN
    struct p_msgb *b_next; /*unused - using listop features instead*/
    struct p_msgb *b_prev; /*unused*/
#endif
    struct p_msgb *b_cont; /*next message block in logical message*/
    unsigned char *b_rptr; /*1st unread data byte of buffer*/
    unsigned char *b_wptr; /*1st unwritten data byte of buffer*/
    P_DATAB    *b_datap;        /*data block*/
    unsigned char    b_band; /*message priority*/
#ifndef PSTREAMS_LEAN
    unsigned short    b_flag; /*used by streamhead - unused now*/
#endif
} P_MSGB;

/*the queue itself*/
typedef struct p_queue
{
    P_QINIT q_qinfo; /*info on processing routines for queue*/

    LISTHDR *q_msglist; /*queue of messages - whence the name*/
    void *strmhead;     /*pointer to its streamhead*//*TODO remove void * */

    struct p_queue *q_next; /*next queue downstream*/
    struct p_queue *q_peer; /*special - to satisfy the condition 
                              *"that the routines associated with 
                              *one half of a stream module may find 
                              *the queue associated with the other half"*/
    void *q_ptr;    /*private data store*/
    ushort q_count; /*count of outstanding bytes queued*/
    ushort q_flag;    /*state of queue - treated as a bit flag*/
    short  q_minpsz; /*min packet size - unused*/
    short q_maxpsz; /*max packet size - unused*/
    ushort q_hiwat; /*hi water mark - in bytes*/
    ushort q_lowat; /*lo water mark - in bytes*/
    P_BOOL q_enabled; /*special - TRUE if enabled for srvp*/
    P_LTCODE ltfilter; /*log trace filter - higher => more restrictive*/
} P_QUEUE;

typedef struct p_buf /*like strbuf in stropts.h*/
{
    int maxlen;   /* size of buf below  - used for reading*/
    int len;      /* length of data in buf below*/
    char *buf;    /* pointer to buffer */
} P_BUF;

typedef struct p_esbuf /*like strbuf in stropts.h*/
{
    int maxlen;   /* not used here */
    int len;      /* length of data */
    char *buf;    /* pointer to buffer */
    P_FREE_RTN *fr_rtnp; /*used to free buf*/
} P_ESBUF;

/*internal definition*/
typedef struct p_mem 
{
    char *base;       /* start address*/
    char *limit;   /* end address*/
    void *buf;        /* pointer to buffer */
} P_MEM;

typedef struct p_streamhead /*my own*/
{
#ifdef M2STRICTTYPES
    P_STREAMS_DEVID devid;
#else
    int devid;
#endif
    P_STREAMTAB appmod;
    P_STREAMTAB devmod;
    P_QUEUE appwrq;
    P_QUEUE apprdq;
    P_QUEUE devwrq;
    P_QUEUE devrdq;

    /*memory buffers*/
    P_MEM *mem;
    P_MEM *pmem;

    /*various memory pools*/
    POOLHDR *msgpool;
    POOLHDR *datapool;
    POOLHDR *qpool;
#if(POOL16SIZE > 0)
    POOLHDR *pool16;
#endif
#if(POOL64SIZE > 0)
    POOLHDR *pool64;
#endif
#if(POOL256SIZE > 0)
    POOLHDR *pool256;
#endif
#if(POOL512SIZE > 0)
    POOLHDR *pool512;
#endif
#if(POOL1792SIZE > 0)
    POOLHDR *pool1792;
#endif

    /*takes the place of errno in unix systems*/
    uint16 perrno; /*holds last error*/

    /*log and trace output filename*/
    LOGFILE *ltfile;
    char ltfname[MAXFILENAMESIZE];
} P_STREAMHEAD;

/*
 * User level ioctl format for ioctls that go downstream - 
 * like strioctl in stropts.h
 */
typedef struct p_ioctl
{
    int ic_cmd;        /*command*/
    int ic_timeout;        /*timeout value*/
    int ic_len;        /*length of following data*/
    uchar *ic_dp;        /*data pointer*/
} P_IOCTL;

/*function prototypes*/

/*public functions*/
P_STREAMHEAD *
pstreams_open(int devid, P_MEM *mem, P_MEM *pmem);
int
pstreams_close(P_STREAMHEAD *strmhead);
int
pstreams_push(P_STREAMHEAD *strmhead, const P_STREAMTAB *mod);
int
pstreams_pop(P_STREAMHEAD *strmhead);
int
pstreams_putmsg(P_STREAMHEAD *strmhead, P_BUF *ctlbuf, P_BUF *msgbuf, int flags);
int
pstreams_esmsgput(P_STREAMHEAD *strmhead, P_ESBUF *ctlbuf, P_ESBUF *msgbuf, int flags);
int
pstreams_getmsg(P_STREAMHEAD *strmhead, P_BUF *ctlbuf, P_BUF *msgbuf, int*pflags);
int
pstreams_msgcount(P_STREAMHEAD *strmhead);

/*private functions*/
uint
pstreams_msgread(P_BUF *wbuf, P_MSGB *msg);
uint
pstreams_msgwrite(P_MSGB *msg, P_BUF *rbuf);

/*adjmsg - trims bytes from the front of back of a message*/
int
pstreams_msgconsume(P_MSGB *msg, uint32 bytes);
int
pstreams_msgerase(P_MSGB *msg, uint32 bytes);
int
pstreams_msg1erase(P_MSGB *msg, uint32 bytes);

P_MSGB *
pstreams_allocmsgb(P_STREAMHEAD *strmhead, int size, unsigned int priority);
int
pstreams_putctl(P_QUEUE *q, int ctlflag);
int
pstreams_putnext(P_QUEUE *wrq, P_MSGB *msg);
int
pstreams_putq(P_QUEUE *q, P_MSGB *msg);
int 
pstreams_putbq(P_QUEUE *q, P_MSGB *msg);
P_MSGB *
pstreams_getq(P_QUEUE *q);
int
pstreams_canput(P_QUEUE *q);
int     
pstreams_init_queue(P_STREAMHEAD *strmhead, P_QUEUE *q, P_QINIT *qi);
int
pstreams_connect_queue(P_QUEUE *inq, P_QUEUE *outq);
int
pstreams_relmsg(P_QUEUE *q, P_MSGB *msg);
int
pstreams_callsrvp(P_STREAMHEAD *strmhead);
int
pstreams_srvp(P_QUEUE *q);
int pstreams_qsize(P_QUEUE *q);

/*msgdsize - number of bytes in M_DATA blocks attached to a message*/
ushort 
pstreams_msg1size(P_MSGB *msg);
ushort 
pstreams_msgsize(P_MSGB *msg);

void
pstreams_put_strmhead(P_QUEUE *q, P_STREAMHEAD *strmhead);
uint32
pstreams_unwrit1bytes(P_MSGB *msg);
uint32
pstreams_unwritbytes(P_MSGB *msg);

/*copyb, copymsg - allocates and copies*/
int
pstreams_msg1cpy(P_MSGB *to, P_MSGB *from, uint bytestocopy);
int
pstreams_msgcpy(P_MSGB *to, P_MSGB *from, uint32 bytestocopy);

int
pstreams_garbagecollect(P_QUEUE *q, P_MSGB **msg);

/*linkb - links a message to the b_cont pointer of another message*/
int
pstreams_addmsg(P_MSGB **msg, P_MSGB *tailmsg);

int
ctl_or_data(P_QUEUE *q, P_MSGB *msg);
int
pstreams_sift(P_QUEUE *q, P_MSGB *msg, int (*sift)(P_QUEUE *q, P_MSGB *msg), 
        P_MSGB **msglist1, P_MSGB **msglist2);
int
pstreams_ctlexpress(P_QUEUE *q, P_MSGB *msg, P_BOOL (*myctl)(P_QUEUE *, P_MSGB *),
                    P_MSGB **ctlmsgs, P_MSGB **datmsgs);

int
pstreams_console(const char *fmt,...);

/*strlog - passes message to the STREAMS logging device*/
int
pstreams_log(P_QUEUE *q, P_LTCODE ltcode, const char *fmt,...);

P_BOOL
pstreams_ltfilter(P_QUEUE *q, P_LTCODE ltcode);
void
pstreams_flushlog(P_STREAMHEAD *strm);
FILE *
pstreams_setltfile(P_STREAMHEAD *strmhead, char *ltfilename);
int
pstreams_mchk(P_QUEUE *q, P_GETVAL *get);
int
pstreams_checkmem(P_STREAMHEAD *strmhead);

/*qsize - counts the number of messages on a queue - not the bytes*/
int
pstreams_countmsgcont(P_MSGB *msg);
int
pstreams_countmsg(LISTHDR *msglist);

void *
pstreams_memassign(P_MEM *mem, int32 size);
P_MSGB *
pstreams_allocb(P_STREAMHEAD *strmhead, int32 size, uint priority);
P_MSGB *
pstreams_esballoc(P_STREAMHEAD *strmhead, unsigned char *base, 
                  int32 size, int pri, P_FREE_RTN *free_rtn);
int32
pstreams_mpool(int32 size);
unsigned char *
pstreams_mem_alloc(P_STREAMHEAD *strmhead, int32 size, int flag);
void
pstreams_mem_free(P_STREAMHEAD *strmhead, void *buf, int32 size);
P_MSGB *
pstreams_dupmsg(P_STREAMHEAD *strmhead, P_MSGB *initmsg);
P_MSGB *
pstreams_dupb(P_STREAMHEAD *strmhead, P_MSGB *initmsg);
P_MSGB *
pstreams_dupnmsg(P_STREAMHEAD *strmhead, P_MSGB *initmsg, int32 len);
P_MSGB *
pstreams_copyb(P_STREAMHEAD *strmhead, P_MSGB *initmsg);
P_MSGB *
pstreams_copymsg(P_STREAMHEAD *strmhead, P_MSGB *initmsg);
P_MSGB *
pstreams_msgpullup(P_STREAMHEAD *strmhead, P_MSGB *initmsg, int32 len);
int
pstreams_linkb(P_MSGB *msg, P_MSGB *tailmsg);
P_MSGB *
pstreams_unlinkb(P_MSGB *msg);
void
pstreams_freeb(P_STREAMHEAD *strmhead, P_MSGB *msg);
void
pstreams_freemsg(P_STREAMHEAD *strmhead, P_MSGB *msg);
P_BOOL
pstreams_comparemsg(P_STREAMHEAD *strm, P_MSGB *msg1, P_MSGB *msg2); /*strictly for debug*/
int
pstreams_checkmsg(P_MSGB *msg);
void
pstreams_memstats(P_STREAMHEAD *strm);
#endif
