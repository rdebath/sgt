CC = gcc
CFLAGS = -g -c -Wall -I/home/sgtatham/include
LINK = gcc
LFLAGS =
LIBS = -L/home/sgtatham/lib -lslang

AXE = axe.o axektab.o axeacts.o axesrch.o axerc.o nca.o

.c.o:
	$(CC) $(CFLAGS) $*.c

axe:	$(AXE)
	$(LINK) -o axe $(AXE) $(LIBS)

clean:
	rm -f *.o axe

axe.o: axe.c axe.h nca.h
axektab.o: axektab.c axe.h
axeacts.o: axeacts.c axe.h nca.h
axesrch.o: axesrch.c axe.h nca.h
axerc.o: axerc.c axe.h nca.h
nca.o: nca.c nca.h
