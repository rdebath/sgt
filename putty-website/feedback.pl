#!/usr/bin/perl

print << 'EOF';
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<HTML><HEAD>
<TITLE>PuTTY Feedback and Bug Reporting</TITLE>
<link rel="stylesheet" type="text/css" href="sitestyle.css" title="PuTTY Home Page Style">
</HEAD>
<BODY>
<h1 align=center>PuTTY Feedback and Bug Reporting</h1>

<p align=center>
<a href="./">Home</a> |
<a href="licence.html">Licence</a> |
<a href="faq.html">FAQ</a> |
<a href="docs.html">Docs</a> |
<a href="download.html">Download</a> |
<a href="keys.html">Keys</a><br>
<a href="mirrors.html">Mirrors</a> |
<a href="maillist.html">Updates</a> |
<b>Feedback</b> |
<a href="changes.html">Changes</a> |
<a href="wishlist.html">Wishlist</a></p>
EOF

open FILE, $ENV{"HOME"} . "/pub/putty/doc/AppendixB.html";
while (<FILE>) {
  last if /<body>/i;
}
# The line after <body> is the navigation links within the Halibut output
<FILE>;

while (<FILE>) {
  # The bottom navigation links terminate the file material;
  # conveniently, they use single quotes in the hrefs instead of
  # double.
  last if /^<p><a href='/i;
  # for each line, we must rewrite hrefs, although not if the href has
  # a slash in it.
  s/<a href="([^"\/]*)">/'<a href="' . &fix_href($1) . '">'/eg;
  print;
}

print << 'EOF';

<p><hr>If you want to comment on this web site, see the above guidelines.
<br>
(last modified on <!--LASTMOD-->[insert date here]<!--END-->)
</BODY></HTML>
EOF

sub fix_href {
  local ($_) = @_;
  /^news:/ or /^mailto:/ or
    s/^AppendixB.html// or
    s/^/http:\/\/www.tartarus.org\/~simon\/puttydoc\//;
  $_;
}
