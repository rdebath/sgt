all: dodec.ps dodec.pdf

dodec.pdf: dodec.ps
	ps2pdf dodec.ps

dodec.ps: pic.ps dodec
	$$HOME/src/polyhedra/drawnet.py \
		-sSTHPD -aKLHTF,ERGLK -aBSDJI,IJCQA \
		-aQREMA,IJCQA -aCOGRQ,ERGLK \
		-ppic.ps dodec | \
	sed > dodec.ps \
		-e 's/288 400 translate/288 430 translate/' \
		-e 's/^[0-9\.]* dup scale/70 dup scale/' \
		-e 's/^[0-9\.]* setlinewidth/0.002 setlinewidth/'

icon: dodec-cubes.png

dodec-cubes.png: dodec-cubes.pov dodec-cubes-frag.pov
	povray +fn +I$< +O$@ +W150 +H150 +V -D +X

dodec-cubes-frag.pov: pic.ps dodec
	$$HOME/src/polyhedra/povpoly.py -ppic.ps dodec $@

clean:
	rm -f dodec.ps *-frag.pov*

spotless: clean
	rm -f *.pdf *.png
