COMPILE = $(CC) $(PS2) $(CFLAGS) -I.. -MD -c -o $@ $<

ntris: ntris.o ntrissdl.o sdlstuff.o beebfont.o utils.o
	$(CC) $(LFLAGS) -o $@ $^ -lSDL -lpthread

ntris.o: ntris.c; $(COMPILE)
ntrissdl.o: ntrissdl.c; $(COMPILE)
sdlstuff.o: ../sdlstuff.c; $(COMPILE)
beebfont.o: ../beebfont.c; $(COMPILE)
utils.o: ../utils.c; $(COMPILE)

clean:
	rm -f ntris *.o *.d

-include *.d
