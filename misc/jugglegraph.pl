#!/usr/bin/perl

# usage: jugglegraph.pl <nballs> <maxheight>

$n = shift @ARGV;
$max = shift @ARGV;

$nnodes = $ncomplete = 0;
@nodes = ();

$ground = (1 << $n) - 1;
&addnode($ground);

while ($nnodes > $ncomplete) {
  &donode($nodes[$ncomplete]);
  $ncomplete++;
}

print "Unreduced graph:\n";
foreach $link (sort { &lcmp($a,$b) } @links) {
  printf "%s --%d--> %s\n", &fmt($link->[0]),
    &fmtt($link->[2]), &fmt($link->[1]);
}

# Now reduce the graph. This is the fun bit!
while (1) {
  my $doneone = 0;
  FL: foreach $i (sort {$b <=> $a} keys %nodeexists) {
    if (!$linksto{$i.":".$i}) {
      &removenode($i);
      $doneone = 1;
      last FL;
    }
  }
  last if !$doneone;
}

print "Nodes in reduced graph:\n";
foreach $n (sort keys %nodeexists) {
  printf "%s\n", &fmt($n);
}

print "Reduced graph:\n";
foreach $link (sort { &lcmp($a,$b) } @links) {
  next unless $nodeexists{$link->[0]} and $nodeexists{$link->[1]};
  printf "%s --%s--> %s\n", &fmt($link->[0]),
    &fmtt($link->[2]), &fmt($link->[1]);
}

sub lcmp {
  my ($a,$b) = @_;
  my $i;
  return $a->[0] <=> $b->[0] if $a->[0] != $b->[0];
  return $a->[1] <=> $b->[1] if $a->[1] != $b->[1];
  for ($i = 0; $i <= $#{$a->[2]} && $i <= $#{$b->[2]}; $i++) {
    return $a->[2]->[$i] <=> $b->[2]->[$i] if $a->[2]->[$i] != $b->[2]->[$i];
  }
  return 0; # shouldn't happen
}

sub fmt {
  my ($n) = @_;
  my $s = "|";
  while ($n > 0) {
    $s .= ($n & 1) ? "x" : "-";
    $n >>= 1;
  }
  return $s;
}

sub fmtt {
  my ($t) = @_;
  my $i;

  $i = $max > 9 ? "," : "";
  return join $i,@$t;
}

sub addnode {
  my ($n) = @_;
  return if $nodeexists{$n};
  $nodeexists{$n} = 1;
  push @nodes, $n;
  $nnodes++;
  $linkfrom{$n} = [];
  $linkto{$n} = [];
}

sub addlink {
  my ($s, $d, $t) = @_;
  &addnode($s);
  &addnode($d);
  push @links, [$s,$d,$t];
  push @{$linkfrom{$s}}, [$d,$t];
  push @{$linkto{$d}}, [$s,$t];
  $linksto{$s.":".$d} = 1;
}

sub donode {
  my ($s) = @_;
  my $d;

  if ($s & 1) {
    # ball to throw; can throw anywhere there is a zero bit
    for ($i = 1; $i <= $max; $i++) {
      if ( ($s & (1 << $i)) == 0 ) {
        $d = ($s | (1 << $i)) >> 1;
	&addlink($s, $d, [$i]);
      }
    }
  } else {
    # no ball to throw; can only do zero
    $d = $s >> 1;
    &addlink($s, $d, [0]);
  }
}

sub removenode {
  my ($n) = @_;
  my $s,$d,$si,$di,@st,@dt,$t;
  foreach $si (@{$linkto{$n}}) {
    $s = $si->[0];
    next unless $nodeexists{$s};
    @st = @{$si->[1]};
    foreach $di (@{$linkfrom{$n}}) {
      $d = $di->[0];
      next unless $nodeexists{$d};
      @dt = @{$di->[1]};
      $t = [@st,@dt];
      &addlink($s, $d, $t);
    }
  }
  delete $nodeexists{$n};
}
