# for `make release' and `make html'
DESTDIR = .

# for `make install'
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man/man1
INSTALL = install
IPROG =#   flags for installing programs (default none)
IDATA = -m 0644  # flags for installing data

all: lns.1

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 *.html *.tar.gz

html:
	halibut --html=$(DESTDIR)/lns.html lns.but

release: lns.1
	mkdir -p reltmp/lns
	ln -s ../../lns reltmp/lns
	ln -s ../../lns.1 reltmp/lns
	ln -s ../../lns.but reltmp/lns
	ln -s ../../Makefile reltmp/lns
	tar -C reltmp -chzf $(DESTDIR)/lns.tar.gz lns
	rm -rf reltmp

install: lns.1
	mkdir -p $(BINDIR)
	$(INSTALL) $(IPROG) lns $(BINDIR)/lns
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) lns.1 $(MANDIR)/lns.1
