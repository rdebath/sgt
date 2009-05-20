#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <X11/Xlib.h>

/*
 * Small shared library for Linux, to be loaded with LD_PRELOAD.
 * Overrides the Xlib function XInternAtom() to make it return the
 * special value None if passed the string "NONE". This has the
 * effect of working around the GDK bug described at
 * 
 *   http://bugzilla.gnome.org/show_bug.cgi?id=580511
 *
 * compilation (linux):
 *   gcc -fpic -c gtkfix.c && ld -shared -o gtkfix.so gtkfix.o -ldl
 * use:
 *   export LD_PRELOAD=/full/path/name/to/gtkfix.so
 */

typedef Atom (*XInternAtom_t)(Display *display, const char *atom_name,
                              Bool only_if_exists);
Atom XInternAtom(Display *display, const char *atom_name,
                 Bool only_if_exists)
{
    XInternAtom_t real = (XInternAtom_t)dlsym(RTLD_NEXT, "XInternAtom");
    if (atom_name && !strcmp(atom_name, "NONE"))
        return None;
    return real(display, atom_name, only_if_exists);
}
