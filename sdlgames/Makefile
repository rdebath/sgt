COMPILE = $(CC) $(CFLAGS) -I. -MD -c -o $@ $<

sdlgames: selector.o linuxrc.o nort.o sumo.o \
          sdlstuff.o game256.o swash.o beebfont.o
	$(CC) $(LFLAGS) -o $@ $^ -lSDL -lpthread

selector.o: selector.c; $(COMPILE)
sumo.o: sumo/sumo.c; $(COMPILE) -Dmain=sumo_main
nort.o: nort/nort.c; $(COMPILE) -Dmain=nort_main
linuxrc.o: linuxrc/linuxrc.c; $(COMPILE) -Dmain=linuxrc_main
sdlstuff.o: sdlstuff.c; $(COMPILE)
game256.o: game256.c; $(COMPILE)
swash.o: swash.c; $(COMPILE)
beebfont.o: beebfont.c; $(COMPILE)

clean:
	rm -f sdlgames *.o *.d

-include *.d
