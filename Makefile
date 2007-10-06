SUBDIRS = amble badger dodec-cubes eek globe

all:
	for i in $(SUBDIRS); do make -C $$i; done

clean:
	for i in $(SUBDIRS); do make -C $$i clean; done
