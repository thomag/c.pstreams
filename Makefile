CC = gcc
CCFLAGS += -g
SRCS = 	envlinux.c listop.c pstreams.c pstreams_echo.c saw.c stdmod.c tcpdev.c test.c testutil.c udpdev.c util.c

OBJS =		$(SRCS:.c=.o)
HDRS =		$(SRCS:.c=.h)

LIBS =

TARGET =	test


all:	$(CHECKHDR) $(TARGET)

list: $(SRCS)
	ls $^

depend: .depend
.depend: $(SRCS)
	rm -f ./.depend
	$(CC) $(CCFLAGS) -MM $^ >  ./.depend;

include .depend

$(CHECKHDR): $(HDRS)
	rm -f $(OBJS) $(TARGET)

$(TARGET):	$(OBJS)
	$(CC) $(CCFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(TARGET)
