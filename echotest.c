#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdlib.h>
#include "pstreams.h"
#include <unistd.h>
#include "stdmod.h"
#include "pstreams_echo.h"

extern P_STREAMTAB echo_streamtab;

int MSGSIZE=85;
FILE *ltfile=NULL;

char ltfilename[]="/tmp/pstreamslog.txt";
int max_msgs=1;
int max_tries=10;

int
main()
{
	P_STREAMHEAD *strm=NULL;
	
	unsigned char mydata[2048] = {0};
	unsigned char readdata[2048]={0};
	P_BUF	mydatabuf = {0};
	P_BUF  readbuf={0};
	P_BUF ctlbuf={0};

	mydatabuf.maxlen=sizeof(mydata);
	/*setting up mydatabuf*/
	int ii=0;
	for(ii=0; ii<MSGSIZE; ii++)
	{
		static char c='0';
		mydata[ii]=c;
		c++;
		if(c > '9')
		{
			c = '0';
		}
	}
	mydata[MSGSIZE]='\0';
	mydatabuf.len = MSGSIZE;
	mydatabuf.buf = mydata;

	readbuf.maxlen = sizeof(readdata);
	readbuf.len = 0;
	readbuf.buf = readdata;

	echo_init();

	strm = pstreams_open(P_UDP);
	assert(strm);

	ltfile = pstreams_setltfile(strm, ltfilename);
	if(!ltfile)
	{
		printf("Cannot open logtrace file %s. exit\n",
			ltfilename);
		return 0;
	}

	fprintf(ltfile, "\nOpened stream. logtrace file: %s\n",
		ltfilename);
	printf("\nOpened stream...\n");

	fprintf(ltfile, "\nPushing echo module in...\n");
	printf("\nPushing echo module in...\n");
	pstreams_push(strm, &echo_streamtab);


	sprintf(mydata, "MSG%2d", max_msgs);
	mydata[strlen(mydata)]=' ';/*overwrite '\0'*/

	fprintf(ltfile, "\nPutting %d bytes into streamhead : \n%s\n", 
		mydatabuf.len, (char *)mydatabuf.buf);
	printf("\nPutting %d bytes into streamhead : \n%s\n", 
		mydatabuf.len, (char *)mydatabuf.buf);
	{
		if(pstreams_putmsg(strm, NULL, &mydatabuf, 0) != P_STREAMS_SUCCESS)
		{
			fprintf(ltfile, "\nmain: couldn't send msg\n");
			printf("\nmain: couldn't send msg\n");
		}
	}

	readbuf.len=0;

	while(readbuf.len == 0)
	{
		int getflags=0;

		fprintf(ltfile, "\nReading from streamhead...\n");
		printf("\nReading from streamhead...\n");
		pstreams_getmsg(strm, NULL, &readbuf, &getflags);

		if(readbuf.len == 0)
		{
			fprintf(ltfile, "\nstreamhead is empty\n");
			printf("\nstreamhead is empty\n");
		}
		else
		{
			int cmpval = 0;
			readdata[readbuf.len] = '\0';
			fprintf(ltfile, "\nstreamhead has %d bytes: \n%s\n", 
				readbuf.len, (char *)readbuf.buf);
			printf("\nstreamhead has %d bytes: \n%s\n", 
				readbuf.len, (char *)readbuf.buf);
			usleep(100);
			if(readbuf.len != MSGSIZE)
			{
				fprintf(ltfile, "\nNO MATCH - wrong length");
				printf("\nNO MATCH - wrong length");
			}
			else if(cmpval = memcmp(mydatabuf.buf, readbuf.buf, MSGSIZE))
			{
				fprintf(ltfile, "\nNO MATCH - data mismatch. memcmp returned %d\n", cmpval);
				printf("\nNO MATCH - data mismatch. memcmp returned %d\n", cmpval);
			}
			else
			{
				fprintf(ltfile, "\n SUCCESS - MATCHED \n");
				printf("\n SUCCESS - MATCHED \n");
			}
		}
		
		pstreams_callsrvp(strm);

		if(!--max_tries)
		{
			break;
		}
		fprintf(ltfile, "       LOOP %d                      ", max_tries);
		printf("       LOOP %d                      ", max_tries);

		usleep(100);

	}

	if(--max_msgs)
	{
		usleep(200);
		fprintf(ltfile, "\nSECOND MSG\n");
		max_tries=30;
	}

	return 0;
}
	
