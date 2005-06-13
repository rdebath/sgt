# Makefile for v.

# Currently we require gtk-1.2 because we use gdk-imlib1 which
# won't work with 2.0. Ideally I'd like to move up to supporting
# either of that and Imlib2 (which will take some ifdefs or a
# porting layer, but doesn't otherwise look too hard).
GTK_CONFIG = gtk-config

CFLAGS = -O2 -Wall -Werror -g -I./ `$(GTK_CONFIG) --cflags`
LDFLAGS = `$(GTK_CONFIG) --libs`

%.o: %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c $<

all: v

V = v.o malloc.o bumf.o imlib1.o
v: $(V)
	$(CC) $(LDFLAGS) -o $@ $(V) -lgdk_imlib

malloc.o: ./malloc.c ./v.h
imlib1.o: ./imlib1.c ./v.h
v.o: ./v.c ./v.h
bumf.o: ./bumf.c ./v.h

clean:
	rm -f *.o v
