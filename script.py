# Script execution engine for my build system.

import sys
import os
import glob
import struct
import shutil
import string

import lexer
import log
import checkout
import misc
import name

try:
    # New Python 2.6 way of spawning subprocesses
    import subprocess
    def popen2(command):
        p = subprocess.Popen(command, shell=True, stdin=subprocess.PIPE, \
                                 stdout=subprocess.PIPE, close_fds=True)
        return (p.stdin, p.stdout)
except ImportError, e:
    # Old-style fallback, deprecated in 2.6
    from os import popen2

delegatefps = None

def run_script_line(s, is_config, cfg):
    global delegatefps

    # Execute the script line given in s.

    # Trim the newline off the end of the string, to begin with.
    while s[-1:] == "\r" or s[-1:] == "\n":
        s = s[:-1]

    w, sr = lexer.get_word(s)

    if w == None or w == "":
        return # no command on this line

    # Log every line executed by a non-config script.
    if not is_config:
        log.logscript(s)

    if w == "ifeq" or w == "ifneq":
        w1, sr = lexer.get_word(sr)
        w2, sr = lexer.get_word(sr)
        log.logmsg("testing string equality of `%s' and `%s'" % (w1, w2))
        if (w1 == w2) != (w == "ifeq"):
            return # condition not taken
        w, sr = lexer.get_word(sr) # now read the main command
    if w == "ifexist" or w == "ifnexist":
        if is_config:
            raise misc.builderr("`%s' command invalid in config file" % w)
        if not cfg.seen_module:
            raise misc.builderr("`%s' command seen before `module' command" % w)
        w1, sr = lexer.get_word(sr)
        log.logmsg("testing existence of `%s'" % w1)
        if (os.path.exists(os.path.join(cfg.workpath,w1))!=0) != (w=="ifexist"):
            return # condition not taken
        w, sr = lexer.get_word(sr) # now read the main command

    if w == "set":
        # Set a variable.
        var, val = lexer.get_word(sr)
        val = lexer.lex_all(lexer.trim(val))
        if not is_config:
            log.logmsg("Setting variable `%s' to value `%s'" % (var,val))
        lexer.set_multicharvar(var, val)
    elif w == "in" or w == "in-dest":
        if is_config:
            raise misc.builderr("`%s' command invalid in config file" % w)
        if not cfg.seen_module:
            raise misc.builderr("`%s' command seen before `module' command" % w)
        if delegatefps != None and w != "in":
            raise misc.builderr("`in-dest' command invalid during delegation" % w)
        dir, sr = lexer.get_word(sr)
        do, sr = lexer.get_word(sr)
        if do != "do":
            raise misc.builderr("expected `do' after `%s'" % w)
        cmd = lexer.lex_all(lexer.trim(sr))
        if delegatefps != None:
            log.logmsg("Running command on delegate server: " + cmd)
            # Instead of running the command locally, send it to
            # the delegate host, and receive in return some output
            # and an exit code.
            delegatefps[0].write("C" + struct.pack(">L", len(dir)) + dir + struct.pack(">L", len(cmd)) + cmd)
            delegatefps[0].flush()

            # Retrieve the build command's output, line by line.
            output = ""
            while 1:
                outlen = delegatefps[1].read(4)
                if len(outlen) < 4:
                    raise misc.builderr("unexpected EOF from delegate server")
                outlen = struct.unpack(">L", outlen)[0]
                if outlen == 0:
                    break
                outchunk = delegatefps[1].read(outlen)
                if len(outchunk) < outlen:
                    raise misc.builderr("unexpected EOF from delegate server")
                output = output + outchunk
                while 1:
                    newline = string.find(output, "\n")
                    if newline < 0:
                        break
                    line = output[:newline]
                    output = output[newline+1:]
                    while line[-1:] == "\r" or line[-1:] == "\n":
                        line = line[:-1]
                    log.logoutput(line)

            # Log the final partial line, if any.
            if len(output) > 0:
                while output[-1:] == "\r" or output[-1:] == "\n":
                    output = output[:-1]
                log.logoutput(output)

            exitcode = delegatefps[1].read(4)
            if len(exitcode) < 4:
                raise misc.builderr("unexpected EOF from delegate server")
            exitcode = struct.unpack(">l", exitcode)[0]

            if exitcode > 0:
                raise misc.builderr("build command terminated with status %d" % exitcode)

        else:
            if w == "in-dest":
                dir = os.path.join(cfg.outpath, dir)
            else:
                dir = os.path.join(cfg.workpath, dir)
            log.logmsg("Running command in directory `%s': %s" % (dir, cmd))
            cmd = misc.shellquote(["cd", dir]) + " && " + cmd
            f = os.popen(cmd + " 2>&1", "r")
            while 1:
                line = f.readline()
                if line == "": break
                while line[-1:] == "\r" or line[-1:] == "\n":
                    line = line[:-1]
                log.logoutput(line)
            ret = f.close()
            if ret > 0:
                raise misc.builderr("build command terminated with status %d" % ret)
    elif w == "deliver":
        if is_config:
            raise misc.builderr("`%s' command invalid in config file" % w)
        if not cfg.seen_module:
            raise misc.builderr("`%s' command seen before `module' command" % w)
        srcpath, sr = lexer.get_word(sr)
        sr = lexer.trim(sr)
        nfiles = 0
        for srcfile in glob.glob(os.path.join(cfg.workpath, srcpath)):
            save = lexer.save_vars()
            lexer.set_onecharvar("@", os.path.basename(srcfile))
            dstfile, sx = lexer.get_word(sr)
            lexer.restore_vars(save)
            dstfile = os.path.join(cfg.outpath, dstfile)
            log.logmsg("Delivering `%s' to `%s'" % (srcfile, dstfile))
            dstdir = os.path.dirname(dstfile)
            if not os.path.exists(dstdir):
                os.makedirs(dstdir)
            shutil.copyfile(srcfile, dstfile)
            nfiles = nfiles + 1

        if nfiles == 0:
            raise misc.builderr("deliver statement did not match any files")

    elif w == "checkout":
        if is_config:
            raise misc.builderr("`%s' command invalid in config file" % w)
        if not cfg.seen_module:
            raise misc.builderr("`%s' command seen before `module' command" % w)
        module, sr = lexer.get_word(sr)
        destdir, sr = lexer.get_word(sr)
        if module == None or destdir == None:
            raise misc.builderr("`checkout' command expects two parameters")
        destdir = os.path.join(cfg.workpath, destdir)
        checkout.checkout(cfg, module, destdir, 0)
    elif w == "module":
        if is_config:
            raise misc.builderr("`%s' command invalid in config file" % w)
        newmodule, sr = lexer.get_word(sr)
        if newmodule == None:
            raise misc.builderr("`module' command expects a parameter")
        srcdir = os.path.join(cfg.workpath, cfg.mainmodule)
        destdir = os.path.join(cfg.workpath, newmodule)
        if srcdir == destdir:
            log.logmsg("main module already has correct filename")
        else:
            log.logmsg("renaming main module directory `%s' to `%s'" % (srcdir, destdir))
            os.rename(srcdir, destdir)
            cfg.mainmodule = newmodule
        cfg.seen_module = 1
    elif w == "delegate":
        if is_config:
            raise misc.builderr("`%s' command invalid in config file" % w)
        if not cfg.seen_module:
            raise misc.builderr("`%s' command seen before `module' command" % w)
        hosttype, sr = lexer.get_word(sr)
        if hosttype == None:
            raise misc.builderr("expected a host type after `delegate'")
        if delegatefps != None:
            raise misc.builderr("a delegation session is already open")

        # Read the config file to find out what actual host to
        # connect to for the given host type.
        save = lexer.save_vars()
        process_script(cfg.cfgfile, 1, cfg)
        host = lexer.get_multicharvar("host_" + hosttype)
        sshid = lexer.get_multicharvar("id_" + hosttype)
        usercmd = lexer.get_multicharvar("cmd_" + hosttype)
        lexer.restore_vars(save)
        if host == "":
            raise misc.builderr("configuration does not specify a host for delegate type `%s'" % hosttype)

        # Open a connection to the delegate host.
        log.logmsg("Starting delegation to host type `%s'" % hosttype)
        if usercmd != None:
            delcmd = usercmd
        else:
            if hosttype == "-":
                # Special case: a host name of "-" causes a
                # self-delegation, i.e. we invoke the delegate
                # server directly rather than bothering with ssh.
                for pdir in sys.path:
                    delcmd = pdir + "/" + name.server
                    if os.path.exists(delcmd):
                        break
                    delcmd = None
                if delcmd == None:
                    raise misc.builderr("unable to find delegate server")
                delcmd = [delcmd]
            else:
                delcmd = ["ssh"]
                # If the user has specified an SSH identity key, use it.
                if sshid != None:
                    delcmd = ["SSH_AUTH_SOCK="] + delcmd + ["-i", sshid]
                delcmd.append(host)
                delcmd.append(name.server)
            delcmd = misc.shellquote(delcmd)
        log.logmsg("  Running delegation command: " + delcmd)
        delegatefps = popen2(delcmd)

        # Wait for the announcement from the far end which says the
        # delegate server is running.
        while 1:
            s = delegatefps[1].readline()
            if s == "":
                raise misc.builderr("unexpected EOF from delegate server")
            while s[-1:] == "\r" or s[-1:] == "\n":
                s = s[:-1]
            if s == name.server_banner:
                log.logmsg("  Successfully started delegate server")
                break

        # Send a tarball of our build work directory.
        tarpipe = os.popen(misc.shellquote(["tar", "-C", cfg.workpath, "-czf", "-", "."]), "r")
        data = tarpipe.read()
        tarpipe.close()
        delegatefps[0].write("T" + struct.pack(">L", len(data)) + data)
        delegatefps[0].flush()
    elif w == "return":
        if is_config:
            raise misc.builderr("`%s' command invalid in config file" % w)
        if not cfg.seen_module:
            raise misc.builderr("`%s' command seen before `module' command" % w)
        if delegatefps == None:
            raise misc.builderr("no delegation session open")

        # Copy file(s) back from the delegate host. We send our
        # command character "R", then a glob pattern; we then
        # repeatedly read a filename and file contents until we
        # receive zero filename length.
        pattern, sr = lexer.get_word(sr)
        if pattern == None:
            raise misc.builderr("expected a file name after `return'")
        delegatefps[0].write("R" + struct.pack(">L", len(pattern)) + pattern)
        delegatefps[0].flush()

        nfiles = 0
        while 1:
            fnamelen = delegatefps[1].read(4)
            if len(fnamelen) < 4:
                raise misc.builderr("unexpected EOF from delegate server")
            fnamelen = struct.unpack(">L", fnamelen)[0]
            if fnamelen == 0:
                break
            fname = delegatefps[1].read(fnamelen)
            if len(fname) < fnamelen:
                raise misc.builderr("unexpected EOF from delegate server")
            datalen = delegatefps[1].read(4)
            if len(datalen) < 4:
                raise misc.builderr("unexpected EOF from delegate server")
            datalen = struct.unpack(">L", datalen)[0]
            data = delegatefps[1].read(datalen)
            if len(data) < datalen:
                raise misc.builderr("unexpected EOF from delegate server")
            log.logmsg("Returned file `%s' from delegate server" % fname) #'
            # Vet the filename for obvious gotchas.
            if string.find("/"+fname+"/", "/../") >= 0 or fname[:1] == "/":
                raise misc.builderr("returned file `%s' failed security check" % fname) #'
            dstfile = os.path.join(cfg.workpath, fname)
            dstdir = os.path.dirname(dstfile)
            if not os.path.exists(dstdir):
                os.makedirs(dstdir)
            outfp = open(dstfile, "wb")
            outfp.write(data)
            outfp.close()
            nfiles = nfiles + 1

        if nfiles == 0:
            raise misc.builderr("return statement did not match any files")

    elif w == "enddelegate":
        if is_config:
            raise misc.builderr("`%s' command invalid in config file" % w)
        if not cfg.seen_module:
            raise misc.builderr("`%s' command seen before `module' command" % w)
        if delegatefps == None:
            raise misc.builderr("no delegation session open")

        # Close the delegate session
        delegatefps[0].write("Q")
        delegatefps[0].close()
        delegatefps[1].close()
        delegatefps = None

        log.logmsg("Closed delegate session")
    else:
        raise misc.builderr("unrecognised statement keyword `%s'" % w)

def process_script(scriptfile, is_config, cfg):
    # Log the start and end of script processing if this isn't the
    # config file.
    if not is_config:
        log.logmsg("Opening script file %s" % scriptfile)
        cfg.seen_module = 0
    scriptfp = open(scriptfile, "r")
    while 1:
        line = scriptfp.readline()
        if line == "": break
        run_script_line(line, is_config, cfg)
    scriptfp.close()
    if not is_config:
        log.logmsg("Closing script file %s" % scriptfile)
