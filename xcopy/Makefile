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

all: xcopy.1 xcopy
man: xcopy.1
progs: xcopy

xcopy: xcopy.c
	$(CC) $(CFLAGS) -o $@ $< $(X11LIB)

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 xcopy *.html *.tar.gz

html:
	halibut --html=$(DESTDIR)/xcopy.html xcopy.but

release: xcopy.1
	mkdir -p reltmp/xcopy
	ln -s ../../xcopy.c reltmp/xcopy
	ln -s ../../xcopy.1 reltmp/xcopy
	ln -s ../../xcopy.but reltmp/xcopy
	ln -s ../../Makefile reltmp/xcopy
	tar -C reltmp -chzf $(DESTDIR)/xcopy.tar.gz xcopy
	rm -rf reltmp

install: install-progs install-man

install-progs: xcopy
	mkdir -p $(BINDIR)
	$(INSTALL) $(IPROG) xcopy $(BINDIR)/xcopy
install-man: xcopy.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) xcopy.1 $(MANDIR)/xcopy.1
