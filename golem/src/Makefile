all: golem connect.telnet

CC = gcc

GOLEM = golem.o input.o tasks.o loew.o loewlib.o search.o

LOGALLOCH = # -include logalloc.h
LOGALLOCO = # logalloc.c

DEBUG  = -DDIAGNOSTIC
CFLAGS = -Wall -pedantic -ansi $(DEBUG) #-Wmissing-prototypes

all:	golem connect.telnet

clean:	
	rm -f *.o golem testsearch

.c.o:
	$(CC) -g $(CFLAGS) $(LOGALLOCH) -c $*.c

golem: $(GOLEM)
	$(CC) -o golem $(GOLEM) $(LOGALLOCO)

golem.o: golem.c golem.h loew.h
input.o: input.c golem.h loew.h
tasks.o: tasks.c golem.h loew.h
loewlib.o: loewlib.c golem.h loew.h search.h
loew.o: loew.c loew.h
search.o: search.c search.h
testsearch.o: testsearch.c

connect.telnet: connect.telnet.c
	$(CC) -o connect.telnet connect.telnet.c

loew: loew.c loew.h
	$(CC) -g -Wall -DTESTMODE -o loew loew.c

testsearch: testsearch.o search.o loew.o loewlib.o  input.o tasks.o golem.o
