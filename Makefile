CC := gcc
CFLAGS := -g -c -Wall $(XFLAGS)
LINK := gcc
LFLAGS :=
LIBS := 

AXE := axe.o axektab.o axeacts.o axesrch.o axerc.o axebuf.o btree.o

ifeq ($(SLANG),yes)
# INCLUDE += -I/path/to/slang/include
# LIBS += -L/path/to/slang/lib
LIBS += -lslang
AXE += axeslang.o
else
LIBS += -lncurses
AXE += axecurs.o
endif

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
axeslang.o: axeslang.c axe.h
axecurs.o: axecurs.c axe.h
btree.o: btree.c btree.h
