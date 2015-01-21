CC = gcc
CCFLAGS += -g
SRCS = 	listop.c pstreams.c pstreams_echo.c tcpdev.c stdmod.c udpdev.c echotest.c

OBJS =		$(SRCS:.c=.o)
HDRS =		$(SRCS:.c=.h)

LIBS =

TARGET =	echo_test


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
