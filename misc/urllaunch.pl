#!/usr/bin/perl 

$url = `xcopy -r`;

# Strip leading spaces.
$url =~ s/^[\s\240]*//;
# Strip every \n, plus any spaces just after that \n.
$url =~ s/\n[\s\240]*//gs;
# Prepend `http://' if there's no leading protocol prefix.
$url = "http://" . $url unless $url =~ /^[a-zA-Z]+:/;

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
