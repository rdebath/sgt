# Caltrap master makefile

# Requires a compiler with -MD support, currently

# `make' from top level will build in caltrap.b
# `make BUILDDIR=foo' from top level will build in directory foo
ifndef REALBUILD
ifndef BUILDDIR
BUILDDIR := build
endif
all:
	@test -d $(BUILDDIR) || mkdir $(BUILDDIR)
	@make -C $(BUILDDIR) -f ../Makefile REALBUILD=yes
clean:
	@test -d $(BUILDDIR) && make -C $(BUILDDIR) -f ../Makefile REALBUILD=yes clean
else

# The `real' makefile part.

ifdef RELEASE
ifndef VERSION
VERSION := $(RELEASE)
endif
else
CFLAGS += -g
endif

CFLAGS += -Wall

ifndef VER
ifdef VERSION
VER := $(VERSION)
endif
endif
ifdef VER
VDEF := -DVERSION=\"$(VER)\"
endif

SRC := ../

MODULES := main malloc error help licence version add list datetime sqlite
MODULES += cron dump tree234 misc

OBJECTS := $(addsuffix .o,$(MODULES))
DEPS := $(addsuffix .d,$(MODULES))

LIBS := -lsqlite

caltrap: $(OBJECTS)
	$(CC) $(LFLAGS) -o caltrap $(OBJECTS) $(LIBS)

%.o: $(SRC)%.c
	$(CC) $(CFLAGS) -MD -c $<

version.o: FORCE
	$(CC) $(VDEF) -MD -c $(SRC)version.c

FORCE: # phony target to force version.o to be rebuilt every time

clean:
	rm -f *.o caltrap

-include $(DEPS)

endif
