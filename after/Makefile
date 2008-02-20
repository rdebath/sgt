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

all: after.1 after
man: after.1
progs: after

after: after.c
	$(CC) $(CFLAGS) -o $@ $<

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 after *.html *.tar.gz

html:
	halibut --html=after.html after.but
	mv after.html $(DESTDIR)

release: after.1
	mkdir -p reltmp/after
	ln -s ../../after.c reltmp/after
	ln -s ../../after.1 reltmp/after
	ln -s ../../after.but reltmp/after
	ln -s ../../Makefile reltmp/after
	tar -C reltmp -chzf $(DESTDIR)/after.tar.gz after
	rm -rf reltmp

install: install-progs install-man

install-progs: after
	mkdir -p $(BINDIR)
	$(INSTALL) $(IPROG) after $(BINDIR)/after
install-man: after.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) after.1 $(MANDIR)/after.1
