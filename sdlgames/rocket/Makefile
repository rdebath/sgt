COMPILE = $(CC) $(PS2) $(CFLAGS) -I.. -MD -c -o $@ $<

rocket: rocket.o rgraph.o sdlstuff.o data.o game256.o utils.o
	$(CC) $(LFLAGS) -o $@ $^ -lSDL -lpthread

rocket.o: rocket.c; $(COMPILE)
rgraph.o: rgraph.c; $(COMPILE)
sdlstuff.o: ../sdlstuff.c; $(COMPILE)
data.o: ../data.c; $(COMPILE)
game256.o: ../game256.c; $(COMPILE)
utils.o: ../utils.c; $(COMPILE)

clean:
	rm -f rocket *.o *.d

-include *.d
