#!/usr/bin/perl

print << 'EOF';
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<HTML><HEAD>
<TITLE>PuTTY FAQ</TITLE>
<link rel="stylesheet" type="text/css" href="sitestyle.css" title="PuTTY Home Page Style">
</HEAD>
<BODY>
<h1 align=center>PuTTY FAQ</h1>

<p align=center>
<a href="./">Home</a> |
<a href="licence.html">Licence</a> |
<b>FAQ</b> |
<a href="docs.html">Docs</a> |
<a href="download.html">Download</a> |
<a href="keys.html">Keys</a> |
<a href="links.html">Links</a><br>
<a href="mirrors.html">Mirrors</a> |
<a href="maillist.html">Updates</a> |
<a href="feedback.html">Feedback</a> |
<a href="changes.html">Changes</a> |
<a href="wishlist/">Wishlist</a> |
<a href="team.html">Team</a></p>
EOF

open FAQ, $ENV{"HOME"} . "/pub/putty/doc/AppendixA.html";
while (<FAQ>) {
  last if /<body>/i;
}
# The line after <body> is the navigation links within the Halibut output
<FAQ>;

while (<FAQ>) {
  # The bottom navigation links terminate the FAQ material;
  # conveniently, they use single quotes in the hrefs instead of
  # double.
  last if /^<p><a href='/i;
  # for each line, we must rewrite hrefs
  s/<a href="([^"\/]*)">/'<a href="' . &fix_href($1) . '">'/eg;
  # don't put in the </body> or </html>, since we're adding text on
  # the end
  s/<\/(body|html)>//ig;
  print;
}

print << 'EOF';

<br>
(last modified on <!--LASTMOD-->[insert date here]<!--END-->)
</BODY></HTML>
EOF

sub fix_href {
  local ($_) = @_;
  /^#/ or s/^AppendixA.html// or
      s/^/http:\/\/www.tartarus.org\/~simon\/puttydoc\//;
  $_;
}
