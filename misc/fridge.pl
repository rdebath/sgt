#!/usr/bin/perl
$p="";
while (<>) { &t; &h($p,$_); print $_,"\n"; $p=$_; }
&h($p,"");

sub t {
  chomp;
  s/ / | /g;
  $_ = "| $_ |";
}

sub h {
  my ($x,$y) = @_;
  $h = "-" x length $x;
  $h = "-" x length $y if length $y > length $x;
  for ($i=0; $i<length $h; $i++) {
    substr($h,$i,1) = "+" if substr($x,$i,1) eq "|" or substr($y,$i,1) eq "|";
  }
  print $h,"\n";
}
