COMPILE = $(CC) $(CFLAGS) -MD -c -o $@ $<

filter: main.o pty.o
	$(CC) $(LFLAGS) -o $@ $^

main.o: main.c; $(COMPILE)
pty.o: pty.c; $(COMPILE)

clean:
	rm -f filter *.o *.d

-include *.d
