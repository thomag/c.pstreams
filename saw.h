/*===========================================================================
FILE: saw.h

Description: SAW - stop-and-wait protocol module

Activity:
 Date         Author           Comments
 Oct.09,2003  tgeorge          Created.

===========================================================================*/

#ifndef SAW_H
#define SAW_H

#include "listop.h"

typedef struct saw_area
{
    int8 SeqNo;
    int8 AckNo;
    uint32 AckWaitTimer; //holds time at which ACK wait times-out
    uint32 SendAckTimer; //holds time at which send-ACK wait times-out
    int CurrentReTxCount; //holds re-transmit count of current message
    int MaxReTXCount; //holds maximum re-transmits allowed
    uint32 AckWaitTimeout; //holds time-out value for AckWaitTimer
    uint32 SendAckTimeout; //holds time-out value for SendAckTimer
} SAWAREA;

typedef struct saw_hdr
{
    int8 SeqNo;
    int8 AckNo;
} SAWHDR;

int
saw_init();
int
saw_open(P_QUEUE *q);
int
saw_wput(P_QUEUE *q, P_MSGB *msg);
int
saw_rput(P_QUEUE *q, P_MSGB *msg);
int
saw_rsrvp(P_QUEUE *q);
int
saw_wsrvp(P_QUEUE *wq);
P_BOOL
saw_myctl(P_QUEUE *q, P_MSGB *msg);
SAWAREA *
saw_getarea(P_QUEUE *q);
P_MSGB *
saw_gethdr(P_QUEUE *q);
void
saw_abort();
#endif
