COMPILE = $(CC) $(CFLAGS) -I.. -MD -c -o $@ $<

linuxrc: linuxrc.o sdlstuff.o game256.o swash.o beebfont.o
	$(CC) $(LFLAGS) -o $@ $^ -lSDL -lpthread

linuxrc.o: linuxrc.c; $(COMPILE)
sdlstuff.o: ../sdlstuff.c; $(COMPILE)
game256.o: ../game256.c; $(COMPILE)
swash.o: ../swash.c; $(COMPILE)
beebfont.o: ../beebfont.c; $(COMPILE)

clean:
	rm -f linuxrc *.o *.d

-include *.d
