#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include "options.h"
#include "env.h"
#include "assert.h"
#include "pstreams.h"
#include "stdmod.h"
#include "pstreams_echo.h"
#include "saw.h"
#include "util.h"
#include "testutil.h"


void my_dummyfree(char *ptr);
#ifdef PSTREAMS_ECHO
extern P_STREAMTAB echo_streamtab;
#endif
extern P_STREAMTAB saw_streamtab;

#define MAXRMSGS 1
#define MSGSIZE 32

/*public*/
FILE *ltfile=NULL;
char ltfilename[] = "testlog.txt";

/*private globals*/
char vmem_region[VMEMSIZE]={0};
char pmem_region[PMEMSIZE]={0};
MY_PROTO proto;

#define LOOPBACKPORT 3000
#define LOOPBACKIP "127.0.0.1"


#define MAXBUFSIZE 2048

/*buffers for put/get of ctl/data*/
unsigned char putdata[MAXBUFSIZE] = {0};
unsigned char getdata[MAXBUFSIZE]={0};
unsigned char putctl[MAXBUFSIZE]={0};
unsigned char getctl[MAXBUFSIZE]={0};

/*
 * buffer holders for put/get of ctl/data 
 *   - assigning maxlen, len and buf respectively
 */
P_BUF    putdbuf = {sizeof(putdata), 0, (char *)putdata};
P_BUF    getdbuf={sizeof(getdata), 0, (char *)getdata};
P_BUF    putcbuf={sizeof(putctl), 0, (char *)putctl};
P_BUF    getcbuf={sizeof(getctl), 0, (char *)getctl};

/******************************************************************************
Name: init_test
Purpose: 
Parameters:
Caveats:
******************************************************************************/
void
init_test()
{
    int ii=0;

    ltfile=NULL;

/*private globals*/
    memset(vmem_region, 0, VMEMSIZE);
    memset(pmem_region, 0, PMEMSIZE);

    init_global_buffers();

    return;
}

void
init_global_buffers()
{
    putdbuf.buf = (char *)putdata;
    putdbuf.len = 0;
    putdbuf.maxlen = MAXBUFSIZE;

    getdbuf.buf = (char *)getdata;
    getdbuf.len = 0;
    getdbuf.maxlen = MAXBUFSIZE;

    putcbuf.buf = (char *)putctl;
    putcbuf.len = 0;
    putcbuf.maxlen = MAXBUFSIZE;

    getcbuf.buf = (char *)getctl;
    getcbuf.len = 0;
    getcbuf.maxlen = MAXBUFSIZE;

/*buffers for put/get of ctl/data*/
    memset(putdbuf.buf, 0, MAXBUFSIZE);
    memset(getdbuf.buf, 0, MAXBUFSIZE);
    memset(putcbuf.buf, 0, MAXBUFSIZE);
    memset(getcbuf.buf, 0, MAXBUFSIZE);
}

