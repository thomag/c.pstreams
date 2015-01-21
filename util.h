#ifndef UTIL_H
#define UTIL_H


#define MAX_INT_AS_STR 2048

#include "pstreams.h"

/*function prototypes*/
void bintohex(uchar *hexbuf, uchar *binbuf, uint32 len);
void hextobin(uchar *binbuf, uchar *hexbuf, uint32 len);
uchar hextobin_nibble(uchar hexcode);
int fieldassign(uchar *to, uint32 from, int16 size);
void fieldread(void *to, void *from, int16 size);
short readshort(void *from);
long readlong(void *from);
int strtoi(int *i, char *str);
int arrtoi(int*i, char *arr, int16 len);
int itoarr(char *arr, int i, int16 len);
void senderror(P_QUEUE *q, uchar err);
void sendctl(P_QUEUE *q, uchar code, char *data, int16 datalen);
void sendproto(P_QUEUE *q, uchar code, char *data, int16 datalen);
void sendctlmsg(P_STREAMHEAD *strm, uchar code, char *data, int datalen);
#endif
