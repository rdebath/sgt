#!/usr/bin/perl

use Fcntl;

$stdout = 1 if $ARGV[1] eq '-c';

$inbox = $ENV{"MAIL"};
$inbox = "/var/spool/mail/" . ((getpwuid($<))[0]) unless $inbox;

open(INBOX,$inbox) or exit 1;
open(MBOX,">>$ARGV[0]");

flock(INBOX, LOCK_EX);
while (<INBOX>) {
  print MBOX $_;
  print $_ if $stdout;
}
seek(INBOX,0,0);
open(INBOX2,">$inbox");
close(INBOX2);
flock(INBOX, LOCK_UN);

close INBOX;
close MBOX;
