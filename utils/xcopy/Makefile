DESTDIR = .

X11LIB=-L/usr/X11R6/lib -lX11

all: xcopy.1 xcopy

xcopy: xcopy.c
	$(CC) $(CFLAGS) -o $@ $< $(X11LIB)

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 xcopy *.html *.tar.gz

html:
	halibut --html=$(DESTDIR)/xcopy.html xcopy.but

release: xcopy.1
	mkdir -p reltmp/xcopy
	ln -s ../../xcopy.c reltmp/xcopy
	ln -s ../../xcopy.1 reltmp/xcopy
	ln -s ../../xcopy.but reltmp/xcopy
	ln -s ../../Makefile reltmp/xcopy
	tar -C reltmp -chzf $(DESTDIR)/xcopy.tar.gz xcopy
	rm -rf reltmp


