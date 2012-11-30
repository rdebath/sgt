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

all: pid.1 pid
man: pid.1
progs: pid

pid: pid.c
	$(CC) $(CFLAGS) -o $@ $<

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 pid *.html *.tar.gz

html:
	halibut --html=pid.html pid.but
	mv pid.html $(DESTDIR)

release: pid.1
	mkdir -p reltmp/pid
	ln -s ../../pid.c reltmp/pid
	ln -s ../../pid.1 reltmp/pid
	ln -s ../../pid.but reltmp/pid
	ln -s ../../Makefile reltmp/pid
	tar -C reltmp -chzf $(DESTDIR)/pid.tar.gz pid
	rm -rf reltmp

install: install-progs install-man

install-progs: pid
	mkdir -p $(BINDIR)
	$(INSTALL) $(IPROG) pid $(BINDIR)/pid
install-man: pid.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) pid.1 $(MANDIR)/pid.1
