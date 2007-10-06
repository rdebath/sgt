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

clean:
	rm -f dodec.ps

