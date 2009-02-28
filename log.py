# Logging module for my build scripts.

import sys

logfp = None
logcfg = None

def init(fp, cfg):
    global logfp
    global logcfg
    logfp = fp
    logcfg = cfg

def internal_log(msg):
    logfp.write(msg + "\n")
    logfp.flush()
    if logcfg.verbose:
        sys.stdout.write(msg + "\n")
        sys.stdout.flush()

def logmsg(s):
    internal_log("* " + s)

def logscript(s):
    internal_log("> " + s)

def logoutput(s):
    internal_log("| " + s)
