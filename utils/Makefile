SUBDIRS = base64 cvt-utf8 lns multi nntpid xcopy

# for `make html' and `make release'; should be a relative path
DESTDIR = .

# for `make install'; should be absolute paths
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man/man1

all:
	for i in $(SUBDIRS); do make -C $$i; done

clean:
	rm -f *.html *.tar.gz
	for i in $(SUBDIRS); do make -C $$i clean; done

html:
	for i in $(SUBDIRS); do make -C $$i html DESTDIR=../$(DESTDIR); done

release:
	for i in $(SUBDIRS); do make -C $$i release DESTDIR=../$(DESTDIR); done

install:
	for i in $(SUBDIRS); do \
	    make -C $$i install; \
	done
