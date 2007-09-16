# Makefile for solarmy pictures and animations.

IMAGES = startpoint randommoves parallel halfwhoosh1 parskew halfwhoosh2r \
         halfwhoosh2 whoosh badmega megawhoosh0 megawhoosh1 megawhoosh2 \
         megawhoosh3 megawhoosh4 megawhoosh5 megawhoosh6 megawhoosh7 \
         megawhoosh8 megawhoosh9 solution

GIFS = $(patsubst %,%.gif,$(IMAGES))

all: $(GIFS) index.html

$(GIFS) : %.gif : solarmy.py
	./solarmy.py $*

index.html: $(GIFS) index.template gen.sh
	sh gen.sh

clean:
	rm -f $(patsubst %,%*.png,$(IMAGES))

spotless: clean
	rm -f *.gif
