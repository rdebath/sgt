# Makefile for polyhedra examples.

all: $(subst .points,.vertices,$(wildcard *.points)) \
     $(subst .points,.faces,$(wildcard *.points)) \

.SUFFIXES: .faces .vertices .points

.points.faces:
	../nfaces.py $*.points $*.faces

.points.vertices:
	../nvertices.py $*.points $*.vertices

# 8 is a special case: the only one of my examples which requires
# cleaning up.
8.vertices.unclean: 8.points
	../nvertices.py 8.points 8.vertices.unclean
8.faces.unclean: 8.points
	../nfaces.py 8.points 8.faces.unclean
8.vertices: 8.vertices.unclean
	../cleanpoly.py 8.vertices.unclean 8.vertices
8.faces: 8.faces.unclean
	../cleanpoly.py 8.faces.unclean 8.faces
