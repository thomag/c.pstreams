# c.pstreams
Portable implementation of an 'Unix Streams' like framework.

See Ritchie, D.M.,  “A Stream Input-Output System”, AT&T Bell Labs Technical Journal, vol. 63, pp. 311-324, Oct. 1984. 
See also http://cm.bell-labs.com/cm/cs/who/dmr/st.html

Includes a sample module implementing the Stop-And-Wait protocol in saw.h/.c

Tracing test.c shows how the sample saw module is pushed into the stack and a test message loops thru (UDP loopback) the stack.

pstreams.pdf here has some pictorial explanation.
