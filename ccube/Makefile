all: cube.ps cube.pdf

cube.pdf: cube.ps
	ps2pdf cube.ps

cube.ps: pic.ps cube
	$$HOME/src/polyhedra/drawnet.py \
		-ppic.ps cube | \
	sed > cube.ps \
		-e 's/288 400 translate/288 430 translate/' \
		-e 's/^[0-9\.]* dup scale/70 dup scale/' \
		-e 's/^[0-9\.]* setlinewidth/0.002 setlinewidth/'

icon: ccubesmall.jpeg ccubelarge.jpeg

ccubesmall.jpeg: ccube.png
	convert -scale 150x150 $< $@
ccubelarge.jpeg: ccube.png
	convert -scale 450x450 $< $@

ccube.png: ccube.pov ccubefrag.pov
	povray +fn +I$< +O$@ +W900 +H900 +V -D +X

ccubefrag.pov: pic.ps cube
	$$HOME/src/polyhedra/povpoly.py -ppic.ps cube $@

clean:
	rm -f cube.ps *-frag.pov* Memory.Log ccube.png

spotless: clean
	rm -f *.pdf *.png *.jpeg
