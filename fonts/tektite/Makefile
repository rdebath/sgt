all: tektite.fon tektitec.fon

tektite.fon: tektite.fd
	../winfont/mkwinfont -o tektite.fon tektite.fd
tektitec.fon: tektitec.fd
	../winfont/mkwinfont -o tektitec.fon tektitec.fd

clean:
	rm -f *.fon
