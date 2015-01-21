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
#include "options.h"
#include "env.h"
#include"assert.h"
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


/******************************************************************************
Name: lop_getpoolsize
Purpose: returns total number of bytes required to support a memory pool
         of given count of objects of given size
Parameters: adjobjectsize: each objects size in bytes
            count: count of such objects to be in pool
Caveats: 
******************************************************************************/
uint32 lop_getpoolsize(SIZET objectsize, uint32 count)
{
    return sizeof(POOLHDR) + (WALIGN(objectsize)+sizeof(LISTHDR))*count;
}

/******************************************************************************
Name: lop_allocpool
Purpose:  Allocates a chunk of memory organized as a LIST of
          free objects of fixed size.
usage :
Parameters:
Caveats: 
    TODO: lop_allocpool's argument list should include a structrure like FREE_RTN
and should be stored in POOLHDR. This would be used to free pplacement at release.
Currently, the endgame is not played out well - lop_free is called at all times.
******************************************************************************/
POOLHDR *lop_allocpool(SIZET objectsize, uint32 count, void *pplacement)
{
    POOLHDR *ppool;/*pointer to pool*/
    LISTHDR *plhdr;/*pointer to list header*/
    uint32 adjobjectsize; /*object size after word alignment*/
    uint32 allocsize; /*total memory needed for pool*/
    uint32 i;/*temp - for loop only*/

    if(!objectsize || !count)
    {
        return NULL;
    }

    /*adjust size of object to end on word boundaries*/
    adjobjectsize = WALIGN(objectsize);
    ASSERT(objectsize == WALIGN(objectsize)); /*not really needed - but...*/
    allocsize = lop_getpoolsize(adjobjectsize, count);

    if(pplacement)
    {
        /* pplacement should point to atleast allocsize bytes of allocated memory*/
        /*adjust pplacement to fall on a word boundary*/
        ppool = (POOLHDR *)WALIGN((unsigned long)pplacement);
    }
    else
    {
        ppool = (POOLHDR *)lop_malloc(allocsize);
    }

    memset(ppool, 0, allocsize);

    /*the following assignments are some statistics used during debugging*/
    ppool->mptr = pplacement;
    ppool->msize = allocsize;
    ppool->objsize = adjobjectsize;
    ppool->endptr = (char *)ppool+allocsize;

    ppool->pfreelist = (LISTHDR *)GETPOOLOBJ(ppool);
    ppool->freecount = ppool->count = count;

#ifdef PDBG_ON
    ppool->lowat = ppool->freecount;
#endif

    plhdr = ppool->pfreelist;
    for(i=1; i<count; i++) /*count starts with 1 since, plhdr has been assigned first one*/
    {
        ASSERT((char *)plhdr >= (char *)ppool->mptr); /*bounds check*/
        ASSERT((char *)plhdr < (char *)ppool->endptr);/*bounds check*/

        plhdr->pnext = (LISTHDR *)((char *)plhdr+sizeof(LISTHDR)+adjobjectsize);
        plhdr = plhdr->pnext;

#ifdef MMGR_DEBUG
        if((char*)plhdr + adjobjectsize >= (char *)ppool->endptr)
        {
            printf("overflowing:\n");
            printf("start: 0x%x end: 0x%x step: 0x%x "
                "end-start: 0x%x allocsize: 0x%x, count: %d\n",
                ppool->pfreelist, plhdr, adjobjectsize,
                ((int)plhdr - (int)ppool->pfreelist),
                allocsize, count);
            ASSERT(0);
        }
#endif

    }
    plhdr->pnext = ppool->pfreelist; /*circular link list*/

    ASSERT(lop_checkpool(ppool) == LISTOP_SUCCESS);

    return ppool;
}

