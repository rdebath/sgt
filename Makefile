#
# Use `nmake' to build.
#

CFLAGS = /nologo /W3 /YX /O2 /Yd /D_WINDOWS /DDEBUG /ML /Fd
LFLAGS = /incremental:no

.c.obj:
	cl $(COMPAT) $(CFLAGS) /c $*.c

LIBS = gdi32.lib user32.lib shell32.lib

all: winurl.exe

winurl.exe: winurl.obj winurl.res
	link $(LFLAGS) -out:winurl.exe winurl.obj winurl.res $(LIBS)

winurl.res: winurl.rc winurl.ico winurlsmall.ico
	rc -r winurl.rc

clean:
	del *.obj
	del *.exe
	del *.res
	del *.pch
	del *.aps
	del *.ilk
	del *.pdb
	del *.rsp