/******************************************************************************
Name: 
Purpose: 
Parameters:
Caveats:
******************************************************************************/
P_STREAMHEAD *
buildstream()
{
    P_STREAMHEAD *strm=NULL;
    P_MEM vmem={0};
    P_MEM pmem={0};

#ifdef PSTREAMS_ECHO
    if(echo_init() != P_STREAMS_SUCCESS)
    {
        ASSERT(0);
    };
#endif

	saw_init();

    /*allocate local memory for pstreams*/
    vmem.buf = vmem_region;
    vmem.base = (char *)vmem.buf;
    vmem.limit = vmem.base + VMEMSIZE;

    /*allocate persistent memory for pstreams*/
    pmem.buf = pmem_region;
    pmem.base = (char *)pmem.buf;
    pmem.limit = pmem.base + PMEMSIZE;

#ifdef PSTREAMS_UDP
    strm = pstreams_open(P_UDP, &vmem, &pmem);
#else
    strm = pstreams_open(P_NULL, &vmem, &pmem);
#endif

    ASSERT(strm);

    /*do this block only if you have file IO*/
    ltfile = pstreams_setltfile(strm, ltfilename);
    if(!ltfile)
    {
        CONSOLEWRITE("Cannot open logtrace file %s. console log? maybe!\n",
            ltfilename);
    }

    CONSOLEWRITE("\nOpened stream...\n");

#ifdef PSTREAMS_ECHO
    CONSOLEWRITE("\nPushing PSTREAMS_ECHO module in...\n");
    if(pstreams_push(strm, &echo_streamtab) != P_STREAMS_SUCCESS)
    {
        ASSERT(0);
    }
#else
    {
        /*
         * This block sets up the UDP transport parameters
         */
        struct sockaddr_in sockaddr={0};
        MY_PROTO proto={0};

        proto.ctlfunc = UDPDEV_RADDR;
        memcpy(putcbuf.buf, &proto, sizeof(MY_PROTO));
        putcbuf.len = sizeof(MY_PROTO);

        sockaddr.sin_family = AF_INET;

        sockaddr.sin_port = p_htons(LOOPBACKPORT);
        sockaddr.sin_addr.s_addr = inet_addr(LOOPBACKIP);

        memcpy(&putcbuf.buf[putcbuf.len], &sockaddr, sizeof(struct sockaddr_in));
        putcbuf.len += sizeof(struct sockaddr_in);
    
        lop_checkpool(strm->msgpool);

        pstreams_putmsg(strm, &putcbuf, NULL, RS_HIPRI);

        proto.ctlfunc = UDPDEV_LADDR;
        memcpy(putcbuf.buf, &proto, sizeof(MY_PROTO));
        putcbuf.len = sizeof(MY_PROTO);

        sockaddr.sin_family = AF_INET;

        sockaddr.sin_port = p_htons(LOOPBACKPORT);
        sockaddr.sin_addr.s_addr = p_htonl(INADDR_ANY);

        memcpy(&putcbuf.buf[putcbuf.len], &sockaddr, sizeof(struct sockaddr_in));
        putcbuf.len += sizeof(struct sockaddr_in);
    
        lop_checkpool(strm->msgpool);

        if(pstreams_putmsg(strm, &putcbuf, NULL, RS_HIPRI) != P_STREAMS_SUCCESS)
        {
            ASSERT(0);
        }
    }
#endif

	CONSOLEWRITE("\nPushing SAW module in...\n");
    if(pstreams_push(strm, &saw_streamtab) != P_STREAMS_SUCCESS)
    {
        ASSERT(0);
    }

    return strm;
}

void
my_dummyfree(char *ptr)
{
    ptr=NULL;
}


/******************************************************************************
Name: echotest
Purpose: sends byte streams and expects to read the same. 
Parameters:
Caveats:
******************************************************************************/
int
echotest(P_STREAMHEAD *strm, int msgCount)
{
    int loopcount;
	int countMsgSent = 0;
	int countMsgReceived = 0;


	for(loopcount=10000*msgCount; loopcount>0; loopcount--)
	{
		if(countMsgSent < msgCount)
		{
			countMsgSent += send_echomsg(strm);
		}

		pstreams_callsrvp(strm);

		countMsgReceived += rcv_echomsg(strm);

		if(strm->perrno == P_BUSY)
		{
			Sleep(100); //sleep a while to let UDP/IP to catch up
		}

		if(countMsgReceived == msgCount)
		{
			break;
		}

	}

    CONSOLEWRITE("RESULT: Msg Sent=%d\tMsg Received=%d\n", 
		countMsgSent, countMsgReceived);

    return 0;
}

/*
 *return number of messages sent - which is 1 or 0
 */
