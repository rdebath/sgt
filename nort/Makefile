COMPILE = $(CC) $(CFLAGS) -I.. -MD -c -o $@ $<

nort: nort.o sdlstuff.o beebfont.o
	$(CC) $(LFLAGS) -o $@ $^ -lSDL -lpthread

nort.o: nort.c; $(COMPILE)
sdlstuff.o: ../sdlstuff.c; $(COMPILE)
beebfont.o: ../beebfont.c; $(COMPILE)

clean:
	rm -f nort *.o *.d

-include *.d
