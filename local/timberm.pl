#!/usr/local/gnu/bin/perl
#
# usage:  timberm.pl filename type

sub base64_decode($);

$stripspace = 0;

open F,$ARGV[0] or die "Unable to open $ARGV[0]\n";

$type = $ARGV[1];

(open FOO,">".$ARGV[2]), select FOO if defined $ARGV[2];

if ($type =~ /[A-Z]/) {
  $stripspace = 1;
  $type =~ y/A-Z/a-z/;
}

if ($type eq "quoted-printable") {
  while (<F>) {
    s/^ // if $stripspace;
    s/=\n//;
    $newl = chomp;
    while (/^([^=]*)=(..)(.*)$/) {
      print $1;
      $x = $2;
      $_ = $3;
      print "?" unless $x =~ /[0-9A-Fa-f][0-9A-Fa-f]/;
      print chr(hex($x));
    }
    print $_;
    print "\n" if $newl;
  }
} elsif ($type eq "base64") {
  $chars = '';
  while (<F>) {
    y/A-Za-z0-9+\/=//cd;
    y/A-Za-z0-9+\/=/\000-\100/;
    $chars .= $_;
    while (length($chars) >= 4) {
      print base64_decode $chars;
      $chars = unpack("x4 a*",$chars);
    }
  }
} elsif ($type eq "x-uuencode") {
  while (<F>) {
    s/^ // if $stripspace;
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
      print $out;
    }
  }
} else {
  while (<F>) {
    s/^ // if $stripspace;
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
