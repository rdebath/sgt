#!/usr/bin/perl 

$url = `xcopy -r`;

$url =~ s/^[\s\240]*//;
$url =~ s/\n[\s\240]*//gs;

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
