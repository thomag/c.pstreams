/*===========================================================================
FILE: util.c

  Utility routines

 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.

===========================================================================*/

#include <stdlib.h>
#include "options.h"
#include "assert.h"
#include "env.h"
#include "pstreams.h"
#include "util.h"


void
bintohex(uchar *hexbuf, uchar *binbuf, uint32 len)
{
    static char hexcodes[]="0123456789ABCDEF";  /* Hex to binary table */
    uint32 i;                                                                        

    for(i = 0; i < len; i++ )
    {                                                                           
        *hexbuf++ = hexcodes[(*binbuf >> 4) & 15];
        *hexbuf++ = hexcodes[ *binbuf++ & 15 ];                           
    }
    
    return;
}

uchar
hextobin_nibble(uchar hexcode)
{
    if((hexcode >= '0') && (hexcode <= '9'))
    {
        return hexcode - '0';
    }

    if((hexcode >= 'a') && (hexcode <= 'z'))
    {
        return hexcode - 'a' + 10;
    }

    if((hexcode >= 'A') && (hexcode <= 'Z'))
    {
        return hexcode - 'A' + 10;
    }

    return 0;
}


    
void
hextobin(uchar *binbuf, uchar *hexbuf, uint32 len)
{
    uint32 i=0;


    if(len%2)
    {
        len--; /*lose a nibble from the tail*/
    }

    for(i=0; i+1<len; i += 2)
    {
        int nibble1=hextobin_nibble(hexbuf[i]);
        int nibble2=hextobin_nibble(hexbuf[i+1]);

        binbuf[i/2] = nibble1 << 4;
        binbuf[i/2] |= nibble2;
    }

    return;
}
 
int
fieldassign(uchar *to, uint32 from, int16 size)
{
    switch(size)
    {
    case 1:
        *to = (char)from;
        break;
    case 2:
        {
            uint16 val = p_htons((uint16)from);
            memcpy(to, &val, size);
        }
        break;
    case 4:
        {
            uint32 val = p_htonl((uint32)from);
            memcpy(to, &val, size);
        }
        break;

    default:
        ASSERT(0);

    }
    return from;
}

void
fieldread(void *to, void *from, int16 size)
{
    switch(size)
    {
    case 1:
        *(uint8 *)to = *(uint8 *)from;
        break;
    case 2:
        {
            uint16 val=0;
            memcpy(&val, from, size);
            *(uint16 *)to = p_ntohs(val); /*it is assumed "to" is aligned*/
        }
        break;
    case 4:
        {
            uint32 val=0;
            memcpy(&val, from, size);
            *(uint32 *)to = p_ntohl(val); /*it is assumed "to" is aligned*/
        }
        break;

    default:
        ASSERT(0);

    }
    return;
}

/*
 *given a pointer convert the first two bytes to a short.
 *assume the pointer location holds a short read off the net
 *therefore provide for byte-ordering and memory alignment
 */
short
readshort(void *from)
{
    short read=0;

    ASSERT(sizeof(short) == sizeof(uint16));/*code elswhere assumes this*/

    memcpy(&read, from, sizeof(short));

    return p_ntohs(read);
}

/*
 *given a pointer convert the first four bytes to a long.
 *assume the pointer location holds a long read off the net
 *therefore provide for byte-ordering and memory alignment
 */
long
readlong(void *from)
{
    long read=0;

    ASSERT(sizeof(long) == sizeof(uint32));/*code elswhere assumes this*/

    memcpy(&read, from, sizeof(long));

    return p_ntohl(read);
}

int
strtoi(int *i, char *str)
{
    return sscanf(str, "%d", i);
}

int
arrtoi(int *i, char *arr, int16 len)
{
    char str[MAX_INT_AS_STR]={0};

    memcpy(str, arr, len);

    str[len]='\0';

    strtoi(i, str);

    return *i;
}

