#!/usr/bin/perl
$prev="";
while (<>) { &t; &h($prev,$end); print $line,"\n"; $prev=$end; }
&h($prev,"");

sub t {
  chomp;
  @a = unpack "C*", $_;
  $line = $end = "";
  $state = 0;
  foreach $i (@a) {
    $c = pack "C", $i;
    if ($i == 32) {
      # A space.
      if    ($state == 0) { $line .= "  ";   $end .= "  ";              }
      elsif ($state == 1) { $line .= " ";    $end .= " ";   $state = 0; }
      elsif ($state == 2) { $line .= " |";   $end .= "-+";  $state = 1; }
    } else {
      # A non-space.
      if    ($state == 0) { $line .= "| $c"; $end .= "+--"; $state = 2; }
      elsif ($state == 1) { $line .= " $c";  $end .= "--";  $state = 2; }
      elsif ($state == 2) { $line .= "$c";   $end .= "-";               }
    }
  }
  if ($state == 2) { $line .= " |"; $end .= "-+"; }
}

sub h {
  my ($x,$y) = @_;
  $prev = "";
  for ($i=0; $i<length $x or $i<length $y; $i++) {
    $c1 = substr($x,$i,1);
    $c2 = substr($y,$i,1);
    if    ($c1 eq "+" or $c2 eq "+") { $prev .= "+"; }
    elsif ($c1 eq "-" or $c2 eq "-") { $prev .= "-"; }
    else                             { $prev .= " "; }
  }
  print $prev,"\n";
}
