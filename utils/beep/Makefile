# for `make release' and `make html'
DESTDIR = .

# for `make install'
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
SCRIPTDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man/man1
INSTALL = install
IPROG =#   flags for installing programs (default none)
IDATA = -m 0644  # flags for installing data

X11LIB=-L/usr/X11R6/lib -lX11

all: beep.1 beep
man: beep.1
progs: beep

beep: beep.c
	$(CC) $(CFLAGS) -o $@ $< $(X11LIB)

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 beep *.html *.tar.gz

html:
	halibut --html=$(DESTDIR)/beep.html beep.but

release: beep.1
	mkdir -p reltmp/beep
	ln -s ../../beep.c reltmp/beep
	ln -s ../../beep.1 reltmp/beep
	ln -s ../../beep.but reltmp/beep
	ln -s ../../Makefile reltmp/beep
	tar -C reltmp -chzf $(DESTDIR)/beep.tar.gz beep
	rm -rf reltmp

install: install-progs install-man

install-progs: beep
	mkdir -p $(BINDIR)
	$(INSTALL) $(IPROG) beep $(BINDIR)/beep
install-man: beep.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) beep.1 $(MANDIR)/beep.1
