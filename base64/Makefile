DESTDIR = .

all: base64.1 base64

base64: base64.c
	$(CC) $(CFLAGS) -o $@ $<

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 base64 *.html *.tar.gz

html:
	halibut --html=$(DESTDIR)/base64.html base64.but

release: base64.1
	mkdir -p reltmp/base64
	ln -s ../../base64.c reltmp/base64
	ln -s ../../base64.1 reltmp/base64
	ln -s ../../base64.but reltmp/base64
	ln -s ../../Makefile reltmp/base64
	tar -C reltmp -chzvf $(DESTDIR)/base64.tar.gz base64
	rm -rf reltmp
