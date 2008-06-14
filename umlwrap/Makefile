# Makefile for umlwrap.

prefix = /usr/local
libdir = $(prefix)/lib
ourlibdir = $(prefix)/lib/umlwrap
bindir = $(prefix)/bin

INSTALL = install

CFLAGS = -Wall

-include Makefile.local

UMLWRAP_MODULES := umlwrap options sel malloc protocol isdup rawmode fgetline
UMLWRAP_OBJS := $(patsubst %,%.o,$(UMLWRAP_MODULES))

INIT_MODULES := init sel malloc protocol rawmode movefds
INIT_OBJS := $(patsubst %,%.o,$(INIT_MODULES))

ALLMODULES := $(sort $(UMLWRAP_MODULES) $(INIT_MODULES))
ALLOBJS := $(patsubst %,%.o,$(ALLMODULES))
ALLDEPS := $(patsubst %,%.d,$(ALLMODULES))

umlwrap: $(UMLWRAP_OBJS)
	gcc $(LFLAGS) -o umlwrap $(UMLWRAP_OBJS)

init: $(INIT_OBJS)
	gcc $(LFLAGS) -static -o init $(INIT_OBJS)

INTERNALFLAGS=#
umlwrap.o : override INTERNALFLAGS=-DUMLROOT="\"$(ourlibdir)\""

$(ALLOBJS): %.o: %.c
	gcc $(CFLAGS) -MM $*.c > $*.d
	gcc $(CFLAGS) $(INTERNALFLAGS) -c $*.c

install: umlwrap init
	mkdir -p $(ourlibdir)
	mkdir -p $(ourlibdir)/tmp
	mkdir -p $(ourlibdir)/dev
	mkdir -p $(ourlibdir)/sbin
	$(INSTALL) -m 0755 init $(ourlibdir)/sbin/init
	$(INSTALL) -m 0755 umlwrap $(bindir)/umlwrap

clean:
	rm -f umlwrap init $(ALLOBJS) $(ALLDEPS)

-include $(ALLDEPS)
