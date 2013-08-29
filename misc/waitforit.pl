#!/usr/bin/perl

use Time::Local;

$date = shift @ARGV;
$time = shift @ARGV;

if (!defined $date || !defined $time) {
    die "usage: waitforit.pl YYYY-MM-DD HH:MM[:SS] <command> <args>\n";
}

$date =~ /^(\d+)-(\d+)-(\d+)$/ or do {
    die "date was not in YYYY-MM-DD format\n";
};
($year,$mon,$day) = ($1,$2,$3);

$time =~ /^(\d+):(\d+)(:(\d+))?$/ or do {
    die "time was not in HH:MM[:SS] format\n";
};
($hour,$min,$sec) = ($1,$2,$4);

# adjust month by 1 because timelocal() expects jan=0 dec=11
$time = timelocal($sec,$min,$hour,$day,$mon-1,$year);

if (($now = time) > $time) {
    warn "warning: selected time is already in the past\n";
} else {
    warn sprintf "waiting %d seconds...\n", $time - $now;
}

# Wait for that time. (Loop round out of sheer paranoia!)
while (($now = time) < $time) {
    $seconds = $time - $now;
    sleep $seconds;
}

# Now do the thing.
exec @ARGV;
