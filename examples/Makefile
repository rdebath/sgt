# Makefile for polyhedra examples.

all: $(subst .points,.vertices,$(wildcard *.points)) \
     $(subst .points,.faces,$(wildcard *.points)) \
     8.vertices.clean 8.faces.clean

.SUFFIXES: .faces .vertices .points

.points.faces:
	../nfaces.py $*.points $*.faces

.points.vertices:
	../nvertices.py $*.points $*.vertices

# 8 is a special case: the only one of my examples which requires
# cleaning up.
8.vertices.clean: 8.vertices
	../cleanpoly.py 8.vertices 8.vertices.clean
8.faces.clean: 8.faces
	../cleanpoly.py 8.faces 8.faces.clean
