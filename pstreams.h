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
#include "ptypes.h"
#include "listop.h" 

enum P_DEFINES
{
	MAXQUEUES=1000, MAXMSGBS=2048, MAXDATABS=2048,
		MAXDATABSIZE=2048, FASTBUF=2048, MAXFILENAMESIZE=255
};

/*log and trace codes passed to pstreams_log()*/
/*
 *Note on using the codes:
 * There are 4 cateogries, debug, info, warning and error.
 * Each of the 4 categories have a code set of 3 codes associated with it.
 * The the 3 codes in each code set are consecutive integers
 * The higher the integer value of the code the more critical it is
 * The mid value of the code set is assigned to that category, for ex.
 *	error code set has 10, 11 and 12 and the value of 
 *	PSTREAMS_LTERROR is 11
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

/*bit flags representing state of stream queue*/
enum P_QFLAG
{
	QRESET = 0x0000, 
	QENAB  = 0x0001, 
	QWANTR = 0x0002, 
	QWANTW = 0x0004, 
	QFULL  = 0x0010, 
	QNOENB = 0x0020
};

typedef enum Boolean 
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
	ushort mi_idnum;	/*module ID number*/
	char *mi_idname;	/*module ID name*/
	short mi_minpsz;	/*default min pdu size in bytes*/
	short mi_maxpsz;	/*default max pdu size* in bytes*/
	ushort mi_hiwat;	/*default bytes for 'high water' level - flow control*/
	ushort mi_lowat;	/*default bytes for 'low water' level - flow control*/
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
	/*...my add-ons*/
	P_M_DELIM /*DELIM == BREAK ???*/
	/*...priority message types*/
} P_M_TYPES;

/*
 * IOCTL commands.
 * While this looks like it doesn't belong here, I couldn't
 * find any other place for it. Not the UDP* stuff; hence
 * oxproto.h is out. Also since the values need to be distinct
 * they cannot be split
 */
enum p_ioctl_cmds
{
	UDPDEV_RADDR, 
	UDPDEV_LADDR, 
	TCPDEV_RADDR,
	TCPDEV_LADDR,
	TCPDEV_BIND,
	TCPDEV_CONNECT,
	TCPDEV_DISCONNECT,
	TCPDEV_CLOSE
};


/*should hold a function used to free related memory*/
typedef struct p_free_rtn
{
	int unused;
} P_FREE_RTN;

/*
 *DATA blocks - blocks of different sizes, 
 *each in its own pool, would be available.
 *Data blocks are associated with one or more
 *MESSAGE blocks.
 */
typedef struct p_datab
{
	P_FREE_RTN	*db_frtnp; /*unused*/
	unsigned char	*db_base;
	unsigned char	*db_lim;
	unsigned char	db_ref; /*number of MESSAGE blocks referencing this*/
	P_M_TYPES		db_type;
	unsigned char	db_flags; /*unused*/
	struct msgb	*db_msgaddr; /*unused - backptr to MSGB*/

	unsigned char data[MAXDATABSIZE]; /*TODO - for now fixed at one size*/
} P_DATAB;

/*MESSAGE block - each block is associated with one DATA block. 
 *Message blocks can be linked together to denote a logical connection -
 *for example, protocol header in one block followed by the message
 *body in another
 */
typedef struct p_msgb
{
	struct p_msgb *b_next; /*unused - using listop features instead*/
	struct p_msgb *b_prev; /*unused*/
	struct p_msgb *b_cont; 
	unsigned char *b_rptr;
	unsigned char *b_wptr;

	P_DATAB *b_datap;
} P_MSGB;

#ifdef DEADCODE
typedef struct p_mbinfo
{
	P_MSGB *m_mblock; /*message header ptr*/
	void (*m_func)(); /*fn. that allocated message header*/
} P_MBINFO;

typedef struct p_dbinfo
{
	P_DATAB *d_dblock; /*data block part of the structure*/
} P_DBINFO;

