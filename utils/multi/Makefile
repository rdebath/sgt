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

all: multi.1
man: multi.1
progs:;

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 *.html *.tar.gz

html:
	halibut --html=multi.html multi.but
	mv multi.html $(DESTDIR)

release: multi.1
	mkdir -p reltmp/multi
	ln -s ../../multi reltmp/multi
	ln -s ../../multi.1 reltmp/multi
	ln -s ../../multi.but reltmp/multi
	ln -s ../../Makefile reltmp/multi
	tar -C reltmp -chzf $(DESTDIR)/multi.tar.gz multi
	rm -rf reltmp

install: install-progs install-man
install-progs:
	mkdir -p $(SCRIPTDIR)
	$(INSTALL) $(IPROG) multi $(SCRIPTDIR)/multi
install-man: multi.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) multi.1 $(MANDIR)/multi.1
