all: fixed.fd fixedc.fd fixed.fon fixedc.fon

fixed.fd fixedc.fd: 6x13.bdf genfixed.py
	python genfixed.py

fixed.fon: fixed.fd
	../winfont/mkwinfont -o fixed.fon fixed.fd
fixedc.fon: fixedc.fd
	../winfont/mkwinfont -o fixedc.fon fixedc.fd

clean:
	rm -f *.fd *.fon