int
send_echomsg(P_STREAMHEAD *strm)
{
    int ii;

    char c='0';
    for(ii=putdbuf.len; ii<MSGSIZE; ii++)
    {
        putdbuf.buf[ii]=c;
        c++;
        if(c > '9')
        {
            c = '0';
        }
    }
    putdbuf.buf[MSGSIZE]='\0';
    putdbuf.len = MSGSIZE;

    {
        P_ESBUF esputdbuf;
        static P_FREE_RTN frtn = {0};

        frtn.free_func = (void (*)())my_dummyfree;

        esputdbuf.buf = putdbuf.buf;
        esputdbuf.len = putdbuf.len;
        esputdbuf.maxlen = putdbuf.maxlen;
        esputdbuf.fr_rtnp = &frtn;

		strm->perrno = 0; /*reset errno*/
       if(pstreams_putmsg(strm, NULL, &putdbuf, 0) != P_STREAMS_SUCCESS)
        /*if(pstreams_esmsgput (strm, NULL, &esputdbuf, 0) != P_STREAMS_SUCCESS)*/
        {
		   if(strm->perrno != P_BUSY)
		   {
				CONSOLEWRITE("\npstreams_esmsgput: couldn't send return msg\n");
		   }
			return 0;
        }
    }

	CONSOLEWRITE("\nSent %d bytes\n", MSGSIZE);
	return 1;
}

/*
 * returns number of messages received
 */
int
rcv_echomsg(P_STREAMHEAD *strm)
{
	int rxMsgs=0; /*received messages count*/

    pstreams_getmsg(strm, &getcbuf, &getdbuf, 0);

    if(getcbuf.len > 0)
    {
        assert(0);
    }

    if(getdbuf.len > 0)
    {
        rxMsgs=1;
        if((getdbuf.len == putdbuf.len) && !memcmp(getdbuf.buf, putdbuf.buf, putdbuf.len))
        {
            CONSOLEWRITE("RESULT: Success. Matched %d bytes\n", putdbuf.len);
        }
        else
        {
            CONSOLEWRITE("RESULT: Failed. Match for %d bytes\n", putdbuf.len);
        }
        
    }

	if(rxMsgs > 0)
	{
		rxMsgs += rcv_echomsg(strm);
	}

    return rxMsgs;
}
    
/******************************************************************************
Name: 
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
service_strm(P_STREAMHEAD *strm)
{
    int getflags=0;
    
    CONSOLEWRITE("\nReading from streamhead...\n");

    /*re-init in each loop*/
    getdbuf.maxlen = sizeof(getdata);
    getdbuf.len = 0;
    getdbuf.buf = (char *)getdata;

    getcbuf.maxlen = sizeof(getctl);
    getcbuf.len = 0;
    getcbuf.buf = (char *)getctl;

    pstreams_getmsg(strm, &getcbuf, &getdbuf, &getflags);

    if((getdbuf.len+getcbuf.len) == 0)
    {
        CONSOLEWRITE("\nstreamhead is empty\n");
    }
    else
    {
        getdata[getdbuf.len] = '\0';
        CONSOLEWRITE("\nstreamhead has %d M_DATA bytes: \n%s\n", 
            getdbuf.len, getdbuf.buf);
    
        getctl[getcbuf.len] = '\0';
        CONSOLEWRITE("\nstreamhead has %d non M_DATA bytes: \n%s\n", 
            getcbuf.len, getcbuf.buf);

        handle_msgin(strm, &getcbuf, &getdbuf);
    }
        
    //pstreams_flushlog(strm);

    pstreams_callsrvp(strm);

    return 0;
}

/******************************************************************************
Name: 
Purpose: 
Parameters:
Caveats:
******************************************************************************/
int
handle_msgin(P_STREAMHEAD *strm, P_BUF *cbuf, P_BUF *dbuf)
{
    MY_PROTO proto={0};

    memcpy(&proto, cbuf->buf, sizeof(proto));
    cbuf->buf += sizeof(proto);
    cbuf->len -= sizeof(proto);

    switch(proto.ctlfunc)
    {

    default:
        ;
    }

    if(dbuf->len > 0)
    {
        printf("Msg RX %s", dbuf->buf);
    }

    return 0;
}

