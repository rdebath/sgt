OBJECTS = main.o screen.o engine.o memory.o levelfile.o misc.o savefile.o

enigma: $(OBJECTS)
	gcc -o enigma $(OBJECTS) -lncurses

.c.o:
	gcc -g -c $*.c
