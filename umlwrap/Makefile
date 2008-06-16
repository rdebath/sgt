# Makefile for umlwrap.

prefix = /usr/local
libdir = $(prefix)/lib
ourlibdir = $(prefix)/lib/umlwrap
bindir = $(prefix)/bin
mandir = $(prefix)/man
man1dir = $(mandir)/man1

INSTALL = install

CFLAGS = -Wall

-include Makefile.local

UMLWRAP_MODULES := umlwrap options sel malloc protocol isdup rawmode \
                   fgetline licence
UMLWRAP_OBJS := $(patsubst %,%.o,$(UMLWRAP_MODULES))

INIT_MODULES := init sel malloc protocol rawmode movefds
INIT_OBJS := $(patsubst %,%.o,$(INIT_MODULES))

ALLMODULES := $(sort $(UMLWRAP_MODULES) $(INIT_MODULES))
ALLOBJS := $(patsubst %,%.o,$(ALLMODULES))
ALLDEPS := $(patsubst %,%.d,$(ALLMODULES))

binaries: umlwrap init

umlwrap: $(UMLWRAP_OBJS)
	gcc $(LFLAGS) -o umlwrap $(UMLWRAP_OBJS)

init: $(INIT_OBJS)
	gcc $(LFLAGS) -static -o init $(INIT_OBJS)

licence.c: LICENCE
	perl -pe 'BEGIN{print"const char *const licence[] = {\n"}' \
	     -e 's/["\\]/\\$$&/g;s/^/    "/;s/$$/\\n",/;' \
	     -e 'END{print"    0\n};\n"}' $< > $@

INTERNALFLAGS=#
umlwrap.o : override INTERNALFLAGS=-DUMLROOT="\"$(ourlibdir)\""

$(ALLOBJS): %.o: %.c
	gcc $(CFLAGS) -MM $*.c > $*.d
	gcc $(CFLAGS) $(INTERNALFLAGS) -c $*.c

umlwrap.1: umlwrap.but
	halibut --man=$@ $<

install: umlwrap init umlwrap.1
	mkdir -p $(ourlibdir)
	mkdir -p $(ourlibdir)/tmp
	mkdir -p $(ourlibdir)/dev
	mkdir -p $(ourlibdir)/sbin
	$(INSTALL) -m 0755 init $(ourlibdir)/sbin/init
	mkdir -p $(bindir)
	$(INSTALL) -m 0755 umlwrap $(bindir)/umlwrap
	mkdir -p $(man1dir)
	$(INSTALL) -m 0644 umlwrap.1 $(man1dir)/umlwrap.1

clean:
	rm -f umlwrap init $(ALLOBJS) $(ALLDEPS) licence.c

-include $(ALLDEPS)
