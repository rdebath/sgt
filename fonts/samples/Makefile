all: tektite.png fixed.png

clean:
	rm -f *.png

tektite.png: sample.py ../tektite/tektite.fd
	./sample.py ../tektite/tektite.fd | convert xpm:- tektite.png

fixed.png: sample.py ../fixed/fixed.fd
	./sample.py ../fixed/fixed.fd | convert xpm:- fixed.png
