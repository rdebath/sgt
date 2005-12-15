COMPILE = $(CC) $(CFLAGS) -MD -c -o $@ $<

all: nullfilter csfilter nhfilter

nullfilter: main.o pty.o nulltrans.o
	$(CC) $(LFLAGS) -o $@ $^

csfilter: main.o pty.o cstrans.o ../charset/libcharset.a
	$(CC) $(LFLAGS) -o $@ $^

nhfilter: main.o pty.o nhtrans.o
	$(CC) $(LFLAGS) -o $@ $^

main.o: main.c; $(COMPILE)
pty.o: pty.c; $(COMPILE)
nulltrans.o: nulltrans.c; $(COMPILE)
cstrans.o: cstrans.c; $(COMPILE) -I../charset
nhtrans.o: nhtrans.c; $(COMPILE)

clean:
	rm -f filter *.o *.d

-include *.d
