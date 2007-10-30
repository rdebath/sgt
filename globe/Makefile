CFLAGS = -g

IMAGES = icos-bw-vert icos-colour-vert icos-bw-face icos-colour-face
PSPRE = $(patsubst %,%-pre.ps,$(IMAGES))
PDF = $(patsubst %,%.pdf,$(IMAGES))

all: $(PDF)

$(PSPRE): %.ps:
	$$HOME/src/polyhedra/drawnet.py \
		-sEGB -aIFC,IKF -aFHC,IFC -aDHF,FHC \
		-p$^ | \
	sed > $@ \
		-e 's/288 400 translate/288 430 translate/' \
		-e 's/[0-9\.]* dup scale/70 dup scale/' \
		-e 's/[0-9\.]* setlinewidth/0.002 setlinewidth/'

icos-bw-vert-pre.ps: mapbw.ps icos-vertices
icos-colour-vert-pre.ps: mapcolour.ps icos-vertices
icos-bw-face-pre.ps: mapbw.ps icos-faces
icos-colour-face-pre.ps: mapcolour.ps icos-faces

$(PDF): %.pdf:
	ps2pdf $< $@

icos-bw-vert.pdf: icos-bw-vert-pre.ps desc.bw
icos-colour-vert.pdf: icos-colour-vert-pre.ps desc.colour
icos-bw-face.pdf: icos-bw-face-pre.ps desc.bw
icos-colour-face.pdf: icos-colour-face-pre.ps desc.colour

desc.bw: makebmp makemap.sh
	./makemap.sh

desc.colour: makebmp makecmap.sh
	./makecmap.sh

makebmp: makebmp.c
	gcc -o makebmp makebmp.c

CUBEFACES = 1 2 3 4 5 6
CUBEFACEPNGS = $(patsubst %,cube%.png,$(CUBEFACES))
CUBEFACEBMPS = $(patsubst %,cube%.bmp,$(CUBEFACES))
ANGLES = 000 015 030 045 060 075 090 105 120 135 150 165 \
         180 195 210 225 240 255 270 285 300 315 330 345
CUBEPICS = $(patsubst %,cubevert%.png,$(ANGLES))
cube: $(CUBEPICS)
$(CUBEPICS): cubevert%.png: $(CUBEFACEPNGS) cube.pov
	povray +fn +Icube.pov +O$@ +W500 +H500 +V -D +X +K$(patsubst cubevert%.png,%,$@)
$(CUBEFACEPNGS): %.png: %.bmp
	convert $< $@
$(CUBEFACEBMPS): makecube
	./makecube -v 500

makecube: makecube.c
	gcc -o makecube makecube.c -lm

icon: bwglobesmall.jpeg bwglobelarge.jpeg \
      cglobesmall.jpeg cglobelarge.jpeg

bwglobesmall.jpeg: bwglobe.png
	convert -scale 150x150 $< $@
bwglobelarge.jpeg: bwglobe.png
	convert -scale 450x450 $< $@
cglobesmall.jpeg: cglobe.png
	convert -scale 150x150 $< $@
cglobelarge.jpeg: cglobe.png
	convert -scale 450x450 $< $@

cglobe.png: icosc.pov icos-colour-vert-frag.pov
	povray +fn +I$< +O$@ +W900 +H900 +V -D +X

icos-colour-vert-frag.pov: icos-vertices mapcolour.ps desc.colour
	$$HOME/src/polyhedra/povpoly.py -pmapcolour.ps icos-vertices $@

bwglobe.png: icosb.pov icos-bw-vert-frag.pov
	povray +fn +I$< +O$@ +W900 +H900 +V -D +X

icos-bw-vert-frag.pov: icos-vertices mapbw.ps desc.bw
	$$HOME/src/polyhedra/povpoly.py -pmapbw.ps icos-vertices $@

clean:
	rm -f makebmp makecube *.eps *.bmp *.shape desc.* *-pre.ps Memory.Log
	rm -f cube?.bmp cube?.png bwglobe.png cglobe.png
	rm -f *-frag.pov* Memory.Log

spotless: clean
	rm -f *.pdf cubevert*.png *globe.png *.jpeg
