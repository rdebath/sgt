#!/usr/bin/env perl
#
# usage:  timberb.pl filename {in|out}

open OUT,">".$ARGV[0] and select OUT;

# Detect a message separator line. Theoretically we need only require that
# the line begins "From ", but in practice some mail systems aren't strict
# about quoting other such lines, so we'll have to look for the whole form
# of a separator line, which is
#
#   From host.part Www Mmm dd hh:mm:ss yyyy
#
# Called with POINT at the beginning of the line.
sub issep($) {
  my ($k) = @_;
  $k =~ /^From [^ ]+ *( [A-Z][a-z]{2}){2} [\d ]\d [\d ]\d:\d\d:\d\d \d\d\d\d$/;
}

# Convert a raw mail folder into a Timber-style mail folder.
#
# Format of a Timber mail folder buffer:
#
# Each line has one header character denoting the type of line.
# `*' denotes a message summary line: one of these exists for each
# message. `|' denotes a message header line. ` ' denotes a message body
# line. `!' denotes a summary line for a MIME attachment. `X' denotes guff at
# the end of the folder. `F' denotes a summary line right at the top.
# `\240' (the other space char) denotes a decoded MIME attachment (eg
# decoded QP). `M' denotes a heading line for MIME summaries. `+' denotes
# a MIME-component header or admin line (including the separator lines
# themselves, the headers in each component, and the `This message is MIME'
# section before the components begin).
#
# Summary lines with nothing but ` [end]' after the initial character
# denote the end of the things they summarise.

# Loop over each message, inserting message summary lines and header
# characters.
$_ = <STDIN>;
while (1) {
  unless (issep($_)) {
    print "* [end]\n";
    print "X$_" while (<STDIN>);
    exit 0;
  }

  # Found a message start, right where we expected one. So start
  # a summary line.
  $sum = "* ";
  $msg = "|$_";

  # Initialise the summary stuff
  $fromfield = "*unspecified*";
  $subjfield = "*unspecified*";
  $datefield = "      ";
  $mimesep = "";
  $flagchr = 'n';
  # Get initial date field from Unix From line
  $datefield = "$2-$1" if /(...) (..) ..:..:.. ....$/;

  # Loop over each message header line, accumulating information
  # for the message summary line. Terminate if we see a blank line
  # (start of message body) or a `From ' message separator line.
  $_ = <STDIN>;
  while ($_ and /./ and !issep($_)) {
    $read = 0;
    $phdr = "|$_";
    if (/^From: (.*)$/i) {
      # FIXME: quoting and stuff is probably wrong here
      $a = $1;
      if ($a =~ /^(.+) <.*>/) {
        $fromfield = $1;
      } elsif ($a =~ /\((.+)\)/) {
        $fromfield = $1;
      } else {
        $fromfield = $a;
      }
    } elsif (/^Subject: (.*)$/i) {
      $subjfield = $1;
    } elsif (/^Content-type: (([^\/; ]+)\/[^\/; ]+)/i) {
      $conttype = $1;
      if (lc $2 eq "multipart") {
        chomp ($h = $_);
        $_ = <STDIN>;
        while ($_ and /^\s/) {
          $h .= $_;
          chomp $h;
          $phdr .= "|$_";
          $_ = <STDIN>;
        }
        $read = 1;
        if ($h =~ /boundary="([^"]*)"/i) {
          $mimesep = "--$1";
        } elsif ($h =~ /boundary=([^ \t;]*)/) {
          $mimesep = "--$1";
        } else {
          $mimesep = "";
        }
      }
    } elsif (/^Status: (.*)$/) {
      $stat = $1;
      $flagchr = {"O"=>"U", "RO"=>" ", "OR"=>" "}->{$stat};
      if (! defined $flagchr) {
        $flagchr = "N";
        $phdr = "|Status: O\n";
      }
    }
    $msg .= $phdr;
    $_ = <STDIN> unless $read;
  }

  # If no `Status:' header was found, put one in.
  if ($flagchr eq 'n') {
    $msg .= "|Status: O\n";
    $flagchr = 'N';
  }

  # We should have a blank line; that, at least, gets a nice
  # simple leading space, whether we're MIME or not.
  if (!/./) {
    $msg .= " \n";
    $_ = <STDIN>;
  }

  if ($mimesep ne "") {
    $mimepart = "";
    while ($_ and !issep($_)) {
      $mimepart .= $_;
      $_ = <STDIN>;
    }
    chomp $mimepart;
    $msg .= &multipart(0, $mimesep, $mimepart, $conttype);
  } else {
    # Loop over message body lines, inserting a leading space as
    # a header character. Terminate on `From ' separator line or
    # end of file.
    while ($_ and !issep($_)) {
      $msg .= " $_";
      $_ = <STDIN>;
    }
  }

  ($sizel = $msg) =~ y/\n//cd; $sizel = length $sizel;
  $sizec = (length $msg) - $sizel;

  $sum .= sprintf "%s   %3d %5d %-6.6s %-20.20s %-32s\n", $flagchr, $sizel,
                  $sizec, $datefield, $fromfield, $subjfield;

  print $sum; print $msg;
}

