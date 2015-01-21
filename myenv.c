/*===========================================================================
FILE: env.c

   environment specific routines - used in protocol

 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/

#include <stdlib.h>
#include "pstreams.h"
#include "util.h"
#include "env.h"

char diagbuf[2048];


PDBG(extern P_STREAMHEAD *dbgstrm);

/*
 * rolls over and goes negative just like lbolt variable in unix(drv_getparm)
 * only difference is valid
 */
int32 my_clockticks()
{
	static ulong prev_ticks = 0;
	ulong curticks = GetTickCount(); //in milliseconds
	if(prev_ticks != 0)
	{
		PDBG(uint32 diff = curticks - prev_ticks);
	}

	prev_ticks = curticks;

    return (int)curticks;
}

UTIME my_time()
{
    return time(0);
}

ulong my_htonl(ulong hostlong)
{
    return htonl(hostlong);
}

ushort my_htons(ushort hostshort)
{
    return htons(hostshort);
}
ulong my_ntohl(ulong netlong)
{
    return ntohl(netlong);
}
ushort my_ntohs(ushort netshort)
{
    return ntohs(netshort);
}


int my_fprintf(LOGFILE *file, const char *fmt, ...)
{

    va_list ap;

    va_start(ap, fmt);

    vfprintf(file, fmt, ap);
                            
    va_end(ap);

    return 0; /*actually 'return strlen(diagbuf)' is better - but for speed*/
}

int my_vfprintf(LOGFILE *file, const char *fmt, va_list ap)
{

    vfprintf(file, fmt, ap);

    fprintf(file, "\n");

    return 0; /*actually 'return strlen(diagbuf)' is better - but for speed*/
}

int my_printf(const char *fmt, ...)
{              
    va_list ap;

    va_start(ap, fmt);

    vsprintf(diagbuf, fmt, ap);

    printf("%s", diagbuf);
    printf("\r\n");
    
    /*va_end(ap);*/
    
    return 0; /*actually 'return strlen(diagbuf)' is better - but for speed*/
}

int my_vprintf(const char *fmt, va_list ap)
{              

    vsprintf(diagbuf, fmt, ap);

    printf("%s", diagbuf);
    printf("\r\n");

    return 0;
}
