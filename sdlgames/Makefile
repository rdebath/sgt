COMPILE = $(CC) $(PS2) $(CFLAGS) -I. -MD -c -o $@ $<

sdlgames: selector.o linuxrc.o nort.o sumo.o \
          sdlstuff.o game256.o swash.o beebfont.o data.o utils.o \
	  ntris.o ntrissdl.o rocket.o rgraph.o
	$(CC) $(LFLAGS) -o $@ $^ -lSDL -lpthread

selector.o: selector.c; $(COMPILE)
sumo.o: sumo/sumo.c; $(COMPILE) -Dmain=sumo_main
nort.o: nort/nort.c; $(COMPILE) -Dmain=nort_main
ntris.o: ntris/ntris.c; $(COMPILE)
ntrissdl.o: ntris/ntrissdl.c; $(COMPILE) -Dmain=ntris_main
linuxrc.o: linuxrc/linuxrc.c; $(COMPILE) -Dmain=linuxrc_main
rocket.o: rocket/rocket.c; $(COMPILE) -Dmain=rocket_main
rgraph.o: rocket/rgraph.c; $(COMPILE)
sdlstuff.o: sdlstuff.c; $(COMPILE)
game256.o: game256.c; $(COMPILE)
swash.o: swash.c; $(COMPILE)
data.o: data.c; $(COMPILE)
utils.o: utils.c; $(COMPILE)
beebfont.o: beebfont.c; $(COMPILE)

clean:
	rm -f sdlgames *.o *.d

-include *.d
