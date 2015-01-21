#ifndef TCPDEV_H
#define TCPDEV_H

#include "options.h"

enum TCPDEV_DEFINES
{
    MAXTCPDGRAMSIZE=2048
};

typedef enum tcpdevstate
{
    TCPDEVSTATE_INIT, /*initial state, socket has to be opened*/
    TCPDEVSTATE_BIND, /*socket is open, local addr. has to be bound*/
    TCPDEVSTATE_CONNECT, /*bind was done, expecting to connect*/
    TCPDEVSTATE_DATA, /*connected, ready for data exchange*/
    TCPDEVSTATE_SNDDIS /*receive side disconnected. Send side needs to be disconnected*/
} TCPDEVSTATE;

typedef enum tcpdevflag
{
    TCPDEVFLAG_INIT        = 0x00000000,
    TCPDEVFLAG_LADDRSET = 0x00000001,
    TCPDEVFLAG_RADDRSET = 0x00000002
} TCPDEVFLAG;
/*
 * module specific local area
 */
typedef struct tcpdevarea
{
    SOCKET sock;
    TCPDEVSTATE state;
    TCPDEVFLAG flag;

    struct sockaddr_in laddr; /*local address*/
    struct sockaddr_in raddr; /*remote address*/
#ifdef PSTREAMS_WIN32
    WSADATA wsadata;
#endif
} TCPDEVAREA;

int
tcpdev_open(P_QUEUE *q);
int
tcpdev_wput(P_QUEUE *q, P_MSGB *msg);
int
tcpdev_rput(P_QUEUE *q, P_MSGB *msg);
int
tcpdev_rsrvp(P_QUEUE *q);
int
tcpdev_init();
int
tcpdev_wput_data(P_QUEUE *q, P_MSGB *msg);
int
tcpdev_wput_ctl(P_QUEUE *q, P_MSGB *msg);
static TCPDEVAREA *
tcpdev_getarea(void *key);

#endif
