#!/usr/bin/perl 

use POSIX;

if ($ARGV[0] eq "--client") {

    # Client mode, called from Jed as a subprocess. Simply copy
    # from fd n to stdout until we get bored.
    $fd = $ARGV[1];
    open PIPE, "<&$fd" or die "unable to open pipe!\n";
    while (($ret = sysread PIPE, $a, 512) > 0) {
        syswrite STDOUT, $a, $ret;
    }
    exit 0;

} else {

    # Directories to search for source files.
    @dirs = @ARGV;

    pipe READ, WRITE;
    setpgrp;

    $pid = fork;
    if ($pid < 0) { die "jed-gdb: fork: $!\n"; }

    $SIG{"CHLD"} = sub { wait; };

    if ($pid == 0) {
        # we are the child
        close WRITE;
        fcntl READ, F_SETFD, 0;
        $fd = fileno(READ);
        exec "jed", "-l", "dbg.sl", "-f", ". $fd dbg_tracker";
    } else {
        close READ;
        # we are the parent. Copy stdin to stdout without delay,
        # and also scan it for stuff as it goes past.
        $data = 0;
        $prefix = '';
        $fn = undef;
        while (($ret = sysread STDIN, $a, 512)) {
            syswrite STDOUT, $a, $ret;
            $data .= $a;
            while ($data =~ /^([^\n]*)\n(.*)$/s) {
                $line = $prefix . $1;
                $data = $2;
                $prefix = '';
                if ($line =~ /WARNING: Source position to address conversion weak/) {
                    $prefix = $`;
                    next;
                }

                $lineno = undef;

                if ($line =~ /at ([^: ]+):(\d+)$/) {
                    $col = 1;
                    $fn = undef;
                    $name = $1;
                    # $lineno = $2;
                    foreach $dir (@dirs) {
                        $fn = "$dir/$name";
                        $fn =~ s:/+:/:g;
                        last if -f $fn;
                    }
                } elsif ($line =~ /^(\d+)\s/ and defined $fn) {
                    $col = 1;
                    $lineno = $1;
                    # $fn is kept from previous attempt.
                }

                if (defined $fn and defined $lineno) {
                    $writedata = "$lineno:$col:$fn\n";
                    syswrite WRITE, $writedata;
                }
            }
        }
    }

}
