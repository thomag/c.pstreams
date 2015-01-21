/*===========================================================================
FILE: listop.c

Description: List Operations. Simple, efficient memory management using lists.

Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
/*#define MMGR_DEBUG 1*/
#include "listop.h"

/*************************************************************************
This memory manager uses the following definition for a LIST.
The LIST is a pointer to the tail of a circular list. Each element of
the list consists of a pointer to the next element in the list followed
by the actual object.

       
  __________________________________________________________  
 |                                                          |
 |                                                          |
 |  o-------o          o-------o               o-------o __/
 o->| pnext | -------> | pnext | ---...--->    | pnext |  
    |-------|          |-------|               |-------|<-              
    |       |          |       |               |       |  \       
    | DATA  |          | DATA  |               | DATA  |   \       
    o-------o          o-------o               o-------o    \       
                                                             \       
                                                              \   
                                                     o----------o
                                                     | LIST HDR |
                                                     |          |
                                                     o----------o

*************************************************************************/

/*skip past pool header to the object*/
#define GETPOOLOBJ(pphdr) ((char *)(pphdr) + sizeof(POOLHDR))

/*given the pool object step to the pool header*/
#define GETPOOLHDR(pobj)  ((POOLHDR *)((char *)(pobj) - sizeof(POOLHDR)))

/*skip past list header to the object*/
#define GETLISTOBJ(plhdr) ((char *)(plhdr) + sizeof(LISTHDR))

/*given the pool object step to the pool header*/
#define GETLISTHDR(pobj)  ((LISTHDR *)((char *)(pobj) - sizeof(LISTHDR)))

/*round up the count of bytes to make sure it falls on a word boundary - TODO make more efficient*/
#define ADJUSTEDOBJSIZE(objectsize)   ((objectsize) + WORDBOUNDARY_DIV - ((objectsize) % WORDBOUNDARY_DIV));

/******************************************************************************
Name: lop_getpoolsize
Purpose: returns total number of bytes required to support a memory pool
         of given count of objects of given size
Parameters: adjobjectsize: each objects size in bytes
            count: count of such objects to be in pool
Caveats: 
******************************************************************************/
int lop_getpoolsize(SIZET adjobjectsize, int count)
{
    return sizeof(POOLHDR) + (adjobjectsize+sizeof(LISTHDR))*count;
}

/******************************************************************************
Name: lop_allocpool
Purpose:  Allocates a chunk of memory organized as a LIST of
          free objects of fixed size.
usage :
Parameters:
Caveats: 
******************************************************************************/
POOLHDR *lop_allocpool(SIZET objectsize, int count, void *pplacement)
{
    POOLHDR *ppool;
    LISTHDR *plhdr;
    int adjobjectsize;
    int allocsize;
    int i;/*temp - for loop only*/

    if(!objectsize || !count)
    {
        return NULL;
    }

    /*adjust size of object to end on word boundaries*/
    adjobjectsize = ADJUSTEDOBJSIZE(objectsize);
    allocsize = lop_getpoolsize(adjobjectsize, count);

    if(pplacement)
    {
        /* pplacement should point to atleast allocsize bytes of allocated memory*/
		/*TODO: adjust pplacement to fall on a word boundary*/
        ppool = (POOLHDR *)pplacement;
    }
    else
    {
        ppool = (POOLHDR *)malloc(allocsize);
    }

    memset(ppool, 0, allocsize);

    ppool->pfreelist = (LISTHDR *)GETPOOLOBJ(ppool);
    ppool->freecount = ppool->count = count;

    plhdr = ppool->pfreelist;
    for(i=0; i<count; i++)
    {
        plhdr->pnext = (LISTHDR *)((char *)plhdr+adjobjectsize);
        plhdr = plhdr->pnext;
#ifdef MMGR_DEBUG
        if((char*)plhdr >= (char *)ppool->pfreelist + allocsize)
        {
            printf("overflowing:\n");
            printf("start: 0x%x end: 0x%x step: 0x%x "
                "end-start: 0x%x allocsize: 0x%x, count: %d\n",
                ppool->pfreelist, plhdr, adjobjectsize,
                ((int)plhdr - (int)ppool->pfreelist),
                allocsize, count);
        }


        printf("%d - start: 0x%x end: 0x%x step: 0x%x "
            "end-start: 0x%x allocsize: 0x%x, count: %d\n",
            i,
            ppool->pfreelist, plhdr, adjobjectsize,
            ((int)plhdr - (int)ppool->pfreelist),
            allocsize, count);
#endif

    }
    plhdr->pnext = ppool->pfreelist; /*circular link list*/

    return ppool;
}

