OBJECTS = main.o screen.o engine.o memory.o levelfile.o

enigma: $(OBJECTS)
	gcc -o enigma $(OBJECTS) -lncurses

.c.o:
	gcc -g -c $*.c
