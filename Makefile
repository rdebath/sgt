dist:
	find . -name CVS -prune -o -name polyhedra -prune -o -type d \
		-exec mkdir -p ./polyhedra/{} \;
	find . -name CVS -prune -o -path ./polyhedra -prune -o \
		-name .cvsignore -prune -o -name '*.pyc' -prune -o \
		-name '*.ps' -prune -o -name '*~' -prune -o -type f \
		-exec ln -s $$PWD/{} ./polyhedra/{} \;
	tar chzvf polyhedra.tar.gz polyhedra
	rm -rf polyhedra
