COMPILE = gcc -MD -c -o $@ $<

nort: nort.o sdlstuff.o
	gcc -o $@ $^ -lSDL -lpthread

nort.o: nort.c; $(COMPILE)
sdlstuff.o: ../sdlstuff.c; $(COMPILE)

-include *.d
