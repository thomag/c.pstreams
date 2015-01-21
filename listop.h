/*===========================================================================
FILE: listop.h

Description: List Operations. Simple, efficient memory management using lists.

Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/
#ifndef LOP_H
#define LOP_H

#include<string.h>
#include "options.h"

/**********************************************************
list operations a.k.a. lop
**********************************************************/

#define SIZET size_t
#define GTFAILURE -1
#define GTSUCCESS 0

enum LISTOP_STATUS {LISTOP_SUCCESS, LISTOP_FAILURE};

typedef int LRET;

/*sizeof(listhdr) is required to end on a word boundary*/
typedef struct listhdr {
	struct listhdr *pnext;
} LISTHDR;

/*sizeof(poolhdr) is required to end on a word boundary*/
typedef struct poolhdr {
	LISTHDR *pfreelist;
	LISTHDR *palloclist;
	uint32 objsize; /*size of each object in pool, not including LISTHDR*/
	uint32 count; /*count of all elements in pool*/
	uint32 freecount;	/*count of free elements in pool*/
#ifdef PDBG_ON
    uint32 lowat; /*remembers minimum attained value for freecount*/
#endif
	uint32 msize; /*size of memory used by this pool*/
	void *mptr; /*ptr. to memory supplied to this pool, for its creation*/
	void *endptr; /*address of last of the consecutive bytes used = (char *)WALIGN(mptr)+msize*/
} POOLHDR;

/*function prototypes*/
uint32 lop_getpoolsize(SIZET adjobjectsize, uint32 count);
POOLHDR *lop_allocpool(SIZET objectsize, uint32 count, void *pplacement);
void *lop_alloc(POOLHDR *ppool);
void *lop_allocarray(POOLHDR *ppool, int arraysize);
LRET lop_releasepool(POOLHDR *pool);
LRET lop_release(POOLHDR *ppool, void *pobj);
LRET lop_checkpool(POOLHDR *ppool);
LISTHDR *lop_push(LISTHDR **plist, void *pobj);
void *lop_pop(LISTHDR **plist);
LISTHDR *lop_queue(LISTHDR **plist, void *pobj);
void * lop_remove(LISTHDR **plist, void *pobj);
void *lop_dequeue(LISTHDR **plist);
void * lop_insert(LISTHDR **plist, void *pobj, void *pobj_marker);
void *lop_getnext(LISTHDR *plist, void *pobj);
uint32 lop_listlen(LISTHDR *plist);
LISTHDR * lop_findprev(LISTHDR *pcurrlhdr);
void *lop_delink(LISTHDR **plist, void *pcurrobj, void *pprevobj);

void *lop_malloc(int32 size);
void lop_free(void *ptr);
#endif
