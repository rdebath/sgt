all: tektite.fon tektitec.fon tektite.pcf.Z tektite.pcf.gz

tektite.pcf.gz: tektite.bdf
	bdftopcf < tektite.bdf | gzip -9 > tektite.pcf.gz
tektite.pcf.Z: tektite.bdf
	bdftopcf < tektite.bdf | compress > tektite.pcf.Z

tektite.fon: tektite.fd
	../winfont/mkwinfont -o tektite.fon tektite.fd
tektitec.fon: tektitec.fd
	../winfont/mkwinfont -o tektitec.fon tektitec.fd

clean:
	rm -f *.fon
