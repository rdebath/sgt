#!/usr/bin/perl
#
# usage:  timberm.pl filename type

sub base64_decode($);

$stripspace = 0;
$remcr = 0;

open F,$ARGV[0] or die "Unable to open $ARGV[0]\n";

$type = $ARGV[1];

open STDOUT,">".$ARGV[2] if defined $ARGV[2];

if ($type =~ /[A-Z]/) {
  $stripspace = 1;
  $type =~ y/A-Z/a-z/;
}

if ($type =~ /\+$/) {
  $remcr = 1;
  $type =~ s/\+$//;
}

# MIME types
if ($type eq "text/html") {
  open PIPE, "|perl " . $ENV{'TIMBER_SCRIPTS'} . "/timberh.pl -w 78 -i 0 -b -e";
  while (<F>) {
    s/^[ \240]// if $stripspace;
    print PIPE $_;
  }
  close PIPE;
  exit 0;
} elsif ($type eq "text/plain" or $type eq "text") {
  # A sensible `decoding' step for plain text is to wrap its lines to about
  # 75 columns, since some people will persist in sending mail with each
  # paragraph on a single line. We'll also detab, while we're here.
  while (<F>) {
    1 while s/\t/" " x (8 - (length $`) % 8)/e;
    s/^[ \240]// if $stripspace;
    chomp;
    while (length $_ > 75) {
      /^(.{0,75})\s(.*)$/ or /^(.{0,75})(.*)$/;
      $_ = $2;
      print "$1\n";
    }
    print "$_\n";
  }
  exit 0;
}

# MIME transfer encodings. If we get one of these and $stripspace
# is set, we MUST destroy all input lines that do not start with a
# real space or plus sign, and strip the ones that do.
if ($type eq "quoted-printable") {
  while (<F>) {
    if ($stripspace) {
      next if !/^[ \+]/;
      s/^[ \+]//;
    }
    s/=\n//;
    $newl = chomp;
    while (/^([^=]*)=(..)(.*)$/) {
      print &remcr($1);
      $x = $2;
      $_ = $3;
      print "?" unless $x =~ /[0-9A-Fa-f][0-9A-Fa-f]/;
      print &remcr(chr(hex($x)));
    }
    print $_;
    print "\n" if $newl;
  }
} elsif ($type eq "base64") {
  $chars = '';
  while (<F>) {
    if ($stripspace) {
      next if !/^[ \+]/;
      s/^[ \+]//;
    }
    # Deal with strange footers on the base64 data, for example added by
    # Mailman in the case when a message body is a single base64 part.
    last if /[^A-Za-z0-9+\/= \r\n\t]/;
    y/A-Za-z0-9+\/=//cd;
    y/A-Za-z0-9+\/=/\000-\100/;
    $chars .= $_;
    while (length($chars) >= 4) {
      print &remcr(base64_decode $chars);
      $chars = unpack("x4 a*",$chars);
    }
  }
} elsif ($type eq "x-uuencode") {
  while (<F>) {
    if ($stripspace) {
      next if !/^[ \+]/;
      s/^[ \+]//;
    }
    @octets = map { $_ == 0x60 ? 0 : $_ - 0x20 } unpack "C*", $_;
    $len = shift @octets;
    if ($len > 0 and $len <= 45) {
      $out = '';
      while ($len > 0 and length @octets) {
        $out .= chr(($octets[0] << 2) | ($octets[1] >> 4)) if $len-- > 0;
        $out .= chr(($octets[1] << 4) | ($octets[2] >> 2)) if $len-- > 0;
        $out .= chr(($octets[2] << 6) | ($octets[3])) if $len-- > 0;
        splice @octets,0,4;
      }
      print &remcr($out);
    }
  }
} else {
  while (<F>) {
    if ($stripspace) {
      next if !/^[ \+]/;
      s/^[ \+]//;
    }
    print;
  }
}

sub base64_decode($) {
  my ($a,$b,$c,$d) = unpack("C4", $_[0]);
  my $stuff = ($a<<18) | ($b<<12) | ($c<<6) | $d;
  if ($c == 64) {
    $stuff = pack("C", ($stuff >> 16) & 255);
  } elsif ($d == 64) {
    $stuff = pack("CC", ($stuff >> 16) & 255, ($stuff >> 8) & 255);
  } else {
    $stuff = pack("CCC", ($stuff >> 16) & 255, ($stuff >> 8) & 255,
                  $stuff & 255);
  }
  $stuff;
}

sub remcr {
  my ($data) = @_;
  $data =~ y/\r//d if $remcr;
  $data;
}