/******************************************************************************
Name: lop_releasepool
Purpose: 
Parameters:
Caveats: 
******************************************************************************/
int
lop_releasepool(POOLHDR *pool)
{
    /*TODO - object size, count and placement in pool
     * and use that to reinit entire section of
     * memory to NULLs.
     */

    /*TODO - do only if not placement(stored in pool)*/
    free(pool);

    return LISTOP_SUCCESS;
}

/******************************************************************************
Name: lop_alloc
Purpose: get a free object from given pool
Parameters:
Caveats: 
******************************************************************************/
void *lop_alloc(POOLHDR *ppool)
{
    LISTHDR *plhdr;

    if(!ppool)
    {
        return NULL;
    }

    assert(ppool->pfreelist);

    if(ppool->pfreelist->pnext == ppool->pfreelist)
    {
        assert(ppool->freecount == 0);

        return NULL; /*empty*/
    }

    plhdr = ppool->pfreelist->pnext;/*give the first one*/

    ppool->pfreelist->pnext = plhdr->pnext;
    plhdr->pnext = NULL; /*init for safety*/


	assert(ppool->freecount >=0);
    ppool->freecount--; /*update count in parallel - just for safety*/
    assert(ppool->freecount >=0);

    return GETLISTOBJ(plhdr);
}

/******************************************************************************
Name: lop_push
Purpose:  push an object onto an existing list OR create a new list with
          that object. The list is organized as a stack - FIFO
Parameters:
Caveats: 
******************************************************************************/
LISTHDR *
lop_push(LISTHDR **plist, void *pobj)
{
    LISTHDR *plhdr;

    plhdr = GETLISTHDR(pobj);

    assert(plhdr->pnext == NULL);
    assert(plist);

    /*add to front of circular list*/
    if(*plist)
    {
        plhdr->pnext = (*plist)->pnext;
    }
    else
    {
        /*first element in queue*/
        *plist = plhdr;
    }
    (*plist)->pnext = plhdr;

    return *plist;
}

/******************************************************************************
Name: lop_pop
Purpose: pop an object from given stack - LIFO list
Parameters:
Caveats: 
******************************************************************************/
void *
lop_pop(LISTHDR **plist)
{
    LISTHDR *plhdr;

    assert(plist);

    if(!*plist)
    {
        return NULL;
    }

    plhdr = (*plist)->pnext; /*get first element in list*/

    /*check if last queue element*/
    if(*plist != (*plist)->pnext)
    {
        /*not last element - now re-thread circular list*/
        (*plist)->pnext = plhdr->pnext;
    }
    else
    {
        *plist = NULL;
    }

    plhdr->pnext = NULL; /*initialise for safety*/


    return GETLISTOBJ(plhdr);
}

/******************************************************************************
Name: lop_queue
Purpose: queue object onto list - FIFO list
Parameters:
Caveats: 
******************************************************************************/
LISTHDR *
lop_queue(LISTHDR **plist, void *pobj)
{
    LISTHDR *plhdr;

    plhdr = GETLISTHDR(pobj);

    assert(plhdr->pnext == NULL);
    assert(plist);

    /*add to tail of circular list*/
    if(*plist)
    {
        plhdr->pnext = (*plist)->pnext;
    }
    else
    {
        /*first element in queue*/
        *plist = plhdr;
    }
    (*plist)->pnext = plhdr;

    *plist = plhdr;/*update list to point to tail*/

    return *plist;
}

/******************************************************************************
Name:  lop_dequeue
Purpose: dequeue for a FIFO list
Parameters:
Caveats: 
******************************************************************************/
void *
lop_dequeue(LISTHDR **plist)
{
    /*same as pop - since our list points to tail of circular list*/
    return lop_pop(plist);
}

