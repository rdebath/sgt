SUBDIRS_NOX = after base64 buildrun cvt-utf8 lns multi nntpid \
              pid reservoir
SUBDIRS = $(SUBDIRS_NOX) beep xcopy

# for `make html' and `make release'; should be a relative path
DESTDIR = .

# for `make install'; should be absolute paths
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
SCRIPTDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man/man1
INSTALL = install
IPROG =#   flags for installing programs (default none)
IDATA = -m 0644  # flags for installing data

all:
	for i in $(SUBDIRS); do (cd $$i && make); done

all-nox:
	for i in $(SUBDIRS_NOX); do (cd $$i && make); done

progs:
	for i in $(SUBDIRS); do (cd $$i && make progs); done

progs-nox:
	for i in $(SUBDIRS_NOX); do (cd $$i && make progs); done

man:
	for i in $(SUBDIRS); do (cd $$i && make man); done

clean:
	rm -f *.html *.tar.gz
	for i in $(SUBDIRS); do (cd $$i && make clean); done

html:
	for i in $(SUBDIRS); do (cd $$i && make html DESTDIR=../$(DESTDIR)); done

release:
	for i in $(SUBDIRS); do (cd $$i && make release DESTDIR=../$(DESTDIR)); done

install:
	for i in $(SUBDIRS); do (cd $$i && make install); done

install-nox:
	for i in $(SUBDIRS_NOX); do (cd $$i && make install); done

install-progs:
	for i in $(SUBDIRS); do (cd $$i && make install-progs); done

install-progs-nox:
	for i in $(SUBDIRS_NOX); do (cd $$i && make install-progs); done

install-man:
	for i in $(SUBDIRS); do (cd $$i && make install-man); done

install-man-nox:
	for i in $(SUBDIRS_NOX); do (cd $$i && make install-man); done
