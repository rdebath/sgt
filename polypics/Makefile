SUBDIRS = amble badger dodec-cubes eek globe

all:
	for i in $(SUBDIRS); do make -C $$i; done

icon:
	for i in $(SUBDIRS); do make -C $$i icon; done

clean:
	for i in $(SUBDIRS); do make -C $$i clean; done

spotless:
	for i in $(SUBDIRS); do make -C $$i spotless; done
