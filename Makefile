TARGETS = chatserver chatclient

all: $(TARGETS)

%.o: %.cc
	g++ $^ --std=c++11 -g -c -o $@

chatserver: chatserver.o cs_common.o cs_client.o
	g++ $^ -o $@

chatclient: chatclient.o
	g++ $^ -o $@

pack:
	rm -f submit-hw3.zip
	zip -r submit-hw3.zip README Makefile *.c* *.h*

clean::
	rm -fv $(TARGETS) *~ *.o submit-hw3.zip

realclean:: clean
	rm -fv cis505-hw3.zip
