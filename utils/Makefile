SUBDIRS = base64 cvt-utf8 multi xcopy

all:
	for i in $(SUBDIRS); do make -C $$i; done

clean:
	for i in $(SUBDIRS); do make -C $$i clean; done
