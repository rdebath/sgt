# Enigma master makefile

# Requires a compiler with -MD support, currently

# `make' from top level will build in directory `build'
# `make BUILDDIR=foo' from top level will build in directory foo
# `make auto' will test the Autoconf makefile in directory `auto'
# `make release VERSION=X.Y' will build enigma-X.Y.tar.gz.

MODULES := main screen engine memory levelfile misc savefile
OBJECTS := $(addsuffix .o,$(MODULES))

ifndef REALBUILD
ifndef BUILDDIR
ifdef TEST
BUILDDIR := test
else
BUILDDIR := build
endif
endif
all:
	@test -d $(BUILDDIR) || mkdir $(BUILDDIR)
	@make -C $(BUILDDIR) -f ../Makefile REALBUILD=yes
clean:
	@test -d $(BUILDDIR) || mkdir $(BUILDDIR)
	@make -C $(BUILDDIR) -f ../Makefile clean REALBUILD=yes
	rm -f configure Makefile.in install-sh
spotless: clean
	rm -f *.tar.gz
	rm -rf auto build
auto: autoconf
	@rm -rf auto && mkdir auto
	cd auto && ../configure && make
release: autoconf
	mkdir enigma-$(VERSION)
	for i in *.c *.h levels README LICENCE \
	Makefile.in configure.in configure install-sh; do \
		ln -s ../$$i enigma-$(VERSION); \
	done
	tar chzvf enigma-$(VERSION).tar.gz enigma-$(VERSION)
	rm -rf enigma-$(VERSION)

autoconf:
	autoconf
	cp /usr/share/automake/install-sh .
	sed 's/@OBJECTS@/$(OBJECTS)/' Makefile.in.in > Makefile.in
	for i in $(MODULES); do \
		gcc -MM $$i.c >> Makefile.in; \
	done
else

# The `real' makefile part.

CFLAGS += -Wall -W -DTESTING
LIBS += -lncurses

ifdef TEST
CFLAGS += -DLOGALLOC
LIBS += -lefence
endif

ifdef RELEASE
ifndef VERSION
VERSION := $(RELEASE)
endif
else
CFLAGS += -g
endif

ifndef VER
ifdef VERSION
VER := $(VERSION)
endif
endif
ifdef VER
VDEF := -DVERSION=\"$(VER)\"
endif

SRC := ../

MODULES := main screen engine memory levelfile misc savefile

OBJECTS := $(addsuffix .o,$(MODULES))
DEPS := $(addsuffix .d,$(MODULES))

enigma: $(OBJECTS)
	$(CC) $(LFLAGS) -o enigma $(OBJECTS) $(LIBS)

%.o: $(SRC)%.c
	$(CC) $(CFLAGS) -MD -c $<

version.o: FORCE
	$(CC) $(VDEF) -MD -c $(SRC)version.c

clean::
	rm -f *.o enigma core

FORCE: # phony target to force version.o to be rebuilt every time

-include $(DEPS)

endif
