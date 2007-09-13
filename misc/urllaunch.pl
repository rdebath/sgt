#!/usr/bin/perl

# Script for automatically launching URLs.
#
#  - Will read data from the command line (if a URL argument is
#    supplied) or from the X selection via `xcopy -r' (if not).
#  - Will process that data into a plausible URL, by means of
#    stripping newlines and whitespace, prepending `http://' if it
#    looks like a good idea, or prepending `file://' if the data
#    begins with a single leading slash.
#  - If passed `-g', it will instead use the data to construct a
#    Google search URL.
#  - Will then launch the URL.
#
# Configuration files:
#
#  - `~/.urllaunch' is an optional piece of Perl which configures
#    the browser to use. It should define two variables, $try1 and
#    $try2. Each of them should be a piece of Perl which evaluates
#    to a list of strings suitable for passing to `exec' or
#    `system'. See the default definitions below for an example.
#
#  - `~/.urlrewrite' is an optional executable script. Once the
#    final URL has been constructed, it is piped through this
#    script if it exists. The script may produce no output if it
#    does not wish to rewrite the URL, or else it may output a
#    different URL (which must be fully formed).

$try1 = '"mozilla", "-remote", "openurl($url,new-window)"';
$try2 = '"mozilla", $url';

if (-f "$ENV{'HOME'}/.urllaunch") {
    open RC, "$ENV{'HOME'}/.urllaunch";
    $rc = "";
    $rc .= $_ while <RC>;
    close RC;
    eval $rc;
}

$google = 0;
$wikipedia = 0;
$rewrite = 1;
$opts = 1;
$url = undef;
while (scalar @ARGV) {
    $arg = shift @ARGV;
    if ($opts) {
        if ($arg eq "-g") { $google = 1; next; }
        if ($arg eq "-gq") { $google = 2; next; }
        if ($arg eq "-wp") { $wikipedia = 1; next; }
        if ($arg eq "-R") { $rewrite = 0; next; }
        if ($arg eq "--") { $opts = 0; next; }
        if ($arg =~ /^-/) { print STDERR "unknown option '$arg'\n"; next; }
    }
    $url = $arg;
}

$url = `xcopy -r` unless defined $url;

if ($google) {
    # Construct a Google search URL.
    $url =~ s/\n/ /gs;
    $url =~ s/^[\s\240]*//;
    $url =~ s/[\s\240]*$//;
    $url =~ s/[\s\240]+/ /g;
    $url = "\"$url\"" if $google == 2;
    $url =~ s/[^0-9a-zA-Z]/$& eq " " ? "+" : sprintf "%%%02x", ord $&/eg;

    $url = "http://www.google.co.uk/search?q=" . $url;
} elsif ($wikipedia) {
    # Construct a Wikipedia URL.
    $url =~ s/\n/ /gs;
    $url =~ s/^[\s\240]*//;
    $url =~ s/[\s\240]*$//;
    $url =~ s/[\s\240]+/_/g;
    $url =~ s/[^0-9a-zA-Z_]/$& eq " " ? "+" : sprintf "%%%02x", ord $&/eg;
    $url =~ s/^[a-z]/uc $&/e;

    $url = "http://en.wikipedia.org/wiki/" . $url;
} else {

    # Strip leading spaces.
    $url =~ s/^[\s\240]*//;
    # Strip every \n, plus any spaces just after that \n.
    $url =~ s/\n[\s\240]*//gs;
    # Prepend `http://' if there's no leading protocol prefix,
    # unless the string begins with a single slash in which case we
    # prepend `file://'.
    if ($url =~ /^[a-zA-Z]+:/) {
        # do nothing
    } elsif ($url =~ /^\/([^\/].*)?$/) {
        $url = "file://" . $url;
    } else {
        $url = "http://" . $url;
    }
}

# URL transformation.
if ($rewrite and -x "$ENV{'HOME'}/.urlrewrite") {
    # ~/.urlrewrite should be an executable program (or script)
    # which accepts a URL as its sole argument, and either produces
    # nothing on stdout (if the URL doesn't need rewriting) or
    # produces a rewritten one.
    $cmd = $url;
    $cmd =~ s/'/'\\''/g;
    $cmd = "$ENV{'HOME'}/.urlrewrite '$cmd'";
    $newurl = `$cmd`;
    $newurl =~ s/\n//gs;
    $url = $newurl if $newurl =~ /\/\//;
}

# Fork, in case we're running from the command line.
$pid = fork;
if ($pid < 0) {
    warn "fork: $!\n"; # but then continue regardless; it _might_ work!
} elsif ($pid > 0) {
    exit 0;
}

# Mozilla is a bit weird about launching URLs. If we just run
# `mozilla $url' it runs the risk of converting an _existing_
# Mozilla window into the new URL. Instead we must use a -remote
# command to open it in a new window - but that in turn won't work
# if there isn't a running Mozilla, so we must be alert for a
# failure return and launch `mozilla $url' if we see one.

open(STDERR, ">/dev/null");
eval "system $try1";

if ($? != 0) {
    eval "exec $try2";
}
