all: sixtypoints.png \
     thirteenpoints.png \
     facediagram.png \
     thirteenfaces.png \
     thirteenvertices.png

sixtypoints.png: ../examples/60.points ../drawpoints.py
	../drawpoints.py ../examples/60.points tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -crop 1600x1600+640+900 -geometry 300x300 \
		-antialias tmp.bmp $@
	rm -f tmp.ps tmp.bmp

thirteenpoints.png: ../examples/13.points ../drawpoints.py
	../drawpoints.py ../examples/13.points tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -crop 1600x1600+640+900 -geometry 300x300 \
		-antialias tmp.bmp $@
	rm -f tmp.ps tmp.bmp

facediagram.png: ../examples/13.points ../nfaces.py
	../nfaces.py -ps ../examples/13.points tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp%d.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -crop 2880x2880+0+260 -geometry 400x400 \
		-antialias tmp2.bmp $@
	rm -f tmp.ps tmp*.bmp

thirteenfaces.png: ../examples/13.faces ../drawpoly.py
	../drawpoly.py ../examples/13.faces tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -crop 1600x1600+640+900 -geometry 300x300 \
		-antialias tmp.bmp $@
	rm -f tmp.ps tmp*.bmp

thirteenvertices.png: ../examples/13.vertices ../drawpoly.py
	../drawpoly.py ../examples/13.vertices tmp.ps
	gs -sDEVICE=bmpmono -sOutputFile=tmp.bmp -g3000x4200 -r360 \
		-dBATCH -dNOPAUSE -q tmp.ps
	convert -crop 1040x1040+920+1180 -geometry 195x195 \
		-antialias tmp.bmp $@
	rm -f tmp.ps tmp*.bmp

clean:
	rm -f *.png
