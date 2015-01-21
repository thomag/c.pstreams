#include <stdio.h>
#include "testutil.h"

int main(int argc, char *argv[])
{
    int countOfMsgsToSend=1;/*number of messages to echo - default value*/

    P_STREAMHEAD *strm=(void *)0;

    init_global_buffers();
    init_test();

	switch(argc)
	{
	case 1:
		/*do nothing*/
		printf("Using default CountOfMsgsToSend of %d",
				countOfMsgsToSend);
		break;

	case 2:
		if(argc > 1)
		{
			sscanf(argv[1], "%d", &countOfMsgsToSend);
		}
		break;

	default:
		printf("Usage %s CountOfMsgsToSend\n", argv[0]);
		return -1;
	}

    strm = buildstream();

    echotest(strm, countOfMsgsToSend);

    return 0;
}
