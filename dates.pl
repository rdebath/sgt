# Vaguely useful date related stuff in Perl

@mtab = (0,   # buffer because months start at 1
         0,   #jan
         31,  #feb
         59,  #mar (feb==28 and we correct for this elsewhere)
         90,  #apr
         120, #may
         151, #jun
         181, #jul
         212, #aug
         243, #sep
         273, #oct
         304, #nov
         334, #dec
         );

@mons = ('', 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
             'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec');

sub datenow() {
  my @foo = localtime;
  (1900+$foo[5]) * 10000 + (1+$foo[4]) * 100 + $foo[3];
}

sub datecvt($) {
  my $date = $_[0];
  my $y = int($date/10000);
  my $m = ($date/100) %100;
  my $d = $date%100;
  my $days;

  $days = int(($y - 1900) * 365.25);
  $days-- if ($y % 4 == 0 and $m < 3);
  $days += $mtab[$m];
  $days += $d-1;

  # FIXME Y2100 bug! (Leap years are simplistically assumed every 4.)
  $days;
}

sub datestr($) {
  my $yr, $mn, $dy;
  my ($d) = @_;
  $yr = 1900 + 4 * int(($d+1) / (4 * 365 + 1));
  $d = ($d+1) % (4 * 365 + 1);
  # now $d runs from 0 (1 Jan leapyear) to 1460 (31 Dec leapyear+3)
  if ($d == 59) { # leap day special case
    $mn = 2; $dy = 29;
  } else {
    $d-- if $d > 59;
    # now leap years can be ignored
    $yr += int($d / 365);
    $d %= 365;
    # now $d is day (0..364) within a nonleap year, and $yr is correct
    for ($mn = 1; $mn < 12; $mn++) {
      last if $d < $mtab[$mn+1];
    }
    $d -= $mtab[$mn];
    $dy = $d + 1;
  }
  $yr * 10000 + $mn * 100 + $dy;
}

# `today' - gotta be useful
sub today {
  &datecvt(&datenow);
}

# Day of week. Monday is 0, Sunday is 6.
sub weekday {
  $_[0] % 7;
}

# Return yyyymmdd form of Easter Sunday for a given year
sub easter {
  my ($year) = @_;
  my ($a,$b,$c,$d,$e,$f,$g,$h,$i,$k,$l,$m,$p,$month,$day);

  # Butcher's Algorithm, transcribed off Usenet. Hope it works.
  $a = $year % 19;
  $b = int( $year / 100 );
  $c = $year % 100;
  $d = int( $b / 4 );
  $e = $b % 4;
  $f = int( ($b + 8) / 25 );
  $g = int( ($b - $f + 1) / 3 );
  $h = (19*$a + $b - $d - $g + 15) % 30;
  $i = int( $c / 4 );
  $k = $c % 4;
  $l = (32 + 2*$e + 2*$i - $h - $k) % 7;
  $m = int( ($a + 11*$h + 22*$l) / 451 );
  $month = int(($h + $l - 7*$m + 114) / 31 );
  $p = ($h + $l - 7*$m + 114) % 31;
  $day = $p + 1;
  $year * 10000 + $month * 100 + $day;
}

# Return a list of English public holidays in a given year in
# yyyymmdd form.
#
# Algorithm:
#  - New Year's Day: first working day of January.
#  - Good Friday and Easter Monday: calculated by Butcher's algorithm
#    as implemented above.
#  - May Bank Holidays: first and last Mondays in May.
#  - August Bank Holiday: last Monday in August.
#  - Christmas and Boxing Day: first and second working days on or after
#    25th December.
#  - 19991231 is a special case, probably for the Mill*nnium.

sub brithols {
  my ($year) = @_;
  my ($yp, $h, $hd);
  my (@hols);

  $yp = 10000 * $year;
  @hols = ();

  # New Year Bank Holiday
  $h = &datecvt($yp+101);   # 1st January
  $h += 7-&weekday($h) if &weekday($h) >= 5;  # first working day after
  push @hols, &datestr($h); # and store it
  # Easter
  $h = &datecvt(&easter($year));  # Easter Sunday
  push @hols, &datestr($h-2), &datestr($h+1); # Friday and Monday
  # May Bank Holidays
  $h = &datecvt($yp+501);   # 1st May
  $h += (7-&weekday($h)) % 7; # first Monday on or after
  push @hols, &datestr($h); # and store it
  $h = &datecvt($yp+531);   # 31st May
  $h -= &weekday($h);       # last Monday on or before
  push @hols, &datestr($h); # and store it
  # August Bank Holiday
  $h = &datecvt($yp+831);   # 31st August
  $h -= &weekday($h);       # last Monday on or before
  push @hols, &datestr($h); # and store it
  # Christmas and Boxing Days
  $h = &datecvt($yp+1225);  # 25th December
  $h += 7-&weekday($h) if &weekday($h) >= 5;  # first working day after
  push @hols, &datestr($h); # and store it
  $h++;                     # next day after that
  $h += 7-&weekday($h) if &weekday($h) >= 5;  # first working day after
  push @hols, &datestr($h); # and store it
  # 19991231
  push @hols, 19991231 if $year == 1999;

  @hols;
}