sub multipart {
  my ($level, $sep, $data, $type) = @_;
  local $_;
  my ($ret, $attach, $conttype, $contenc, $contdesc, $contname);
  my ($end, $blank, @data, $h, $raw, $cooked);

  $data =~ y/\n/\r/;
  @data = split /\r/, $data, -1;

  $end = $sep . "--";

  $blank = "";
  if ($data[$#data] eq "") {
    $blank = " \n";
    pop @data;
  }

  $ret .= ": " . ("  " x $level) . "MIME: " . lc $type . "\n";

  while (defined $data[0] and $data[0] ne $sep) {
    $ret .= "+" . $data[0] . "\n";
    shift @data;
  }

  while ($#data >= 0) {
    # Begin accumulating an attachment.
    $attach = "";
    $conttype = "";
    $contenc = "";
    $contdesc = "";
    $contname = "";
    $innersep = "";
    $_ = $data[0];
    last if $_ eq $end;
    while (defined $_ and /./) {
      $attach .= "+" . $_ . "\n";
      /Content-type: ([^\/; ]+\/[^\/; ]+)/i and do {
        $conttype = $1;
        while ($data[1] =~ /^\s/) {
          shift @data;
          $attach .= "+" . $data[0] . "\n";
          $_ .= $data[0];
        }
        /name="?([^"]*)/ and $contname = $1;
        if (/boundary="([^"]*)"/i) {
          $innersep = "--$1";
        } elsif (/boundary=([^ \t;]*)/) {
          $innersep = "--$1";
        } else {
          $innersep = "";
        }
      };
      /Content-description: (.*)$/i and $contdesc = $1;
      /Content-transfer-encoding: (.*)$/i and $contenc = $1;
      shift @data;
      $_ = $data[0];
    }
    $cooked = $raw = "";
    while (defined $_ and $_ ne $sep and $_ ne $end) {
      # raw form skips first blank line
      $raw .= $_ . "\n" unless $_ eq "" and $cooked eq "";
      $cooked .= " " . $_ . "\n";
      shift @data;
      $_ = $data[0];
    }
    $ret .= "! " . ("  " x $level) . lc $conttype;
    $ret .= sprintf " (%s)", lc $contenc if $contenc ne "";
    $ret .= " name=`$contname'" if $contname ne "";
    $ret .= " desc=`$contdesc'" if $contdesc ne "";
    # Possibly recurse. We may have to:
    #  - recurse directly if this is a multipart (done)
    #  - decode, do headers, and _then_ potentially recurse if this is
    #    a message/rfc822 (FIXME: not done yet, and _hard_)
    if ($conttype =~ /^multipart\//i) {
      $ret .= "\n" . $attach . " \n";
      chomp $raw;
      $ret .= &multipart($level+1, $innersep, $raw, $conttype);
    } else {
      $ret .= "\n" . $attach . $cooked;
    }
  }

  $ret .= "! " . ("  " x $level) . "[end]\n";

  while (defined $data[0] and $data[0] ne $sep) {
    $ret .= "+" . $data[0] . "\n";
    shift @data;
  }

  $ret .= $blank;
}
