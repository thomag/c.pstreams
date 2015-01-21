#ifndef CYGWIN_OPTIONS_H
#define CYGWIN_OPTIONS_H
/*===========================================================================
FILE: options.h

  machine and compiler dependant options

===========================================================================*/

#include <stdio.h>

/*#define PSTREAMS_STRICTTYPES*/

typedef FILE LOGFILE;

#ifdef __cplusplus
#ifndef PSTREAMS_STRICTTYPES
#define PSTREAMS_STRICTTYPES
#endif
#endif

#define PSTREAMS_LEAN
/*#define PSTREAMS_WIN32
*/
//#define PSTREAMS_LT
#define PDBG_ON

#ifndef ASSERT
#define ASSERT assert
#endif

#define Sleep my_sleep

#define VMEMSIZE  1024*1024
#define PMEMSIZE  96*1024
/*
 *for o2kpkt,o2kses hdr - note these do not co-exist because 
 *o2kseg makes a copy, and release its in msg
 */
#define POOL16SIZE 256

#define POOL64SIZE 16 

/*
 * this being default segment size
 */ 
#define POOL256SIZE 32

#define POOL512SIZE 8

#define POOL1792SIZE 2

/*BLOCK 1. machine dependent codes for memory alignment constraints*/
    /*number bits in the data bus*/
#define DATABITS 0x0100
    /*number of bits in address bus*/
#define ADDRBITS 0x0108

    /*structures are aligned at multiples of their size(rounded to power of 2)*/
    /*typical alignments at*/
#define WORDBOUNDARY sizeof(int)
#define WORDBOUNDARY_DIV 0x0004
/*round up the count of bytes to make sure it falls on a word boundary - TODO make more efficient*/
#define ADJUSTEDOBJSIZE(objectsize)   ((objectsize) % WORDBOUNDARY_DIV ? (objectsize) + WORDBOUNDARY_DIV - ((objectsize) % WORDBOUNDARY_DIV) : (objectsize))
/*more efficient*/
#define WMASK (WORDBOUNDARY_DIV-1)
#define WALIGN(x) ((x & WMASK) ? ((x) & ~WMASK) + WORDBOUNDARY_DIV : (x))


/*BLOCK 2. codes derived from BLOCK 1*/
#define WORDOVERLAP (WORDBOUNDARY-1)
/*mask non-significant bits in word boundary*/
#define WORDMASK (~0UL & WORDBOUNDARY)

/*note from memorymanager.org:
Alignment is a constraint on the address of an object in memory. 

The constraint is usually that the object's address must be a multiple of a power of two,
2^N, and therefore that the least significant N bits of the address must be zero.

The bus hardware of many modern processors cannot access multi-byte(2) objects at any 
memory address. Often word-sized objects must be aligned to word boundaries, double-words 
to double-word boundaries, double-floats to 8-byte boundaries, and so on. If a program 
attempts to access an object that is incorrectly aligned, a bus error occurs.

Relevance to memory management: A memory manager must take care to allocate memory 
with an appropriate alignment for the object that is going to be stored there. 
Implementations of malloc have to allocate all blocks at the largest alignment that 
the processor architecture requires.

Other reasons for aligning objects include using the least significant bits of 
the address for a tag.
*/
#define WORDALIGN(m) ((void *) (((unsigned long)(m) & WORDOVERLAP) ? (((unsigned long)(m) & ~WMASK) + WORDBOUNDARY) : (unsigned long)(m) ))

typedef int            SOCKET;
typedef struct sockaddr_in    MY_REMOTEADDR;

typedef unsigned char        uchar;
/*typedef unsigned short int    ushort;
typedef unsigned int        uint;
*/
typedef unsigned long        ulong;

typedef char                int8;
typedef unsigned char        uint8;
typedef short                int16;
typedef unsigned short        uint16;
typedef int                    int32;
typedef unsigned int        uint32;
typedef unsigned long        UA;
typedef unsigned long        UTIME;
typedef char		boolean;

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MIN3
#define MIN3(x,y,z) MIN((x), MIN((y), (z)))
#endif

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

/*the p_* below stands for 'platform-specific'*/
#define p_ntohs ntohs
#define p_ntohl ntohl
#define p_htons htons
#define p_htonl htonl
#define p_inet_addr inet_addr

#define LOGOPEN fopen
#define LOGWRITE my_fprintf
#define CONSOLEWRITE my_printf
#define VCONSOLEWRITE my_vprintf
#define VLOGWRITE my_vfprintf
#define LOGFLUSH fflush
/*#define PDEV_INIT WSAStartup
*/
/*#define PDEV_ERROR WSAGetLastError*/


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <assert.h>


/*#define PSTREAMS_ECHO*/
#define PSTREAMS_UDP
/*#define PSTREAMS_TCP*/

#define UDPDEV_LTLEVEL PSTREAMS_LTALL
#define PSTREAMS_UDPDUMP

#endif
