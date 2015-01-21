#ifndef UDPDEV_H
#define UDPDEV_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ptypes.h"

/*
 * module specific local area
 */
typedef struct udpdevarea
{
	SOCKET sock;
	struct sockaddr_in laddr; /*local address*/
	struct sockaddr_in raddr; /*remote address*/
} UDPDEVAREA;

enum UDPDEV_DEFINES
{
	MAXUDPDGRAMSIZE=2048
};

int
udpdev_open(P_QUEUE *q);
int
udpdev_wput(P_QUEUE *q, P_MSGB *msg);
int
udpdev_rput(P_QUEUE *q, P_MSGB *msg);
int
udpdev_rsrvp(P_QUEUE *q);
int
udpdev_init();
int
udpdev_wput_data(P_QUEUE *q, P_MSGB *msg);
int
udpdev_wput_ctl(P_QUEUE *q, P_MSGB *msg);
static UDPDEVAREA *
udpdev_getarea(void *key);

#endif