int
itoarr(char *arr, int i, int16 len)
{
    char str[MAX_INT_AS_STR]={0};
    int printlen=0;/*length of i as str when printed*/
    int zerolen=0;/*leading zeros*/

    sprintf(str, "%d", i);

    printlen = strlen(str);

    zerolen = (len > printlen ? len - printlen : 0);

    memset(arr, '0', zerolen);

    arr += zerolen;

    memcpy(arr, str, printlen);
    
    return len;
}

void
senderror(P_QUEUE *q, uchar errcode)
{
    MY_ERROR err={0};
    P_MSGB *msg=NULL;

#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTINFO+1, "_sendinfo: About to send error code %d",
            (int)errcode);
#endif /*PSTREAMS_LT*/

#ifdef PSTREAMS_LEAN
    PSTRMHEAD(q)->perrno = (uint16)errcode;
#else
    /*send it as a message to streamhead*/
    err.errcode = errcode;
    err.moduleid = q->q_qinfo.qi_minfo->mi_idnum;

    msg = pstreams_allocb(PSTRMHEAD(q), sizeof(err), 0);

    if(msg)
    {
        msg->b_datap->db_type = P_M_ERROR;
        memcpy(msg->b_wptr, &err, sizeof(err));
        msg->b_wptr += sizeof(MY_ERROR);

        pstreams_putq(RD(q), msg);
    }
    else
    {
#ifdef PSTREAMS_LT
        pstreams_log(q, PSTREAMS_LTERROR, "_senderror: out-of-memory while "
            "allocating msg for error code %d", (int)errcode);
#endif /*PSTREAMS_LT*/
    }
#endif
}

void
sendproto(P_QUEUE *q, uchar code, char *data, int16 datalen)
{
    MY_PROTO proto={0};
    P_MSGB *msg=NULL;

#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTINFO+1, "_sendproto: About to send info code %d",
            (int)code);
#endif /*PSTREAMS_LT*/

    proto.ctlfunc = code;

    msg = pstreams_allocb(PSTRMHEAD(q), sizeof(proto)+datalen, 0);

    if(msg)
    {
        msg->b_datap->db_type = P_M_PROTO;
        memcpy(msg->b_wptr, &proto, sizeof(MY_PROTO));
        msg->b_wptr += sizeof(MY_PROTO);

        if(data)
        {
            ASSERT(datalen > 0);
            memcpy(msg->b_wptr, data, datalen);
            msg->b_wptr += datalen;
        }

        pstreams_putnext(RD(q), msg);/*ignoring canput()*/
    }
    else
    {
#ifdef PSTREAMS_LT
        pstreams_log(q, PSTREAMS_LTWARNING+1, "_sendproto: out-of-memory while "
            "allocating msg for code %d", (int)code);
#endif /*PSTREAMS_LT*/
    }
}

void
sendctl(P_QUEUE *q, uchar code, char *data, int16 datalen)
{
    MY_CTL ctl={0};
    P_MSGB *msg=NULL;

#ifdef PSTREAMS_LT
    pstreams_log(q, PSTREAMS_LTINFO+1, "_sendctl: About to send ctl code %d",
            (int)code);
#endif /*PSTREAMS_LT*/

    ctl.ctlfunc = code;

    msg = pstreams_allocb(PSTRMHEAD(q), sizeof(ctl)+datalen, 0);

    if(msg)
    {
        msg->b_datap->db_type = P_M_CTL;
        memcpy(msg->b_wptr, &ctl, sizeof(MY_CTL));
        msg->b_wptr += sizeof(MY_CTL);

        if(data)
        {
            ASSERT(datalen > 0);
            memcpy(msg->b_wptr, data, datalen);
            msg->b_wptr += datalen;
        }

        pstreams_putnext(q, msg);/*send in same direction*/
    }
    else
    {
#ifdef PSTREAMS_LT
        pstreams_log(q, PSTREAMS_LTWARNING+1, "_sendctl: out-of-memory while "
            "allocating msg for code %d", (int)code);
#endif /*PSTREAMS_LT*/
		PSTRMHEAD(q)->perrno = P_OUTOFMEMORY;
    }
}

/*
 *Form and send a control message down the stream
 */
void sendctlmsg(P_STREAMHEAD *strm, uchar code, char *data, int datalen)
{
}
