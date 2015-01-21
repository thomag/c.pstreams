/*===========================================================================
FILE: testutil.h


 Activity:
 Date         Author           Comments
 Oct.09,2001  tgeorge          Created.

===========================================================================*/

#ifndef TESTUTIL_H
#define TESTUTIL_H

#include "pstreams.h"

/*public variables defined in the .c file*/
extern FILE *ltfile;

/*private*/
void init_global_buffers();
int setraddrs(P_STREAMHEAD *strm);

/*public*/
#ifdef __cplusplus
extern "C" {
#endif
P_STREAMHEAD *buildstream();
int service_strm(P_STREAMHEAD *strm);
void init_test();
int echotest(P_STREAMHEAD *strm, int count);
int send_echomsg(P_STREAMHEAD *strm);
int rcv_echomsg(P_STREAMHEAD *strm);
int service_strm(P_STREAMHEAD *strm);
int handle_msgin(P_STREAMHEAD *strm, P_BUF *cbuf, P_BUF *dbuf);

/*defined elsewhere*/
int mydisplay(const char *fmt,...);

#ifdef __cplusplus
}
#endif

#endif

