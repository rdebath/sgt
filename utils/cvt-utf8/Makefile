DESTDIR = .

all: cvt-utf8.1

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 *.html *.tar.gz

html:
	halibut --html=$(DESTDIR)/cvt-utf8.html cvt-utf8.but

release: cvt-utf8.1
	mkdir -p reltmp/cvt-utf8
	ln -s ../../cvt-utf8 reltmp/cvt-utf8
	ln -s ../../cvt-utf8.1 reltmp/cvt-utf8
	ln -s ../../cvt-utf8.but reltmp/cvt-utf8
	ln -s ../../Makefile reltmp/cvt-utf8
	tar -C reltmp -chzf $(DESTDIR)/cvt-utf8.tar.gz cvt-utf8
	rm -rf reltmp
