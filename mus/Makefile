CC = gcc
CFLAGS = -Wall -O2 -g# -include /home/simon/src/lib/logalloc/logalloc.h
LDFLAGS = #/home/simon/src/lib/logalloc/logalloc.o
INSTALL = install
BINDIR = ${HOME}/bin

.c.o:
	$(CC) $(CFLAGS) -c $*.c

target: .depend mus

all: .depend mus hotspot.txt

install: .depend mus
	$(INSTALL) -s mus $(BINDIR)

OBJ = beams.o contin.o drawing.o header.o input.o measure.o melody.o misc.o \
      mus.o notes.o preproc.o prologue.o pscript.o slurs.o
SRC = beams.c contin.c drawing.c header.c input.c measure.c melody.c misc.c \
      mus.c notes.c preproc.c prologue.c pscript.c slurs.c
HDR = beams.h contin.h drawing.h header.h input.h measure.h melody.h misc.h \
      mus.h notes.h preproc.h prologue.h pscript.h slurs.h

dep: .depend

.depend: $(SRC) $(HDR)
	gcc -MM $(SRC) >.depend

clean::
	rm -f *.o core
spotless:: clean
	rm -f prologue.c hotspot.txt mus .depend

hotspot.txt: hotspot.ps prologue.ps
	gs -sDEVICE=bit -dQUIET hotspot.ps quit.ps >hotspot.txt

tarball::
	rm -rf mus
	mkdir mus
	cd mus; ln -s ../*.c ../*.h ../Makefile ../sample.mus .
	tar chzf mus.tar.gz mus
	rm -rf mus

mus: $(OBJ)
	$(CC) $(LDFLAGS) -o mus $(OBJ)

ifeq (.depend,$(wildcard .depend))
include .depend
endif
