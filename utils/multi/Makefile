# for `make release' and `make html'
DESTDIR = .

# for `make install'
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man/man1
INSTALL = install
IPROG =#   flags for installing programs (default none)
IDATA = -m 0644  # flags for installing data

all: multi.1

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 *.html *.tar.gz

html:
	halibut --html=$(DESTDIR)/multi.html multi.but

release: multi.1
	mkdir -p reltmp/multi
	ln -s ../../multi reltmp/multi
	ln -s ../../multi.1 reltmp/multi
	ln -s ../../multi.but reltmp/multi
	ln -s ../../Makefile reltmp/multi
	tar -C reltmp -chzf $(DESTDIR)/multi.tar.gz multi
	rm -rf reltmp

install: multi.1
	mkdir -p $(BINDIR)
	$(INSTALL) $(IPROG) multi $(BINDIR)/multi
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) multi.1 $(MANDIR)/multi.1
