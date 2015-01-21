/*===========================================================================
FILE: env.h

   Environment specific routines

 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.

===========================================================================*/
#ifndef ENV_H
#define ENV_H

#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include "util.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * rolls over and goes negative just like lbolt variable in unix(drv_getparm)
 * only difference is valid
 */
int32 clockticks();

UTIME my_time();

ulong my_htonl(ulong hostlong);

ushort my_htons(ushort hostshort);
ulong my_ntohl(ulong netlong);
ushort my_ntohs(ushort netshort);
void my_sleep(long millisecs);

LOGFILE *LOGOPEN(const char *filename, const char *mode);
int LOGWRITE(LOGFILE *, const char *format, ...);
int VLOGWRITE(LOGFILE *, const char *format, va_list ap);
int CONSOLEWRITE(const char *format, ...);
int VCONSOLEWRITE(const char *format, va_list ap);



/*
 *control codes - for now grouping P_M_PROTO and P_M_CTL codes together.
 *all codes have to be 3 digits. 
 *2 digit codes belong to P_CTLCODE in pstreams.h
 */
enum MY_CTLCODE
{
    MY_ABORTED=100,
    MY_CLOSE,

    CTLLAST
};

enum MY_TRACECODES
{
   TRACE_ALL
};

typedef struct my_proto
{
    int8 ctlfunc; /*control function enum - module specific*/
    /*char data[len]; data associated with control, and 
                        len is length of data associated with control*/
} MY_PROTO;

typedef struct my_ctl
{
    int8 ctlfunc;
} MY_CTL;

typedef struct my_ipaddr
{
    char ipaddr[16]; /*null terminated string for IP addresess*/
    uint16 port;
} MY_IPADDR;


typedef struct my_error
{
    uint8 errcode;
    uint16 moduleid; /*module where generated*/
} MY_ERROR;


#ifdef __cplusplus
}
#endif

#endif
