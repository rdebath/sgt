COMPILE = $(CC) $(CFLAGS) -MD -c -o $@ $<

all: nullfilter csfilter nhfilter record idlewrapper deidle

nullfilter: main.o pty.o nulltrans.o
	$(CC) $(LFLAGS) -o $@ $^

csfilter: main.o pty.o cstrans.o ../charset/libcharset.a
	$(CC) $(LFLAGS) -o $@ $^

nhfilter: main.o pty.o nhtrans.o
	$(CC) $(LFLAGS) -o $@ $^

record: main.o pty.o record.o
	$(CC) $(LFLAGS) -o $@ $^

idlewrapper: main.o pty.o idletrans.o
	$(CC) $(LFLAGS) -o $@ $^

deidle: main.o pty.o deidle.o
	$(CC) $(LFLAGS) -o $@ $^

main.o: main.c; $(COMPILE)
pty.o: pty.c; $(COMPILE)
nulltrans.o: nulltrans.c; $(COMPILE)
cstrans.o: cstrans.c; $(COMPILE) -I../charset
nhtrans.o: nhtrans.c; $(COMPILE)
record.o: record.c; $(COMPILE)

clean:
	rm -f filter *.o *.d

-include *.d
