CC = gcc
CFLAGS = -g -c -Wall -I/home/sgtatham/include
LINK = gcc
LFLAGS =
LIBS = -L/home/sgtatham/lib -lslang

AXE = axe.o axektab.o axeacts.o axesrch.o axerc.o axebuf.o btree.o

.c.o:
	$(CC) $(CFLAGS) $*.c

axe:	$(AXE)
	$(LINK) -o axe $(AXE) $(LIBS)

clean:
	rm -f *.o axe

axe.o: axe.c axe.h
axektab.o: axektab.c axe.h
axeacts.o: axeacts.c axe.h
axesrch.o: axesrch.c axe.h
axerc.o: axerc.c axe.h
axebuf.o: axebuf.c axe.h btree.h
btree.o: btree.c btree.h
