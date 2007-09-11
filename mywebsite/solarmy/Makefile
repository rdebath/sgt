# Makefile for solarmy pictures and animations.

IMAGES = startpoint solution randommoves parallel parskew \
         halfwhoosh1 halfwhoosh2r halfwhoosh2 whoosh three \
	 solution0 solution1 solution2 solution3 solution4 \
	 solution5 solution6 solution7 solution8 solution9 solution10
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
