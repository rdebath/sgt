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

all: reservoir.1
man: reservoir.1
progs:;

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 *.html *.tar.gz

html:
	halibut --html=reservoir.html reservoir.but
	mv reservoir.html $(DESTDIR)

release: reservoir.1
	mkdir -p reltmp/reservoir
	ln -s ../../reservoir reltmp/reservoir
	ln -s ../../reservoir.1 reltmp/reservoir
	ln -s ../../reservoir.but reltmp/reservoir
	ln -s ../../Makefile reltmp/reservoir
	tar -C reltmp -chzf $(DESTDIR)/reservoir.tar.gz reservoir
	rm -rf reltmp

install: install-progs install-man
install-progs:
	mkdir -p $(SCRIPTDIR)
	$(INSTALL) $(IPROG) reservoir $(SCRIPTDIR)/reservoir
install-man: reservoir.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) reservoir.1 $(MANDIR)/reservoir.1
