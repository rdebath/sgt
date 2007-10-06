all: icos.ps dodec.ps

dodec.ps: pic.ps desc
	$$HOME/src/polyhedra/drawnet.py \
		-sSTHPD -aKLHTF,ERGLK -aBSDJI,IJCQA \
		-aQREMA,IJCQA -aCOGRQ,ERGLK \
		-ppic.ps dodec | sed 's/.* scale/70 dup scale/' > dodec.ps

icos.ps: pic.ps desc
	$$HOME/src/polyhedra/drawnet.py \
		-sEGB -aIFC,IKF -aFHC,IFC -aDHF,FHC \
		-ppic.ps icos | sed 's/.* scale/70 dup scale/' > icos.ps

desc: b.dia c.dia d.dia e.dia makedesc.py
	./makedesc.py > desc

pic.ps: map.ps desc
	perl -ne 'if (/\((.*)\) \(r\) file cvx exec/)' \
	      -e '{ system "cat $$1" } else { print }' map.ps > pic.ps

clean:
	rm -f desc pic.ps dodec.ps icos.ps
