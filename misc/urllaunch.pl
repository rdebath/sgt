#!/usr/bin/perl 

$google = 0;
$rewrite = 1;
while (scalar @ARGV) {
    $arg = shift @ARGV;
    if ($arg eq "-g") { $google = 1; next; }
    if ($arg eq "-R") { $rewrite = 0; next; }
    print STDERR "unknown argument '$arg'\n"; next;
}

$url = `xcopy -r`;

if ($google) {
    # Construct a Google search URL.
    $url =~ s/\n/ /gs;
    $url =~ s/[\s\240]+/ /g;
    $url =~ s/[^0-9a-zA-Z]/$& eq " " ? "+" : sprintf "%%%02x", ord $&/eg;

    $url = "http://www.google.com/search?q=" . $url;
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
    print ":$cmd:$newurl:\n";
    $url = $newurl if $newurl =~ /\/\//;
}

# Mozilla is a bit weird about launching URLs. If we just run
# `mozilla $url' it runs the risk of converting an _existing_
# Mozilla window into the new URL. Instead we must use a -remote
# command to open it in a new window - but that in turn won't work
# if there isn't a running Mozilla, so we must be alert for a
# failure return and launch `mozilla $url' if we see one.

open(STDERR, ">/dev/null");
system "mozilla", "-remote", "openurl($url,new-window)";

if ($? != 0) {
    exec "mozilla", $url;
}
