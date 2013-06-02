# Makefile for v.

FLAVOUR = imlib2

ifeq ($(FLAVOUR),imlib1)
# gdk-imlib1 won't work with 2.0.
GTK_CONFIG = gtk-config
LIBS = -lgdk_imlib
else
GTK_CONFIG = pkg-config gtk+-2.0 imlib2
LIBS = `$(GTK_CONFIG) --libs` -lX11
endif

CFLAGS = -O2 -Wall -Werror -g -I./ `$(GTK_CONFIG) --cflags`

%.o: %.c
	$(CC) $(CFLAGS) $(XFLAGS) -c $<

all: v

V = v.o malloc.o bumf.o $(FLAVOUR).o
v: $(V)
	$(CC) $(LDFLAGS) -o $@ $(V) $(LIBS)

malloc.o: ./malloc.c ./v.h
imlib1.o: ./imlib1.c ./v.h
imlib2.o: ./imlib2.c ./v.h
v.o: ./v.c ./v.h
bumf.o: ./bumf.c ./v.h

clean:
	rm -f *.o v
