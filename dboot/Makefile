dboot: dboot.o data.o
	cc -o dboot dboot.o data.o

dboot.o: dboot.c
	cc -c dboot.c

data.o: data.c
	cc -c data.c

data.c: makeloader bootsect loader
	./makeloader bootsect loader >data.c

bootsect: bootsect.asm
	nasm -f bin bootsect.asm

loader: loader.asm
	nasm -f bin loader.asm

makeloader: makeloader.c
	cc -o makeloader makeloader.c

clean:
	rm -f *.o data.c makeloader dboot.tar.gz

distclean: clean
	rm -f dboot

spotless: distclean
	rm -f bootsect loader

install: dboot
	cp dboot /usr/local/sbin/dboot

dist: bootsect loader
	sh makedist.sh
