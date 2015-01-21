/*===========================================================================
FILE: pstreams_echo.h

Description: pstreams echo module

Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.
===========================================================================*/

#ifndef ECHO_H
#define ECHO_H

#include "listop.h"

int
echo_init();
int
echo_open(P_QUEUE *q);
int
echo_wput(P_QUEUE *q, P_MSGB *msg);
int
echo_rput(P_QUEUE *q, P_MSGB *msg);
int
echo_rsrvp(P_QUEUE *q);
int
echo_wsrvp(P_QUEUE *wq);

#endif
