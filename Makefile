CC := gcc
CFLAGS := -g -c -Wall $(XFLAGS)
LINK := gcc
LFLAGS :=
LIBS := 

TWEAK := main.o keytab.o actions.o search.o rcfile.o buffer.o btree.o

ifeq ($(SLANG),yes)
# INCLUDE += -I/path/to/slang/include
# LIBS += -L/path/to/slang/lib
LIBS += -lslang
TWEAK += slang.o
else
LIBS += -lncurses
TWEAK += curses.o
endif

.c.o:
	$(CC) $(CFLAGS) $*.c

all: tweak tweak.1

tweak:	$(TWEAK)
	$(LINK) -o tweak $(TWEAK) $(LIBS)

tweak.1:  manpage.but
	halibut --man=$@ $<

clean:
	rm -f *.o tweak

main.o: main.c tweak.h
keytab.o: keytab.c tweak.h
actions.o: actions.c tweak.h
search.o: search.c tweak.h
rcfile.o: rcfile.c tweak.h
buffer.o: buffer.c tweak.h btree.h
slang.o: slang.c tweak.h
curses.o: curses.c tweak.h
btree.o: btree.c btree.h
