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

all: nntpid.1
man: nntpid.1
progs:;

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 *.html *.tar.gz

html:
	halibut --html=$(DESTDIR)/nntpid.html nntpid.but

release: nntpid.1
	mkdir -p reltmp/nntpid
	ln -s ../../nntpid reltmp/nntpid
	ln -s ../../nntpid.1 reltmp/nntpid
	ln -s ../../nntpid.but reltmp/nntpid
	ln -s ../../Makefile reltmp/nntpid
	tar -C reltmp -chzf $(DESTDIR)/nntpid.tar.gz nntpid
	rm -rf reltmp

install: install-progs install-man
install-progs:
	mkdir -p $(SCRIPTDIR)
	$(INSTALL) $(IPROG) nntpid $(SCRIPTDIR)/nntpid
install-man: nntpid.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) nntpid.1 $(MANDIR)/nntpid.1
