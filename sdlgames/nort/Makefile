COMPILE = gcc -I.. -MD -c -o $@ $<

nort: nort.o sdlstuff.o beebfont.o
	gcc -o $@ $^ -lSDL -lpthread

nort.o: nort.c; $(COMPILE)
sdlstuff.o: ../sdlstuff.c; $(COMPILE)
beebfont.o: ../beebfont.c; $(COMPILE)

-include *.d