/******************************************************************************
Name: lop_releasepool
Purpose: 
Parameters:
Caveats: 
******************************************************************************/
LRET
lop_releasepool(POOLHDR *pool)
{
    /*TODO - object size, count and placement in pool
     * and use that to reinit entire section of
     * memory to NULLs.
     */

    if(pool && pool->mptr)
    {
        /*do only if not placement(stored in pool)*/
        lop_free(pool);
    }

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
    LISTHDR *plhdr; /*allocated object*/

    if(!ppool)
    {
        return NULL;
    }

    /*debug mode*/
    ASSERT(lop_checkpool(ppool) == LISTOP_SUCCESS);

    if(!ppool->pfreelist)
    {
        ASSERT(ppool->freecount == 0);

        return NULL; /*empty*/
    }

    /*
     *Allocate a free object from head of list.
     *Since this is a circular list the following
     *assignment works even if this is the last object
     *in pool
     */
    plhdr = ppool->pfreelist->pnext;

    if(ppool->pfreelist == ppool->pfreelist->pnext)
    {
        /*last object was allocated now*/
        ppool->pfreelist = NULL; 
    }
    else
    {
        ppool->pfreelist->pnext = plhdr->pnext;
    }

    plhdr->pnext = NULL; /*init for safety*/

    ASSERT(ppool->freecount >=0);
    ppool->freecount--; /*update count in parallel - just for safety*/
    ASSERT(ppool->freecount >=0);

#ifdef PDBG_ON
    ppool->lowat = MIN(ppool->lowat, ppool->freecount);
#endif
    
    /*debug mode*/
    ASSERT(lop_checkpool(ppool) == LISTOP_SUCCESS);

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

    ASSERT(plhdr->pnext == NULL);
    ASSERT(plist);

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

    ASSERT(plist);

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

    ASSERT(plhdr->pnext == NULL);
    ASSERT(plist);

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

/******************************************************************************
Name:  lop_insert
Purpose: insert element into list after given marker obj
Parameters:
Caveats: 
******************************************************************************/
void *
lop_insert(LISTHDR **plist, void *pobj, void *pobj_marker)
{
    if(!pobj_marker)
    {
        /*goes to head of queue*/
        lop_push(plist, pobj);
    }
    else
    {
        LISTHDR *plhdr_obj;
        LISTHDR *plhdr_marker;

        plhdr_obj = GETLISTHDR(pobj);
        plhdr_marker = GETLISTHDR(pobj_marker);

        if(*plist == plhdr_marker)
        {
            /*marker is the tail*/
            lop_queue(plist, pobj);
        }
        else
        {
            plhdr_obj->pnext = plhdr_marker->pnext;
            plhdr_marker->pnext = plhdr_obj;
        }
    }

    return *plist;
}

void *lop_allocarray(POOLHDR *ppool, int arraysize)
{
    ppool=NULL; /*unused*/
    arraysize=0;/*unused*/
    return NULL; /*TODO*/
}

/******************************************************************************
Name: lop_remove
Purpose: 
  removes given element from list
Parameters:
Caveats: 
******************************************************************************/
void *
lop_remove(LISTHDR **plist, void *pobj)
{
    LISTHDR *pcurrobjlhdr=NULL; /*plhdr corr. to pobj*/
    LISTHDR *pprevobjlhdr=NULL; /*finally holds plhdr previous to pcurrobjlhdr*/

    ASSERT(plist);

    pcurrobjlhdr = GETLISTHDR(pobj);

   /*
    * optimization : often item to be removed is the head. lop_pop() 
    * is best for such cases
    */
   if((*plist)->pnext == pcurrobjlhdr)
   {
       	return lop_pop(plist);
   }
   /*
    * iterate thru the list until pcurrobjlhdr
    * and then remove it. 
    * First find pprevobjlhdr, so we can rethread
    */

    pprevobjlhdr=*plist; /*point at tail*/
    do
    {
        if(pprevobjlhdr->pnext == pcurrobjlhdr)
        {
            break; /*found it*/
        }
        pprevobjlhdr = pprevobjlhdr->pnext; /*advance*/
    }while(pprevobjlhdr != *plist);

    /*
     *now rethread list omitting the delinked element
     */
    if(pprevobjlhdr->pnext == pcurrobjlhdr)
    {
        /*NOOP for single element list*/
        pprevobjlhdr->pnext = pcurrobjlhdr->pnext;
          
        pcurrobjlhdr->pnext = NULL;/*for safety*/

           if(pcurrobjlhdr == pprevobjlhdr)/*single element list?*/
           {
               *plist = NULL; /*empty list*/
           }
        else if(*plist == pcurrobjlhdr) /*the element delinked is the tail*/
        {
            /*need to update tail*/
            *plist = pprevobjlhdr;
        }

        return pobj;
    }

    return  NULL; /*failed to locate pobj in list*/
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

    ASSERT(plist);

    pcurrobjlhdr = GETLISTHDR(pcurrobj);

   /* Get prev obj listhdr - note : prev object could be itself in case of single elt. list*/
    if(!pprevobj)
    {
        pprevobjlhdr = lop_findprev(pcurrobjlhdr);
    }
    else
    {
        pprevobjlhdr = GETLISTHDR(pprevobj);
    }

    /*rethread. noop for single element list*/
    pprevobjlhdr->pnext = pcurrobjlhdr->pnext;

    pcurrobjlhdr->pnext = NULL;/*for safety*/

    if(pcurrobjlhdr == pprevobjlhdr)/*single element list?*/
    {
        *plist = NULL; /*empty list*/
    }
    else if(*plist == pcurrobjlhdr) /*the element delinked is the tail*/
    {
        /*need to update tail*/
        *plist = pprevobjlhdr;
    }

    return pcurrobj;
}

/******************************************************************************
Name: lop_release
Purpose:  release given object into given pool
Parameters:
Caveats: 
******************************************************************************/
LRET
lop_release(POOLHDR *ppool, void *pobj)
{
    LISTHDR *plhdr;

    ASSERT(ppool);
    ASSERT(pobj);

    if(!ppool || !pobj)
    {
        return LISTOP_FAILURE;
    }

    plhdr = GETLISTHDR(pobj);

    ASSERT((char *)plhdr >= (char *)ppool->mptr); /*bounds check*/
    ASSERT((char *)plhdr < (char *)ppool->endptr);/*bounds check*/

    ASSERT(plhdr->pnext == NULL);

    if(ppool->pfreelist)
    {
        ASSERT(ppool->pfreelist->pnext);

        plhdr->pnext = ppool->pfreelist->pnext;
        ppool->pfreelist->pnext = plhdr;
    }
    else
    {
        /*first element*/
        plhdr->pnext = plhdr;
    }

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
        return GETLISTOBJ(plist->pnext); /*start from head*/
    }

    plhdr = GETLISTHDR(pobj);

    plhdr = plhdr->pnext;

    if(plhdr == plist->pnext) /*check against head*/
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
uint32
lop_listlen(LISTHDR *plist)
{
    LISTHDR *listiter=NULL;
    uint32 listlen=0;

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
Name: lop_findprev
Purpose: for internal use. get the previous listhdr.
Parameters:
Caveats: 
******************************************************************************/
LISTHDR *
lop_findprev(LISTHDR *pcurrlhdr)
{
    LISTHDR *pprevlhdr=NULL;
    LISTHDR *listiter=NULL;

   /*for a single element list - previous element is itself*/

    for(listiter = pcurrlhdr->pnext;
        listiter->pnext != pcurrlhdr;
        listiter = listiter->pnext)
    {
        ;
    }

    ASSERT(listiter->pnext == pcurrlhdr);

    pprevlhdr = listiter;

    return pprevlhdr;
}

/******************************************************************************
Name: lop_checkpool
Purpose: debug mode checks
Parameters:
Caveats: 
******************************************************************************/
LRET
lop_checkpool(POOLHDR *ppool)
{
    uint32 count = 0;
    LISTHDR *plhdr=NULL;

    ASSERT(ppool);

    plhdr = ppool->pfreelist;

    for(count=0; plhdr && (count < ppool->count); count++)
    {
        ASSERT((char *)plhdr >= (char *)ppool->mptr); /*bounds check*/
        ASSERT((char *)plhdr < (char *)ppool->endptr);/*bounds check*/

        plhdr = plhdr->pnext;
        ASSERT(plhdr); /*plhdr can't go NULL midway*/
        if(plhdr == ppool->pfreelist)
        {
            break;
        }
    }

#ifdef MMGR_DEBUG
    printf("pool address : %x\n", ppool);
    printf("pool end : %x\n", ppool->endptr);
    printf("%x->freelist count = %d\n", ppool, ppool->freecount);
#endif

    if(plhdr != ppool->pfreelist)
    {
        return LISTOP_FAILURE;
    }

    return LISTOP_SUCCESS;
}

/******************************************************************************
Name: lop_malloc
Purpose: front end of Operating System's malloc
Parameters:
Caveats: 
******************************************************************************/
void *
lop_malloc(int32 size)
{
    ASSERT(0);
    
    size=0;/*unused*/
    return NULL;
}

/******************************************************************************
Name: lop_free
Purpose: front end of Operating System's free()
Parameters:
Caveats: 
******************************************************************************/
void 
lop_free(void *ptr)
{
    ptr=NULL;/*unused*/
    return;
}
