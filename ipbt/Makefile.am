bin_PROGRAMS = ipbt
ipbt_SOURCES = be_none.c fromucs.c ipbt.c ldiscucs.c localenc.c macenc.c \
       mimeenc.c minibidi.c misc.c noprint.c notiming.c sbcs.c sbcsdat.c \
       settings.c slookup.c terminal.c time.c toucs.c tree234.c utf8.c \
       uxmisc.c uxucs.c wcwidth.c xenc.c
ipbt_LDADD = $(LIBOBJS)
man1_MANS = ipbt.1

BUILT_SOURCES = sbcsdat.c
sbcsdat.c: sbcsgen.pl sbcs.dat
	perl $(srcdir)/sbcsgen.pl $(srcdir)/sbcs.dat

# If Halibut is available to rebuild the man pages from their .but
# source, then man pages are treated as derived files in the obvious
# way, and deleted by 'make clean'; we also build (and clean) the HTML
# equivalents. If Halibut is not available (the typical case if
# someone has downloaded the source archive and rerun mkauto.sh), the
# man pages are treated as source files by this makefile.
if HAVE_HALIBUT
BUILT_MANS = $(man1_MANS)
HTMLFILES = ipbt.html
CLEANFILES = $(BUILT_MANS) $(HTMLFILES) $(BUILT_SOURCES)
.SUFFIXES = .but .1 .html
.but.1:
	halibut --man=$@ $<
.but.html:
	halibut --html=$@ $<
doc: $(BUILT_MANS) $(HTMLFILES)
all-local: $(HTMLFILES)
else
CLEANFILES = $(BUILT_SOURCES)
endif
