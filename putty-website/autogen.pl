#!/usr/bin/perl
# usage: autogen.pl <destfile> eg. autogen.pl faq.html

@files = (
  [ "faq.html",      "AppendixA.html", "FAQ", "PuTTY FAQ" ],
  [ "feedback.html", "AppendixB.html", "Feedback",
        "PuTTY Feedback and Bug Reporting" ],
  [ "keys.html",     "AppendixE.html", "Keys",
        "PuTTY Download Keys and Signatures" ]
);

foreach (@files) {
    ($dstfn, $srcfn, $sect, $title) = @$_ if ($ARGV[0] eq $_->[0]);
}
die "must specify a valid page" unless defined($dstfn);

print << "EOF";
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<HTML><HEAD>
<TITLE>$title</TITLE>
<link rel="stylesheet" type="text/css" href="sitestyle.css" title="PuTTY Home Page Style">
<link rel="shortcut icon" href="putty.ico">
<meta http-equiv="content-type" content="text/html; charset=US-ASCII">
</HEAD>
<BODY>
<h1 align=center>$title</h1>
<!--MIRRORWARNING-->

<p align=center>
EOF

print lnk("./", "Home") . " |\n" .
      lnk("licence.html", "Licence") . " |\n" .
      lnk("faq.html", "FAQ") . " |\n" .
      lnk("docs.html", "Docs") . " |\n" .
      lnk("download.html", "Download") . " |\n" .
      lnk("keys.html", "Keys") . " |\n" .
      lnk("links.html", "Links") . "<br>\n" .
      lnk("mirrors.html", "Mirrors") . " |\n" .
      lnk("maillist.html", "Updates") . " |\n" .
      lnk("feedback.html", "Feedback") . " |\n" .
      lnk("changes.html", "Changes") . " |\n" .
      lnk("wishlist/", "Wishlist") . " |\n" .
      lnk("team.html", "Team") . "</p>\n";

sub lnk {
  ($dst, $capt) = @_;
  if ($capt eq $sect) {
    return "<b>$capt</b>";
  } else {
    return "<a href=\"$dst\">$capt</a>";
  }
}

open FILE, $ENV{"HOME"} . "/pub/putty/doc/" . $srcfn;
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
  # for each line, we must rewrite hrefs
  s/<a href="([^"]*)">/'<a href="' . &fix_href($1) . '">'/eg;
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
  if (s#^\Qhttp://www.chiark.greenend.org.uk/~sgtatham/putty/\E(.+)#$1# or
    /\// or
    /^news:/ or /^mailto:/ or
    /^#/ or s/^\Q$srcfn\E//) {
    return $_;
  } else {
    foreach my $file (@files) {
      return $_ if s#^\Q$file->[1]\E#$file->[0]#;
    }
    s#^#http://tartarus.org/~simon/putty-snapshots/htmldoc\/#;
    return $_;
  }
}
