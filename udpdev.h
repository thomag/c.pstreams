#ifndef UDPDEV_H
#define UDPDEV_H

#include "options.h"

/*
 * module specific local area
 */
typedef struct udpdevarea
{
    SOCKET sock;
    struct sockaddr_in laddr; /*local address*/
    struct sockaddr_in raddr; /*remote address*/
    struct sockaddr_in faddr; /*from address - i.e., responding address*/
#ifdef PSTREAMS_WIN32
    WSADATA wsadata; 
#endif
} UDPDEVAREA;

enum UDPDEV_DEFINES
{
    MAXUDPDGRAMSIZE=1024
};

int
udpdev_open(P_QUEUE *q);
int
udpdev_close(P_QUEUE *q);
int
udpdev_wput(P_QUEUE *q, P_MSGB *msg);
int
udpdev_wsrvp(P_QUEUE *q);
int
udpdev_rput(P_QUEUE *q, P_MSGB *msg);
int
udpdev_rsrvp(P_QUEUE *q);
int
udpdev_init();
int
udpdev_wput_data(P_QUEUE *q, P_MSGB *msg);
int
udpdev_wsnd(P_QUEUE *q, P_MSGB *msg);
int
udpdev_wput_ctl(P_QUEUE *q, P_MSGB *msg);
static UDPDEVAREA *
udpdev_getarea(void *key);

#endif
