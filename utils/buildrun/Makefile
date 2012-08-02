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

all: buildrun.1 buildrun
progs: buildrun
man: buildrun.1

buildrun: buildrun.c
	$(CC) $(CFLAGS) -o $@ $<

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 buildrun *.html *.tar.gz

html:
	halibut --html=buildrun.html buildrun.but
	mv buildrun.html $(DESTDIR)

release: buildrun.1
	mkdir -p reltmp/buildrun
	ln -s ../../buildrun.c reltmp/buildrun
	ln -s ../../buildrun.1 reltmp/buildrun
	ln -s ../../buildrun.but reltmp/buildrun
	ln -s ../../Makefile reltmp/buildrun
	tar -C reltmp -chzf $(DESTDIR)/buildrun.tar.gz buildrun
	rm -rf reltmp

install: install-progs install-man
install-progs: buildrun
	mkdir -p $(BINDIR)
	$(INSTALL) $(IPROG) buildrun $(BINDIR)/buildrun
install-man: buildrun.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) buildrun.1 $(MANDIR)/buildrun.1
