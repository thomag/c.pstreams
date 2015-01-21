/*===========================================================================
FILE: stdmod.h

   Portable and Pure streams. Routines implementing a 'unix streams' inspired
design for layered protocols


 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.

===========================================================================*/
#ifndef STDMOD_H
#define STDMOD_H

int
stdapp_open(P_QUEUE *q);
int
stdapp_wput(P_QUEUE *q, P_MSGB *msg);
int
stdapp_rput(P_QUEUE *q, P_MSGB *msg);
int
stdev_wput(P_QUEUE *q, P_MSGB *msg);
int
stddev_rput(P_QUEUE *q, P_MSGB *msg);
int
stdapp_init();
int
stddev_init();

#endif
