MKREGULAR = \
    cube \
    cuboctahedron \
    dodecahedron \
    greatrhombicosidodecahedron \
    greatrhombicuboctahedron \
    icosahedron \
    icosidodecahedron \
    octahedron \
    rhombicdodecahedron \
    rhombictriacontahedron \
    smallrhombicosidodecahedron \
    smallrhombicuboctahedron \
    tetrahedron \
    truncatedcube \
    truncateddodecahedron \
    truncatedicosahedron \
    truncatedoctahedron \
    truncatedtetrahedron

# Archimedean duals not generated automatically by mkregular.py.

DUALS = \
    hexakisicosahedron \
    hexakisoctahedron \
    deltoidalhexecontahedron \
    deltoidalicositetrahedron \
    triakisoctahedron \
    triakisicosahedron \
    pentakisdodecahedron \
    tetrakishexahedron \
    triakistetrahedron

all: $(MKREGULAR) $(DUALS)

$(MKREGULAR): mkregular.py
	./mkregular.py all

hexakisicosahedron: greatrhombicosidodecahedron ../dual.py
	../dual.py $< $@

hexakisoctahedron: greatrhombicuboctahedron ../dual.py
	../dual.py $< $@

deltoidalhexecontahedron: smallrhombicosidodecahedron ../dual.py
	../dual.py $< $@

deltoidalicositetrahedron: smallrhombicuboctahedron ../dual.py
	../dual.py $< $@

triakisoctahedron: truncatedcube ../dual.py
	../dual.py $< $@

triakisicosahedron: truncateddodecahedron ../dual.py
	../dual.py $< $@

pentakisdodecahedron: truncatedicosahedron ../dual.py
	../dual.py $< $@

tetrakishexahedron: truncatedoctahedron ../dual.py
	../dual.py $< $@

triakistetrahedron: truncatedtetrahedron ../dual.py
	../dual.py $< $@

clean:
	rm -f *hedron *cube *.ps
