all: icos.ps dodec.ps dodec.pdf

dodec.pdf: dodec.ps
	ps2pdf dodec.ps

dodec.ps: pic.ps desc
	$$HOME/src/polyhedra/drawnet.py \
		-sSTHPD -aKLHTF,ERGLK -aBSDJI,IJCQA \
		-aQREMA,IJCQA -aCOGRQ,ERGLK \
		-ppic.ps dodec | \
	sed > dodec.ps \
		-e 's/288 400 translate/288 430 translate/' \
		-e 's/[0-9\.]* dup scale/70 dup scale/' \
		-e 's/[0-9\.]* setlinewidth/0.002 setlinewidth/'

icos.ps: pic.ps desc
	$$HOME/src/polyhedra/drawnet.py \
		-sEGB -aIFC,IKF -aFHC,IFC -aDHF,FHC \
		-ppic.ps icos | sed 's/.* scale/70 dup scale/' | \
	sed > icos.ps \
		-e 's/288 400 translate/288 430 translate/' \
		-e 's/[0-9\.]* dup scale/70 dup scale/' \
		-e 's/[0-9\.]* setlinewidth/0.002 setlinewidth/'

desc: b.dia c.dia d.dia e.dia makedesc.py
	./makedesc.py > desc

pic.ps: map.ps desc
	perl -ne 'if (/\((.*)\) \(r\) file cvx exec/)' \
	      -e '{ system "cat $$1" } else { print }' map.ps > pic.ps

icon: amblesmall.jpeg amblelarge.jpeg

amblesmall.jpeg: amble.png
	convert -scale 150x150 $< $@
amblelarge.jpeg: amble.png
	convert -scale 450x450 $< $@

amble.png: amble.pov amble-frag.pov
	povray +fn +I$< +O$@ +W900 +H900 +V -D +X

amble-frag.pov: pic.ps desc dodec
	$$HOME/src/polyhedra/povpoly.py -ppic.ps dodec $@

clean:
	rm -f desc pic.ps dodec.ps icos.ps *-frag.pov* Memory.Log amble.png

spotless: clean
	rm -f *.pdf *.png *.jpeg