void *lop_allocarray(POOLHDR *ppool, int arraysize)
{
    return NULL; /*TODO*/
}

/******************************************************************************
Name: lop_delink
Purpose: 
  removes listhdr by delinking, given itself and its previous
  see also: lop_remove()
Parameters:
Caveats: 
******************************************************************************/
void *
lop_delink(LISTHDR **plist, void *pcurrobj, void *pprevobj)
{
    LISTHDR *pcurrobjlhdr=NULL;
    LISTHDR *pprevobjlhdr=NULL;

    assert(plist);

    pcurrobjlhdr = GETLISTHDR(pcurrobj);
    pprevobjlhdr = GETLISTHDR(pprevobj);/*prev object could be itself in case of single elt. list*/

    /*rethread. noop for single element list*/
    pprevobjlhdr->pnext = pcurrobjlhdr->pnext;

    pcurrobjlhdr->pnext = NULL;/*for safety*/

    if(pcurrobjlhdr == pprevobjlhdr)/*single element list?*/
    {
        *plist = NULL; /*empty list*/
    }

    return pcurrobj;
}

/******************************************************************************
Name: lop_release
Purpose:  release given object into given pool
Parameters:
Caveats: 
******************************************************************************/
int
lop_release(POOLHDR *ppool, void *pobj)
{
    LISTHDR *plhdr;

	assert(ppool);
	assert(pobj);

    if(!ppool || !pobj)
    {
        return LISTOP_FAILURE;
    }

    assert(ppool->pfreelist);

    assert(ppool->pfreelist->pnext);

    plhdr = GETLISTHDR(pobj);

    assert(plhdr->pnext == NULL);

    plhdr->pnext = ppool->pfreelist->pnext;
    ppool->pfreelist->pnext = plhdr;
    ppool->pfreelist = plhdr;

    ppool->freecount++;

    return LISTOP_SUCCESS;
}

/******************************************************************************
Name: lop_getnext
Purpose: iterator for list
     getnext: usage :
    for(MyObject *tmpobj=(MyObject *)getnext(plisthdr, NULL);
        tmpobj!=NULL;
        tmpobj=(MyObject *)getnext(plisthdr, tmpobj))
    {
    }
Parameters:
Caveats: 
******************************************************************************/
void *
lop_getnext(LISTHDR *plist, void *pobj)
{
    LISTHDR *plhdr;

    if(!plist)
    {
        return NULL;
    }

    if(!pobj)
    {
        return GETLISTOBJ(plist);
    }

    plhdr = GETLISTHDR(pobj);

    plhdr = plhdr->pnext;

    if(plhdr == plist)
    {
        return NULL; /*end of list*/
    }

    return GETLISTOBJ(plhdr);
}

/******************************************************************************
Name: lop_listlen
Purpose: 
 count the number of elements in list
Parameters:
Caveats: 
******************************************************************************/
int
lop_listlen(LISTHDR *plist)
{
    LISTHDR *listiter=NULL;
    int listlen=0;

    if(!plist)
    {
        return listlen;
    }

    for(listlen=1,listiter=plist->pnext;
        listiter != plist;
        listiter=listiter->pnext)
    {
        listlen++;
    }

    return listlen;
}

/******************************************************************************
Name: lop_checkpool
Purpose: debug mode checks
Parameters:
Caveats: 
******************************************************************************/
int
lop_checkpool(POOLHDR *ppool)
{
    int count = 0;
    LISTHDR *plhdr=NULL;

    assert(ppool);
    assert(ppool->pfreelist);

    plhdr = ppool->pfreelist;

    for(count=0; count < ppool->count; count++)
    {
        plhdr = plhdr->pnext;
        if(plhdr == ppool->pfreelist)
        {
            break;
        }
    }

#ifdef MMGR_DEBUG
    printf("%x->freelist count = %d\n", ppool, count);
#endif
    if(count >= ppool->count)
    {
        return LISTOP_FAILURE;
    }

    return LISTOP_SUCCESS;
}

