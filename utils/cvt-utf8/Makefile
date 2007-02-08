# for `make release' and `make html'
DESTDIR = .

# for `make install'
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
SCRIPTDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man/man1
INSTALL = install

all: cvt-utf8.1
man: cvt-utf8.1
progs:;

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 *.html *.tar.gz

html:
	halibut --html=cvt-utf8.html cvt-utf8.but
	mv cvt-utf8.html $(DESTDIR)

release: cvt-utf8.1
	mkdir -p reltmp/cvt-utf8
	ln -s ../../cvt-utf8 reltmp/cvt-utf8
	ln -s ../../cvt-utf8.1 reltmp/cvt-utf8
	ln -s ../../cvt-utf8.but reltmp/cvt-utf8
	ln -s ../../Makefile reltmp/cvt-utf8
	tar -C reltmp -chzf $(DESTDIR)/cvt-utf8.tar.gz cvt-utf8
	rm -rf reltmp

install: install-progs install-man
install-progs:
	mkdir -p $(SCRIPTDIR)
	$(INSTALL) $(IPROG) cvt-utf8 $(SCRIPTDIR)/cvt-utf8
install-man: cvt-utf8.1
	mkdir -p $(MANDIR)
	$(INSTALL) $(IDATA) cvt-utf8.1 $(MANDIR)/cvt-utf8.1
