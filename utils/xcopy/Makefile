X11LIB=-L/usr/X11R6/lib -lX11

all: xcopy.1 xcopy

xcopy: xcopy.c
	$(CC) $(CFLAGS) -o $@ $< $(X11LIB)

%.1: %.but
	halibut --man=$@ $<

clean:
	rm -f *.1 xcopy
