all: tektite.fon tektitec.fon \
     tektite.pcf.Z tektite.pcf.gz \
     tektiteu.pcf.Z tektiteu.pcf.gz \
     tektite8.pcf.Z tektite8.pcf.gz \
     tektiteu8.pcf.Z tektiteu8.pcf.gz

tektite.pcf.gz: tektite.bdf
	bdftopcf < tektite.bdf | gzip -9 > tektite.pcf.gz
tektite.pcf.Z: tektite.bdf
	bdftopcf < tektite.bdf | compress > tektite.pcf.Z

tektiteu.pcf.gz: tektiteu.bdf
	bdftopcf < tektiteu.bdf | gzip -9 > tektiteu.pcf.gz
tektiteu.pcf.Z: tektiteu.bdf
	bdftopcf < tektiteu.bdf | compress > tektiteu.pcf.Z

tektite8.pcf.gz: tektite8.bdf
	bdftopcf < tektite8.bdf | gzip -9 > tektite8.pcf.gz
tektite8.pcf.Z: tektite8.bdf
	bdftopcf < tektite8.bdf | compress > tektite8.pcf.Z

tektiteu8.pcf.gz: tektiteu8.bdf
	bdftopcf < tektiteu8.bdf | gzip -9 > tektiteu8.pcf.gz
tektiteu8.pcf.Z: tektiteu8.bdf
	bdftopcf < tektiteu8.bdf | compress > tektiteu8.pcf.Z

tektite.fon: tektite.fd
	../winfont/mkwinfont -o tektite.fon tektite.fd
tektitec.fon: tektitec.fd
	../winfont/mkwinfont -o tektitec.fon tektitec.fd

clean:
	rm -f *.fon
