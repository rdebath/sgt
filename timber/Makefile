# Timber master makefile

# Requires a compiler with -MD support, currently

# `make' from top level will build in timber.b
# `make BUILDDIR=foo' from top level will build in directory foo
ifndef REALBUILD
ifndef BUILDDIR
BUILDDIR := build
endif
all:
	@test -d $(BUILDDIR) || mkdir $(BUILDDIR)
	@make -C $(BUILDDIR) -f ../Makefile REALBUILD=yes
test:
	@test -d $(BUILDDIR) || mkdir $(BUILDDIR)
	@make -C $(BUILDDIR) -f ../Makefile REALBUILD=yes test
clean:
	@test -d $(BUILDDIR) && make -C $(BUILDDIR) -f ../Makefile REALBUILD=yes clean
spotless:
	rm -rf $(BUILDDIR)
else

# The `real' makefile part.

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

timber:

SRC := ../

ifeq ($(shell test -d $(SRC)charset && echo yes),yes)
LIBCHARSET_SRCDIR = $(SRC)charset/
else
LIBCHARSET_SRCDIR = $(SRC)../charset/
endif
LIBCHARSET_OBJDIR =#
LIBCHARSET_OBJPFX = cs-#
LIBCHARSET_GENPFX = charset-#
include $(LIBCHARSET_SRCDIR)Makefile

MODULES := main malloc error help licence version sqlite mboxstore store
MODULES += config mboxread rfc822 rfc2047 base64 qp date misc export
MODULES += display boringhdr send

OBJECTS := $(addsuffix .o,$(MODULES)) $(LIBCHARSET_OBJS)
DEPS := $(addsuffix .d,$(MODULES))
LIBS := -lsqlite
CFLAGS += $(CFL) -Wall -I$(LIBCHARSET_SRCDIR) -I.

TESTS := main date
TEST_MODULES := $(addsuffix -tests,$(TESTS))
TEST_OBJECTS := $(addsuffix .o,$(TEST_MODULES))
DEPS += $(addsuffix .d,$(TEST_MODULES))
TEST_LIBS := -lcheck

timber: $(OBJECTS)
	$(CC) $(LFLAGS) -o timber $(OBJECTS) $(LIBS)

%.o: $(SRC)%.c
	$(CC) $(CFLAGS) -MD -o $@ -c $<

version.o: FORCE
	$(CC) $(VDEF) -MD -c $(SRC)version.c

FORCE: # phony target to force version.o to be rebuilt every time

test: unit-test external-test

external-test: timber
	perl ../tests/external-tests.pl ./timber

unit-test: unit-tests
	./unit-tests

unit-tests: $(TEST_OBJECTS)
	$(CC) $(LFLAGS) -o unit-tests $(TEST_OBJECTS) $(TEST_LIBS)

%-tests.o: $(SRC)tests/%-tests.c
	$(CC) $(CFLAGS) -I.. -I- -MD -o $@ -c $<

clean:
	rm -f *.o timber

-include $(DEPS)

endif