typedef struct p_mdbblock
{
	P_MBINFO msgblock; /*OR should this be a pointer - TODO*/
	P_DBINFO datablock;/*OR should this be a pointer - TODO*/
	char databuf[FASTBUF]; /*FASTBUF such that sizeof(p_mdbock) is 128*/
} P_MDBBLOCK;
#endif

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
	void *q_ptr;	/*private data store*/
	ushort q_count; /*count of outstanding bytes queued*/
	ushort q_flag;	/*state of queue - treated as a bit flag*/
	short  q_minpsz; /*min packet size - unused*/
	short q_maxpsz; /*max packet size - unused*/
	ushort q_hiwat; /*hi water mark - in bytes*/
	ushort q_lowat; /*lo water mark - in bytes*/
	P_BOOL q_enabled; /*special - TRUE if enabled for srvp*/
	P_LTCODE ltfilter; /*log trace filter - higher => more restrictive*/
} P_QUEUE;

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

	/*various memory pools*/
	POOLHDR *msgpool;
	POOLHDR *datapool;
	POOLHDR *qpool;
	POOLHDR *modinfopool;
	POOLHDR *mdbpool;

	/*log and trace output filename*/
	char ltfname[MAXFILENAMESIZE];
	FILE *ltfile;

} P_STREAMHEAD;

typedef struct p_buf /*like strbuf in stropts.h*/
{
	int maxlen;   /* not used here */
	int len;      /* length of data */
	void *buf;    /* pointer to buffer */
} P_BUF;

/*
 * User level ioctl format for ioctls that go downstream - 
 * like strioctl in stropts.h
 */
typedef struct p_ioctl
{
	int ic_cmd;		/*command*/
	int ic_timeout;		/*timeout value*/
	int ic_len;		/*length of following data*/
	unsigned char *ic_dp;		/*data pointer*/
} P_IOCTL;

/*function prototypes*/

/*public functions*/
P_STREAMHEAD *
pstreams_open(int devid);
int
pstreams_close(P_STREAMHEAD *strmhead);
int
pstreams_push(P_STREAMHEAD *strmhead, const P_STREAMTAB *mod);
int
pstreams_pop(P_STREAMHEAD *strmhead);
int
pstreams_putmsg(P_STREAMHEAD *strmhead, P_BUF *ctlbuf, P_BUF *msgbuf, int flags);
int
pstreams_getmsg(P_STREAMHEAD *strmhead, P_BUF *ctlbuf, P_BUF *msgbuf, int*pflags);

/*private functions*/
uint
pstreams_msgread(P_BUF *wbuf, P_MSGB *msg);
uint
pstreams_msgwrite(P_MSGB *msg, P_BUF *rbuf);

/*adjmsg - trims bytes from the front of back of a message*/
int
pstreams_msgconsume(P_MSGB *msg, uint bytes);
int
pstreams_msgerase(P_MSGB *msg, uint bytes);
int
pstreams_msg1erase(P_MSGB *msg, uint bytes);

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
int pstreams_canput(P_QUEUE *q);
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

/*msgdsize - number of bytes in M_DATA blocks attached to a message*/
ushort 
pstreams_msg1size(P_MSGB *msg);
ushort 
pstreams_msgsize(P_MSGB *msg);

P_STREAMHEAD *
pstreams_get_strmhead(P_QUEUE *q);
void
pstreams_put_strmhead(P_QUEUE *q, P_STREAMHEAD *strmhead);
uint
pstreams_unwrit1bytes(P_MSGB *msg);
uint
pstreams_unwritbytes(P_MSGB *msg);

/*copyb, copymsg - allocates and copies*/
int
pstreams_msg1cpy(P_MSGB *to, P_MSGB *from, uint bytestocopy);
int
pstreams_msgcpy(P_MSGB *to, P_MSGB *from, uint bytestocopy);
P_MSGB *
pstreams_allocmsgb_copy(P_STREAMHEAD *strmhead, P_MSGB *initmsg);

int
pstreams_garbagecollect(P_QUEUE *q, P_MSGB **msg);

/*linkb - links a message to the b_cont pointer of another message*/
int
pstreams_addmsg(P_MSGB **msg, P_MSGB *tailmsg);

int
pstreams_sift(P_QUEUE *q, P_MSGB *msg, int (*sift)(P_QUEUE *q, P_MSGB *msg), 
		P_MSGB **msglist1, P_MSGB **msglist2);
int
pstreams_ctlexpress(P_QUEUE *q, P_MSGB *msg, P_BOOL (*myioctl)(P_QUEUE *, P_MSGB *),
					P_MSGB **ctlmsgs, P_MSGB **datmsgs);

/*strlog - passes message to the STREAMS logging device*/
int
pstreams_log(P_QUEUE *q, P_LTCODE ltcode, const char *fmt,...);

P_BOOL
pstreams_ltfilter(P_QUEUE *q, P_LTCODE ltcode);
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
#endif
