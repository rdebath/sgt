DESTDIR = .

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
