DEPS = points60.png \
       points13.png \
       facediagram.png \
       faces13.png faces13.desc \
       vertices13.png vertices13.desc \
       vertices4.png vertices4.desc \
       faces4.png faces4.desc \
       faces6.png faces6.desc \
       vertices6.png vertices6.desc \
       faces8.png faces8.desc \
       vertices8.png vertices8.desc \
       faces12.png faces12.desc \
       vertices12.png vertices12.desc \
       vertices20.png vertices20.desc \
       vertices60.png vertices60.desc \
       faces7.png faces7.desc \
       faces8.png faces8.desc \
       faces9.png faces9.desc \
       faces10.png faces10.desc \
       faces11.png faces11.desc \
       faces32.png faces32.desc

index.html: makepage.pl index.tmpl $(DEPS)
	./makepage.pl

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

faces%.desc: ../examples/%.faces ../canvas.py
	../canvas.py $< > $@
vertices%.desc: ../examples/%.vertices ../canvas.py
	../canvas.py $< > $@

clean:
	rm -f *.png
