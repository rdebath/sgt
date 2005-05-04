all: points60.png \
     points13.png \
     facediagram.png \
     faces13.png \
     vertices13.png \
     vertices4.png \
     faces4.png \
     faces6.png \
     vertices6.png \
     faces8.png \
     vertices8.png \
     faces12.png \
     vertices12.png \
     vertices20.png \
     vertices60.png \
     faces7.png \
     faces8.png \
     faces9.png \
     faces10.png \
     faces11.png \
     faces32.png

facediagram.png: ../examples/13.points ../nfaces.py
	../nfaces.py -ps ../examples/13.points tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp%d.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -crop 2880x2880+0+260 -geometry 400x400 \
		-antialias tmp2.bmp $@
	rm -f tmp.ps tmp*.bmp

points%.png: ../examples/%.points ../drawpoints.py
	../drawpoints.py $< tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -geometry 600x840 -antialias tmp.bmp tmp.ppm
	pnmcrop tmp.ppm > tmp2.ppm
	convert tmp2.ppm $@
	rm -f tmp.ps tmp.bmp tmp*.ppm

faces%.png: ../examples/%.faces ../drawpoly.py
	../drawpoly.py $< tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -geometry 600x840 -antialias tmp.bmp tmp.ppm
	pnmcrop tmp.ppm > tmp2.ppm
	convert tmp2.ppm $@
	rm -f tmp.ps tmp.bmp tmp*.ppm

vertices%.png: ../examples/%.vertices ../drawpoly.py
	../drawpoly.py $< tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -geometry 600x840 -antialias tmp.bmp tmp.ppm
	pnmcrop tmp.ppm > tmp2.ppm
	convert tmp2.ppm $@
	rm -f tmp.ps tmp.bmp tmp*.ppm

clean:
	rm -f *.png
