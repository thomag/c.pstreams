/*===========================================================================
FILE: listop.h

Description: List Operations. Simple, efficient memory management using lists.

Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/
#ifndef M_H
#define M_H

#include<string.h>

/**********************************************************
list operations a.k.a. lop
**********************************************************/

#define SIZET size_t
#define WORDBOUNDARY_DIV 0x0004
#define GTFAILURE -1
#define GTSUCCESS 0

enum LISTOP_STATUS {LISTOP_SUCCESS, LISTOP_FAILURE};

/*sizeof(listhdr) is required to end on a word boundary*/
typedef struct listhdr {
	struct listhdr *pnext;
} LISTHDR;

/*sizeof(poolhdr) is required to end on a word boundary*/
typedef struct poolhdr {
	LISTHDR *pfreelist;
	LISTHDR *palloclist;
	int count;
	int freecount;
} POOLHDR;

/*function prototypes*/
POOLHDR *lop_allocpool(SIZET objectsize, int count, void *pplacement);
void *lop_alloc(POOLHDR *ppool);
void *lop_allocarray(POOLHDR *ppool, int arraysize);
int lop_releasepool(POOLHDR *pool);
int lop_release(POOLHDR *ppool, void *pobj);
int lop_checkpool(POOLHDR *ppool);
LISTHDR *lop_push(LISTHDR **plist, void *pobj);
void *lop_pop(LISTHDR **plist);
LISTHDR *lop_queue(LISTHDR **plist, void *pobj);
void *lop_dequeue(LISTHDR **plist);
void *lop_getnext(LISTHDR *plist, void *pobj);
int lop_listlen(LISTHDR *plist);
void *lop_delink(LISTHDR **plist, void *pcurrobj, void *pprevobj);

#endif
