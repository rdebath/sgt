COMPILE = $(CC) $(CFLAGS) -I.. -MD -c -o $@ $<

sumo: sumo.o sdlstuff.o game256.o swash.o utils.o
	$(CC) $(LFLAGS) -o $@ $^ -lSDL -lpthread

sumo.o: sumo.c; $(COMPILE)
sdlstuff.o: ../sdlstuff.c; $(COMPILE)
game256.o: ../game256.c; $(COMPILE)
swash.o: ../swash.c; $(COMPILE)
utils.o: ../utils.c; $(COMPILE)

clean:
	rm -f sumo *.o *.d

-include *.d
