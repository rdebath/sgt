# for `make release' and `make html'
DESTDIR = .

# for `make install'
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man/man1

all: nntpid.1

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

install: nntpid.1
	mkdir -p $(BINDIR)
	install nntpid $(BINDIR)/nntpid
	mkdir -p $(MANDIR)
	install -m 0644 nntpid.1 $(MANDIR)/nntpid.1
