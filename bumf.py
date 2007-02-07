# Usage, licence and such bumf for my build system.

import sys

import name

def shortusage():
    sys.stderr.write(
    "usage:   %s [-options] <mainmodule> [<var>=<value>...]\n" % name.name
    )

def longusage():
    shortusage()
    sys.stderr.write(
    "options: -o <builddir>       output build directory\n"+
    "         -l <logfile>        output build log filename\n"+
    "         -s <script>         name of build script within main module\n"+
    "         -f <conffile>       name of local configuration file to read\n"+
    "         -r <revision>       set SVN revision for all checkouts\n"+
    "         -r <module>=<revision> set SVN revision for one module\n"+
    "         -d <date>           set date tag for all checkouts\n"+
    "         -d <module>=<date>  set date tag for one module\n"+
    "         -B <module>=<branch> set branch name for one module\n"+
    "         -W <module>=<directory> acquire a module from a working copy\n"+
    "         -D <var>=<value>    set a build variable (same as extra arg)\n"+
    "         --workdir=<workdir> temporary work directory\n"+
    "         -k, --keep          do not delete work directory\n"+
    "         --complexrev        accept working copies with mixed SVN rev\n"+
    "         -v                  verbose (copy the build log to stdout)\n"+
    "also:    --help              display this text and do nothing else\n"+
    "         --licence           display licence text and do nothing else\n"+
    ""
    )

def licence():
    sys.stdout.write(
    "%s is copyright 2007 Simon Tatham.\n" % name.name+
    "\n"+
    "Permission is hereby granted, free of charge, to any person\n"+
    "obtaining a copy of this software and associated documentation files\n"+
    "(the \"Software\"), to deal in the Software without restriction,\n"+
    "including without limitation the rights to use, copy, modify, merge,\n"+
    "publish, distribute, sublicense, and/or sell copies of the Software,\n"+
    "and to permit persons to whom the Software is furnished to do so,\n"+
    "subject to the following conditions:\n"+
    "\n"+
    "The above copyright notice and this permission notice shall be\n"+
    "included in all copies or substantial portions of the Software.\n"+
    "\n"+
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n"+
    "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n"+
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND\n"+
    "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS\n"+
    "BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n"+
    "ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN\n"+
    "CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"+
    "SOFTWARE.\n"+
    "")
