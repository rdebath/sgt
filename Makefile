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

SRC := ../

CHARSET := big5enc big5set cp949 euc fromucs gb2312 hz iso2022s
CHARSET += jisx0208 jisx0212 ksx1001 localenc macenc mimeenc sbcs
CHARSET += sbcsdat shiftjis slookup toucs utf16 utf7 utf8 xenc
CHARSET += superset

MODULES := main malloc error help licence version sqlite mboxstore store
MODULES += config mboxread rfc822 rfc2047 base64 qp date misc export

CSMODULES := $(addprefix cs-,$(CHARSET))
OBJECTS := $(addsuffix .o,$(MODULES) $(CSMODULES))
DEPS := $(addsuffix .d,$(MODULES))
LIBS := -lsqlite
CFLAGS += $(CFL) -Wall -I$(SRC)charset

timber: $(OBJECTS)
	$(CC) $(LFLAGS) -o timber $(OBJECTS) $(LIBS)

cs-%.o: $(SRC)charset/%.c
	$(CC) $(CFLAGS) -MD -o $@ -c $<

%.o: $(SRC)%.c
	$(CC) $(CFLAGS) -MD -c $<

version.o: FORCE
	$(CC) $(VDEF) -MD -c $(SRC)version.c

FORCE: # phony target to force version.o to be rebuilt every time

clean:
	rm -f *.o timber

-include $(DEPS)

endif
