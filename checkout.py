# SVN checkout handling for my build system.

import string

import log
import lexer
import script
import misc
import os
import shutil

headrevs = {}

# For every (repository, revision) pair we check out on, track the
# most recent revision in which there was actually a real change.
# This is used to populate the $(revision) fields.
lastchange = {}

def get_lcrev(index):
    return "%d" % lastchange[index]

def itostr(index):
    # Convert a tuple used as an index into lastchange[] into a
    # descriptive string for logging.
    if index[0] == None:
        return "working dir " + index[1]
    else:
        return "revision %s of %s" % index

def checkout(cfg, module, path, is_main):
    log.logmsg("Checking out module %s into path %s" % (module, path))

    # First, check the command-line configuration to find out how
    # we need to check out this module.
    details = cfg.specialrev.get(module, [cfg.baserev, module, None])

    set_headrev = 0

    git = 0

    if details[2] != None:
        # If we've been given an actual working directory, we just
        # do an export of that.
        svnparams = [details[2]]

        log.logmsg("  Using existing working directory %s" % details[2])

        # Also, determine the revision number of the working
        # directory using svnversion.
        if os.access(details[2]+"/.svn", os.F_OK):
            svnvcmd = misc.shellquote(["svnversion", details[2]])
            f = os.popen(svnvcmd + " 2>&1", "r")
            newrev = f.read()
            f.close()
            while newrev[-1:] == "\r" or newrev[-1:] == "\n":
                newrev = newrev[:-1]
            log.logmsg("  Revision returned from svnversion: %s" % newrev)
        elif os.access(details[2]+"/.git", os.F_OK):
            git = 1
            cdcmd = misc.shellquote(["cd", details[2]])

            gitstatcmd = misc.shellquote(["git", "status"])
            f = os.popen(cdcmd + "&&" + gitstatcmd + " 2>&1", "r")
            mod = "M" # assume modified unless git status reports clean
            while 1:
                s = f.readline()
                if s == "":
                    break
                if s[-1:] == "\n": s = s[:-1]
                if s[:8] == "nothing ":
                    mod = ""
            f.close()

            gitlogcmd = misc.shellquote(["git", "log"])
            f = os.popen(cdcmd + "&&" + gitlogcmd + " 2>&1", "r")
            first = 1
            while 1:
                s = f.readline()
                if s == "":
                    raise misc.builderr("working directory `%s': unable to find git-svn latest revision in `git log' output")
                if s[-1:] == "\n": s = s[:-1]
                if s[:16] == "    git-svn-id: ":
                    ss = string.split(s)
                    if len(ss) > 1:
                        try:
                            i = string.rindex(ss[1], "@")
                            newrev = ss[1][i+1:] + mod
                            break
                        except ValueError, e:
                            pass
                if s[:6] == "commit":
                    if first:
                        first = 0
                    else:
                        mod = "M"
            f.close()
            log.logmsg("  Revision faked via git-svn: %s" % newrev)
        else:
            raise misc.builderr("working directory `%s' is not a Subversion working copy")

        # If there's more than one revision represented here, raise
        # an error unless we've been told to accept that.
        if not cfg.accept_complex_rev and not misc.checkstr(newrev, "0123456789M"):
            raise misc.builderr("working directory `%s' has complex revision `%s'; use `--complexrev' to proceed regardless" % (details[2], newrev))

        lcrevindex = (None, details[2])
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
            if misc.numeric(rev):
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

            # If this revision is actually a date tag, set the
            # special date variable.
            if rev[:1] == "{" and rev[-1:] == "}":
                date = rev[1:-1]
                if is_main:
                    lexer.set_multicharvar("date", date)
                lexer.set_multicharvar("date_" + module, date)

            svnparams = ["-r"+rev]
        else:
            svnparams = ["-r{"+details[0][1]+"}"]
            # Explicitly provided date tag. Set the date variables.
            if is_main:
                lexer.set_multicharvar("date", details[0][1])
            lexer.set_multicharvar("date_" + module, details[0][1])

        svnparams.append(svnrepos + "/" + details[1])

        lcrevindex = (-1, svnrepos)

    if not git:
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
        if lcrevindex[0] == -1:
            lcrevindex = (newrev,) + lcrevindex[1:]

        # We'll also run `svn info' to get the last-changed revision.
        svnicmd = misc.shellquote(["svn", "info"] + svnparams)
        log.logmsg("  Running info command: " + svnicmd)
        f = os.popen(svnicmd + " 2>&1", "r")
        lcrev = None
        lcout = ""
        while 1:
            line = f.readline()
            if line == "": break
            while line[-1:] == "\r" or line[-1:] == "\n":
                line = line[:-1]
            log.logoutput(line)
            if line[:18] == "Last Changed Rev: ":
                lcrev = string.atoi(line[18:])
        ret = f.close()
        if ret > 0:
            raise misc.builderr("svn info command terminated with status %d" % ret)
    else:
        # Manually fake up an 'svn export' via git.
        cdcmd = misc.shellquote(["cd", details[2]])
        gitlogcmd = misc.shellquote(["git", "ls-files"])
        f = os.popen(cdcmd + "&&" + gitlogcmd + " 2>&1", "r")
        while 1:
            s = f.readline()
            if s == "":
                break
            if s[-1:] == "\n": s = s[:-1]
            p = path + "/" + s
            i = string.rindex(p, "/")
            try:
                os.makedirs(p[:i])
            except OSError, e:
                pass # already exists
            shutil.copyfile(details[2] + "/" + s, p)
        f.close()
        lcrev = newrev
        if lcrev[-1:] == "M":
            lcrev = lcrev[:-1]
        lcrev = string.atoi(lcrev)

    if lcrev == None:
        raise misc.builderr("Unable to determine last-changed revision number for checked-out module `%s'" % module)
    log.logmsg("  Last-changed revision number for this checkout is %d" % lcrev)
    lastchange[lcrevindex] = max(lastchange.get(lcrevindex, 0), lcrev)
    log.logmsg("  Last-changed revision number for %s is %d" % (itostr(lcrevindex), lastchange[lcrevindex]))

    if set_headrev:
        headrevs[svnrepos] = newrev
        log.logmsg("  Using r%s for further head checkouts from %s" % (newrev, svnrepos))

    if is_main:
        lexer.set_multicharvar("crevision", newrev)
        lexer.set_multicharvar("revision", (get_lcrev, lcrevindex))
    lexer.set_multicharvar("crevision_" + module, newrev)
    lexer.set_multicharvar("revision_" + module, (get_lcrev, lcrevindex))
