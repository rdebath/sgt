SUBDIRS = base64 cvt-utf8 multi xcopy
DESTDIR = .

all:
	for i in $(SUBDIRS); do make -C $$i; done

clean:
	rm -f *.html *.tar.gz
	for i in $(SUBDIRS); do make -C $$i clean; done

html:
	for i in $(SUBDIRS); do make -C $$i html DESTDIR=../$(DESTDIR); done

release:
	for i in $(SUBDIRS); do make -C $$i release DESTDIR=../$(DESTDIR); done
