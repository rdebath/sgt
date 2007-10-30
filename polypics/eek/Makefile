all: icos.ps dodec.ps dodec.pdf wing.pdf

dodec.pdf: dodec.ps
	ps2pdf dodec.ps

wing.pdf: wing.ps
	ps2pdf wing.ps

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
		-ppic.ps icos icos.ps

desc: makedesc.py
	python2.1 ./makedesc.py > desc

-include desc.d

pic.ps: map.ps desc
	perl -ne 'if (/\((.*)\) \(r\) file cvx exec/)' \
	      -e '{ system "cat $$1" } else { print }' map.ps > pic.ps

icon: eeksmall.jpeg eeklarge.jpeg eekwingdiag.png

eeksmall.jpeg: eek.png
	convert -scale 200x150 $< $@
eeklarge.jpeg: eek.png
	convert -scale 600x450 $< $@

eek.png: eek.pov eek-frag.pov wingicon.png
	povray +fn +I$< +O$@ +W1200 +H900 +V -D +X

eek-frag.pov: pic.ps desc dodec
	$$HOME/src/polyhedra/povpoly.py -ppic.ps dodec $@

eekwingdiag.png: wingdiag.ps wing.ps
	gs -sDEVICE=pnggray -sOutputFile=tmp.$@ -g6000x2000 -r1440 -dNOPAUSE -dBATCH -q $<
	convert -scale 600x200 tmp.$@ $@

wingicon.png: wingicon.ps
	gs -dNOPAUSE -dBATCH -sDEVICE=pngmono -sOutputFile=$@ \
		-r50 -g500x650 -q $<

clean:
	rm -f desc pic.ps dodec.ps icos.ps desc.d *-frag.pov* Memory.Log
	rm -f eek.png

spotless: clean
	rm -f *.pdf *.png *.jpeg
