# SVN checkout handling for my build system.

import string

import log
import lexer
import script
import misc
import os

headrevs = {}

def checkout(cfg, module, path, is_main):
    log.logmsg("Checking out module %s into path %s" % (module, path))

    # First, check the command-line configuration to find out how
    # we need to check out this module.
    details = cfg.specialrev.get(module, [cfg.baserev, module, None])

    set_headrev = 0

    if details[2] != None:
	# If we've been given an actual working directory, we just
	# do an export of that.
	svnparams = [details[2]]

	# Also, determine the revision number of the working
	# directory using svnversion.
	svnvcmd = misc.shellquote(["svnversion", details[2]])
	f = os.popen(svnvcmd + " 2>&1", "r")
	newrev = f.read()
	f.close()
	while newrev[-1:] == "\r" or newrev[-1:] == "\n":
	    newrev = newrev[:-1]

	# If there's more than one revision represented here, raise
	# an error unless we've been told to accept that.
	if not cfg.accept_complex_rev:
	    ok = 1
	    for c in newrev:
		if not (c in "0123456789M"):
		    ok = 0
		    break
	    if not ok:
		raise misc.builderr("working directory `%s' has complex revision `%s'; use `--complexrev' to proceed regardless" % (details[2], newrev))
    else:
	# Otherwise, we must read the config file to determine the
	# right svn repository location.
	save = lexer.save_vars()
	lexer.set_multicharvar("module", module)
	script.process_script(cfg.cfgfile, 1, cfg)
	svnrepos = lexer.get_multicharvar("svnrepos")
	lexer.restore_vars(save)
	if svnrepos == None:
	    raise misc.builderr("Configuration file did not specify `svnrepos' for module `%s'" % module)
	log.logmsg("  Using SVN repository %s" % svnrepos)

	newrev = None

	if details[0][0] == "rev":
	    rev = details[0][1]
	    # If this is a simple revision number, use it as newrev
	    # unless told otherwise later.
	    ok = 1
	    for c in rev:
		if not (c in "0123456789"):
		    ok = 0
		    break
	    if ok:
		newrev = rev
	    # If this is the special revision number HEAD, we try
	    # to find out the real revision number after doing the
	    # export, and we save that in the `headrevs' hash in
	    # this module. Further head checkouts from this
	    # repository are done with that explicit revision
	    # number, so that if we're checking out multiple
	    # modules we get an effectively atomic checkout over
	    # all of them.
	    if rev == "HEAD":
		if headrevs.has_key(svnrepos):
		    rev = headrevs[svnrepos]
		else:
		    set_headrev = 1
	    svnparams = ["-r"+rev]
	else:
	    svnparams = ["-r{"+details[0][1]+"}"]
	svnparams.append(svnrepos + "/" + details[1])

    # Now we can construct the exact svn export command we want to run.
    svncmdlist = ["svn", "export"] + svnparams + [path]
    svncmdstr = misc.shellquote(svncmdlist)
    log.logmsg("  Running export command: " + svncmdstr)

    # Now actually run the command. We retrieve the output, write
    # it into the log file, and scan it to see if it mentions a
    # revision number.
    f = os.popen(svncmdstr + " 2>&1", "r")
    while 1:
	line = f.readline()
	if line == "": break
	while line[-1:] == "\r" or line[-1:] == "\n":
	    line = line[:-1]
	log.logoutput(line)
	if line[:18] == "Exported revision " and line[-1:] == ".":
	    newrev = line[18:-1]
    ret = f.close()
    if ret > 0:
	raise misc.builderr("svn export command terminated with status %d" % ret)

    if newrev == "":
	raise misc.builderr("Unable to determine revision number for checked-out module `%s'" % module)
    log.logmsg("  Revision number for this module is %s" % newrev)

    if set_headrev:
	headrevs[svnrepos] = newrev
	log.logmsg("  Using r%s for further head checkouts from %s" % (newrev, svnrepos))

    if is_main:
	lexer.set_multicharvar("revision", newrev)
    lexer.set_multicharvar("revision_" + module, newrev)
