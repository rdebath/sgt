: # use perl                                  -*- mode: Perl; -*-
#
# Convert HTML on standard input to plain text on standard output.
#
# Version: 0.05
#
# Still to do:
#
#   + fix dodgy handling of &#160; mixed with spaces
#   + handle <OL TYPE="i">, i.e. lists enumerated by roman numerals
#
# Distributed under the terms of the GNU General Public License; either
# version 2, or at your option any later version.
#
# 13 July 1999 - (C) Andrew Wood <andrew.wood@poboxes.com>

eval 'exec perl -S $0 "$@"'
     if $running_under_some_shell;

$running_under_some_shell = 0;		# shut up `perl -w'
$help = $version = "";			# ditto

$help = "Usage: $0 [OPTIONS] [FILE...]
Read FILE(s), or standard input, as HTML and output on stdout as plain text.

  -w, --width=WIDTH          output text formatted to WIDTH characters wide
  -l, --left-indent=INDENT   default gap of INDENT characters at the left
  -r, --right-indent=INDENT  default gap of INDENT characters at the right
  -i, --indent=INDENT        set left and right indent to INDENT characters
  -b, --no-body              do not require <BODY>...</BODY> tags
  -e, --extra-space          allow consecutive <BR> and <P> to make gaps
  -a, --all-links            dump all hyperlinks, not just full URLs
  -n, --no-links             do not dump any hyperlinks at all, ignore them

      --help                 display this help and exit
  -V, --version              display version number and exit

Report bugs to <andrew.wood\@poboxes.com>.";

$version = "$0 version 0.05
Copyright (C) 1999 Andrew Wood <andrew.wood\@poboxes.com>

Distributed under the terms of the GNU General Public License; either
version 2, or at your option any later version.

Note that this program comes with ABSOLUTELY NO WARRANTY - for more
information please see the License.";

$storestr = "";
@args = ();

%long_options = (			# long command line options
  'width'	=> 'A width',
  'left-indent'	=> 'A indent_l',
  'right-indent'=> 'A indent_r',
  'indent'	=> 'A indent',
  'no-body'	=> 'C $ignore_body = 1;',
  'extra-space'	=> 'C $conseq_br = 1;',
  'all-links'	=> 'C $all_links = 1;',
  'no-links'	=> 'C $no_links = 1;',
  'help'	=> 'C print "$help\n"; exit;',
  'version'	=> 'C print "$version\n"; exit;'
);

%short_options = (			# short command line options
  'w'	=> 'width',
  'l'	=> 'left-indent',
  'r'	=> 'right-indent',
  'i'	=> 'indent',
  'b'	=> 'no-body',
  'e'	=> 'extra-space',
  'a'	=> 'all-links',
  'n'	=> 'no-links',
  'h'	=> 'help',
  'V'	=> 'version'
);

$width = 80;
$indent = -1;
$indent_l = 2;
$indent_r = 2;
$ignore_body = 0;
$conseq_br = 0;
$all_links = 0;
$no_links = 0;

$item = shift @ARGV;

do {					# read command-line arguments
  $item = "" if (not defined $item);

  if ($storestr ne "") {			# store parameter
    eval "\$$storestr = \"$item\";\n";
    $storestr = "";
  } elsif ($item =~ /^-([A-Za-z]+)$/) {		# short item
    $opts = $1;
    foreach $letter (split (//, $opts)) {
      if (not defined ($short_options{$letter})) {
        print STDERR "$0: illegal option -- $letter\n";
        print STDERR "Try \`$0 --help' for more information.\n";
        exit 1;
      }
      $longopt = $short_options{$letter};
      if ($long_options{$longopt} =~ /^A (.*)$/) {
        $storestr = "$1";
      } elsif ($long_options{$longopt} =~ /^C (.*)$/) {
        eval "$1";
      }
    }
  } elsif ($item =~ /^--([-A-Za-z]+)(=.*)?$/) {	# long item
    undef $value;
    $item = "$1";
    if (defined $2) {
      $value = "$2";
      $value =~ s/^=//;
    }
    if (not defined ($long_options{$item})) {
      print STDERR "$0: illegal option -- $item\n";
      print STDERR "Try \`$0 --help' for more information.\n";
      exit 1;
    }
    if ($long_options{$item} =~ /^A (.*)$/) {
      $storestr = "$1";
      if (defined $value) {
        eval "\$$storestr = \"$value\";";
        $storestr = "";
      }
    } elsif ($long_options{$item} =~ /^C (.*)$/) {
      eval "$1";
    }
  } elsif ($item ne "") {			# add parameter to arg list
    push (@args, $item);
  }

  $item = shift @ARGV;
} while (defined $item);

if ($indent != -1) {			# --indent: set left and right indent
  $indent_l = $indent;
  $indent_r = $indent;
}

undef $/;				# read files in one gulp

if ($#args >= 0) {
  foreach $file (@args) {
    open (FILE, "<$file") or die "$0: $file: $!\n";
    $html = <FILE>;
    close (FILE);
    $text = html_to_text ($html, $width, $indent_l,
                          $indent_r, $ignore_body, $conseq_br,
                          $all_links, $no_links);
    print "$text";
  }
} else {
  $html = <>;
  $text = html_to_text ($html, $width, $indent_l,
                        $indent_r, $ignore_body, $conseq_br,
                        $all_links, $no_links);
  print "$text";
}

exit;



# Convert the string $1 from HTML to text, formatting to a width of $2 with
# an initial indentation of $3 to the left and $4 to the right.
#
# If $5 is non-zero, text outside <BODY>...</BODY> will still be displayed
# (although <TITLE> text will still be discarded).
#
# If $6 is non-zero, consecutive <BR>s and <P>s (i.e. lots of vertical
# whitespace) will not be suppressed.
#
# If $7 is non-zero, all <A HREF=...> hyperlinks, not just those specifying
# a full URL, will be dumped. If $8 is non-zero, hyperlinks will be ignored.
#
sub html_to_text {
  local ($html, $ef_WIDTH, $ef_IDL, $ef_IDR,
         $opt_body, $opt_br, $opt_al, $opt_nl) = @_;
  my ($foo, $n, $i, $chunk, $line);

  local %taglist = (	# list of tags with actions - <FLAGS> <ACTIONS>
    'A' => 'C',			# <FLAGS>: C = container
    'ABBREV' => 'C',		#          E = equivalent to tag <ACTION>
    'ACRONYM' => 'C',		#          S = convert to collapsible space
    'ADDRESS' => 'C',		#          . = ignored
    'APP' => 'C',
    'APPLET' => 'C',
    'AREA' => '',
    'AU' => 'C',
    'B' => 'C $ef_B += $inc;',
    'BANNER' => 'C force_line_break',
    'BASE' => '',
    'BASEFONT' => '',
    'BDO' => 'C',
    'BGSOUND' => '',
    'BIG' => 'C',
    'BLINK' => 'C $ef_BLINK += $inc;',
    'BLOCKQUOTE' => 'C force_line_break;$ef_IDL+=3*$inc;$ef_IDR+=3*$inc;',
    'BODY' => 'C',
    'BQ' => 'E BLOCKQUOTE',	# treat <BQ> as <BLOCKQUOTE>
    'BR' => '. force_line_break;$on_list_item=0;',
    'CAPTION' => 'C force_line_break;',
    'CENTER' => 'C force_line_break;$ef_CENTRE += $inc;',
    'CITE' => 'E I',		# treat <CITE> as <I>
    'CODE' => 'C',
    'COL' => '',
    'COLGROUP' => '',
    'CREDIT' => 'C',
    'DD' => '',			# not a strict container - </DD> not required
    'DEL' => 'C',
    'DFN' => 'E B',		# treat <DFN> as <B>
    'DIR' => 'E UL',		# treat <DIR> as <UL>
    'DIV' => 'C force_line_break_weak;',
    'DL' => 'C',
    'DT' => '',			# not a strict container - </DT> not required
    'EM' => 'E I',		# treat <EM> as <I>
    'EMBED' => '',		# not strict container - </EMBED> not req'd
    'FN' => 'C',
    'FIG' => 'C',
    'FONT' => 'C',
    'FORM' => 'C',
    'FRAME' => '',
    'FRAMESET' => 'C',
    'H1' => 'C force_para_break;$ef_CENTRE+=$inc;$ef_B+=$inc;$ef_U+=$inc;',
    'H2' => 'C force_para_break;$ef_IDL-=10*$inc;$ef_B+=$inc;$ef_U+=$inc;',
    'H3' => 'C force_para_break;$ef_IDL-=2*$inc;$ef_B+=$inc;$ef_U+=$inc;',
    'H4' => 'C force_para_break;$ef_IDL-=$inc;$ef_B+=$inc;$ef_U+=$inc;',
    'H5' => 'C force_para_break;$ef_IDL-=$inc;$ef_U+=$inc;',
    'H6' => 'C force_para_break;$ef_U+=$inc;',
    'HEAD' => 'C',
    'HR' => '',
    'HTML' => 'C',
    'I' => 'C $ef_I += $inc;',
    'IMG' => '',
    'INPUT' => '',
    'INS' => 'C',
    'ISINDEX' => '',
    'KBD' => 'E CODE',		# treat <KBD> as <CODE>
    'LANG' => 'C',
    'LH' => 'C',
    'LI' => '',			# not strict container - </LI> not required
    'LINK' => '',
    'LISTING' => 'E PRE',	# treat <LISTING> as <PRE>
    'MAP' => 'C',
    'MARQUEE' => 'C force_line_break;',
    'MENU' => 'E UL',		# treat <MENU> as <UL>
    'META' => '',
    'NOBR' => 'C',
    'NOEMBED' => 'C',
    'NOFRAMES' => 'C',
    'NOTE' => 'C',
    'OL' => 'C force_para_break;$ef_IDL+=6*$inc;',
    'OPTION' => '',		# not strict container - </OPTION> not req'd
    'OVERLAY' => '',
    'P' => '',			# not strict container - </P> not required
    'PARAM' => '',
    'PERSON' => 'C',
    'PRE' => 'C',
    'Q' => 'C',
    'S' => 'C',
    'SAMP' => 'E CODE',		# treat <SAMP> as <CODE>
    'SELECT' => 'C',
    'SMALL' => 'C',
    'SPAN' => 'C',
    'STRIKE' => 'E S',		# treat <STRIKE> as <S>
    'STRONG' => 'E B',		# treat <STRONG> as <B>
    'SUB' => 'C',
    'SUP' => 'C',
    'TAB' => 'S',		# treat <TAB> as collapsible space
    'TABLE' => 'C',
    'TBODY' => '',
    'TD' => 'S',		# treat <TD> as collapsible space
    'TEXTAREA' => 'C force_line_break;',
    'TFOOT' => '',
    'TH' => 'S',		# treat <TH> as collapsible space
    'THEAD' => '',
    'TITLE' => 'C',
    'TR' => 'E BR',		# treat <TR> as <BR>
    'TT' => 'E CODE',		# treat <TT> as <CODE>
    'U' => 'C $ef_U += $inc;',
    'UL' => 'C force_para_break;$ef_IDL+=4*$inc;$depth_ul+=$inc;',
    'VAR' => 'E I',		# treat <VAR> as <I>
    'WBR' => ''
  );

  local %special = (		# &XXXX; sequences
    'AElig' => "\306",
    'Aacute' => "\301",
    'Acirc' => "\302",
    'Agrave' => "\300",
    'Aring' => "\305",
    'Atilde' => "\303",
    'Auml' => "\304",
    'Ccedil' => "\307",
    'Dstrok' => "\320",
    'ETH' => "\320",
    'Eacute' => "\311",
    'Ecirc' => "\312",
    'Egrave' => "\310",
    'Euml' => "\313",
    'Iacute' => "\315",
    'Icirc' => "\316",
    'Igrave' => "\314",
    'Iuml' => "\317",
    'Ntilde' => "\321",
    'Oacute' => "\323",
    'Ocirc' => "\324",
    'Ograve' => "\322",
    'Oslash' => "\330",
    'Otilde' => "\325",
    'Ouml' => "\326",
    'THORN' => "\336",
    'Uacute' => "\332",
    'Ucirc' => "\333",
    'Ugrave' => "\331",
    'Uuml' => "\334",
    'Yacute' => "\335",
    'aacute' => "\341",
    'acirc' => "\342",
    'acute' => "\264",
    'aelig' => "\346",
    'agrave' => "\340",
    'amp' => "\046",
    'aring' => "\345",
    'atilde' => "\343",
    'auml' => "\344",
    'brkbar' => "\246",
    'brvbar' => "\246",
    'ccedil' => "\347",
    'cedil' => "\270",
    'cent' => "\242",
    'copy' => "\251",
    'curren' => "\244",
    'deg' => "\260",
    'die' => "\250",
    'divide' => "\367",
    'eacute' => "\351",
    'ecirc' => "\352",
    'egrave' => "\350",
    'emdash' => '-',
    'emsp' => "\240",
    'endash' => '-',
    'ensp' => "\240",
    'eth' => "\360",
    'euml' => "\353",
    'frac12' => "\275",
    'frac14' => "\274",
    'frac34' => "\276",
    'gt' => "\076",
    'hibar' => "\257",
    'iacute' => "\355",
    'icirc' => "\356",
    'iexcl' => "\241",
    'igrave' => "\354",
    'iquest' => "\277",
    'iuml' => "\357",
    'laquo' => "\253",
    'lt' => "\074",
    'macr' => "\257",
    'mdash' => '-',
    'micro' => "\265",
    'middot' => "\267",
    'nbsp' => "\240",
    'ndash' => '-',
    'not' => "\254",
    'ntilde' => "\361",
    'oacute' => "\363",
    'ocirc' => "\364",
    'ograve' => "\362",
    'ordf' => "\252",
    'ordm' => "\272",
    'oslash' => "\370",
    'otilde' => "\365",
    'ouml' => "\366",
    'para' => "\266",
    'plusmn' => "\261",
    'pound' => "\243",
    'quot' => "\042",
    'raquo' => "\273",
    'reg' => "\256",
    'sect' => "\247",
    'shy' => "\007",
    'sup1' => "\271",
    'sup2' => "\262",
    'sup3' => "\263",
    'szlig' => "\337",
    'thinsp' => "\240",
    'thorn' => "\376",
    'times' => "\327",
    'trade' => '(TM)',
    'uacute' => "\372",
    'ucirc' => "\373",
    'ugrave' => "\371",
    'uml' => "\250",
    'uuml' => "\374",
    'yacute' => "\375",
    'yen' => "\245",
    'yuml' => "\377"
  );

  local @containers = ();	# stack of container tags
  local @links = ();		# list of external links
  local $paragraph = "";	# current output paragraph
  local $output = "";		# output text
  local $freshpara = 1;		# 1 if at the start of a new paragraph

  local $ef_B = 0;		# bold level
  local $ef_I = 0;		# italic level
  local $ef_U = 0;		# underline level
  local $ef_BLINK = 0;		# blinking level
  local $ef_ALIGN = "L";	# text alignment
  local $ef_CENTRE = 0;		# centre text by default

  local $depth_ul = 0;		# depth of <UL> nesting
  local @ol_count = ();		# <OL> sequence count
  local @ol_type = ();		# <OL> sequence type
  local @dl_idl = ();		# <DL> left indentation stack
  local @dl_idr = ();		# <DL> right indentation stack
  local $on_list_item = 0;	# currently on a <LI> item

  $html = "" if not defined $html;

  $html =~ s/\015//g;		# strip \r

  $line = "";

  foreach $chunk (split (/\n/, $html)) {
    $chunk =~ s/\n$//;

    if (($paragraph ne "") or ($line ne "") or (in_tag ("PRE"))) {
    					# add to previous line
      if (in_tag ("PRE")) {			# preformatted - keep break
        $line .= "<BR>";
      } else {
        $line .= " ";				# collapsible space instead
      }
    }

    $line .= $chunk;			# append new text to previous line

    while ($line =~ /</) {		# process all tags
      $tail = $';
      $text = $`;
      if ($tail =~ /^!--/) {		# SGML comment is terminated with -->
        last if ($line !~ /-->/);
        $line =~ s/<!--.*?-->//;
      } else {				# normal tag is terminated with >
        last if ($line !~ />/);
        $line =~ s/^[^<]*<([^> ]*)( [^>]*)??>//;
        $tag = uc($1);
        $attrib = $2;
        process_text ($text);		# process text before tag
        process_tag ($tag, $attrib);	# process the tag itself
      }
    }
  }

  $freshpara = 1 if ($paragraph =~ /^ *$/);
  perform_text_output ($paragraph, $on_list_item);	# flush text

  if ($#links >= 0) {			# dump link URLs
    $output .= "\n" if ($freshpara == 0);
    $foo = 1+$#links;
    $foo = "$foo";
    $n = length ($foo);
    $ef_IDR = 0;
    $ef_IDL = $n + 2;
    $ef_ALIGN = "L";
    $ef_CENTRE = 0;
    for ($i = 0; $i <= $#links; $i ++) {
      $output .= sprintf "%${n}d. ",1+$i;
      perform_text_output ($links[$i], 1);
    }
    $output .= "\n";
  }

  return $output;


  # Process a chunk of text.
  #
  sub process_text {
    my ($text) = @_;
    my ($addstart, $addend);

    if ($opt_body == 0) {
      return if (not in_tag ("BODY"));
    } else {
      return if (in_tag ("TITLE"));
    }

    $freshpara = 0 if ($text !~ /^ *$/);

    $text =~ s/&#([0-9]{1,3});/pack("C", $1)/eg;	# expand &#nnn;
    $text =~ s/&([A-Za-z]+);/$special{$1}/eg;		# expand &XXXX;

    $addstart = $addend = "";

    if ($ef_B > 0) {
      #$addstart .= "\035B";
      #$addend .= "\035b";
    }

    if (($ef_I > 0) or ($ef_U > 0)) {
      #$addstart .= "\035U";
      #$addend .= "\035u";
    }

    if ($ef_BLINK > 0) {
      #$addstart .= "\035F";
      #$addend .= "\035f";
    }

    $text =~ s/([^\s])/${addstart}$1${addend}/g;

    $paragraph .= $text;
    $paragraph =~ s/  / /g if (not in_tag ("PRE"));
  }


  # Force a line break if one hasn't already occurred.
  #
  sub force_line_break_internal($) {
    my ($repeated) = @_;
    if ($opt_body == 0) {
      return if (not in_tag ("BODY"));
    } else {
      return if (in_tag ("TITLE"));
    }
    if ((($opt_br == 0) and (not in_tag('PRE')))
        or (!$repeated)) {	                        # suppress consecutive
      return if ($paragraph eq "");			# line breaks
    }
    perform_text_output ($paragraph, $on_list_item);
    $paragraph = "";
    $freshpara = 0;
  }

  sub force_line_break {
    force_line_break_internal(1);
  }

  sub force_line_break_weak {
    force_line_break_internal(0);
  }

  # Force a paragraph break if one hasn't already occurred.
  #
  sub force_para_break {
    if ($opt_br == 0) {			# suppress consecutive paragraph breaks
      return if ($freshpara);
    }
    force_line_break;
    process_text (" ");
    force_line_break;
    $freshpara = 1;
  }


  # Process a single HTML tag.
  #
  sub process_tag {
    my ($tag, $attrib) = @_;
    my %args = ();
    my ($arg, $rest, $on, $func, $cmd, $inc);

    $on = 1;
    $on = 0 if ($tag =~ s|^/||);

    $attrib = "" if (!defined $attrib);

    if ($on == 1 && $attrib !~ /^\s+$/) {		# read attributes

      $attrib =~ s/\n/ /go;

      while ($attrib =~ /^\s*([^=\s]+)\s*=\s*(.*)$/o
               # catch attributes like ISMAP for IMG, with no arg
             || ($attrib =~ /^\s*(\S+)(.*)/o)) {
        $arg = "\U$1";
        $rest = $2;

        $novalue = 1;
        $novalue = 0 if ($attrib =~ /^\s*([^=\s]+)\s*=\s*(.*)$/o);

        if ($novalue) {
          $args{$arg} = '';
          $attrib = $rest;
        } elsif ($rest =~ /^'([^'']+)'(.*)$/o) {
          $args{$arg} = $1;
          $attrib = $2;
        } elsif ($rest =~
                      /^"([^""]*)"(.*)$/o || $rest =~ /^'([^'']*)'(.*)$/o) {
          $args{$arg} = $1;
          $attrib = $2;
        } elsif ($rest =~ /^(\S+)(.*)$/o) {
          $args{$arg} = $1;
          $attrib = $2;
        } else {
          $args{$arg} = $rest;
          $attrib = '';
        }
      }
    }

    $cmd = '';

  CLUNKYJUMP:
    if (defined $taglist{$tag}) {
      if ($taglist{$tag} =~ /^(\S+)\s*(.*)$/) {
        $arg = $1;
        $rest = $2;
        if ($arg eq "E") {		# tag equivalent to another tag
          $tag = $rest;
          goto CLUNKYJUMP;
        } elsif ($arg eq "C") {		# tag is a proper container
          tag__container ($tag, $on);
        } elsif ($arg eq "S") {		# tag should be a collapsible space
          process_text (" ");
        }
        $cmd = $rest;
      }
    }

    $func = "tag_${tag}";
    $inc = ($on == 1) ? 1 : -1;

    &${func}($tag, $on, %args) if defined &${func};	# run subroutine
    eval "$cmd" if ($cmd ne '');			# evaluate code
  }


  # Add or remove a container tag to/from the stack of active containers.
  #
  sub tag__container {
    my ($tag, $on) = @_;
    my ($i, $j);
    if ($on) {			# push tag on to container stack
      push (@containers, $tag);
    } else {			# pop topmost $tag from container stack
      for ($i = $#containers; $i >= 0; $i --) {
        if ($containers[$i] eq $tag) {
          for ($j = $i; $j < $#containers; $j ++) {
            $containers[$j] = $containers[$j+1];
          }
          pop (@containers);
          return;
        }
      }
    }
  }


  # Return TRUE if tag $_ is on the container stack.
  #
  sub in_tag {
    my ($tag) = @_;
    my $i;
    for ($i = 0; $i <= $#containers; $i ++) {
      return 1 if ($containers[$i] eq $tag);
    }
    return 0;
  }


  # Output a paragraph of text, indenting as necessary, not adding indent
  # spaces (but accounting for them) on the first line if the second parameter
  # is nonzero.
  #
  # NB no indenting is done if we're inside a <PRE> block.
  #
  sub perform_text_output {
    my ($txt, $noind) = @_;
    my ($lefti, $ptxt, $plen, $maxlen, $lineone);
    my ($tmp, $foo, $bar, $baz);
    my @words;

    if (in_tag ("PRE")) {
      $txt =~ s/\035b\035u\035f\035B\035U\035F//g;
      $txt =~ s/\035b\035u\035B\035U//g;
      $txt =~ s/\035b\035f\035B\035F//g;
      $txt =~ s/\035u\035f\035U\035F//g;
      $txt =~ s/\035b\035B//g;
      $txt =~ s/\035u\035U//g;
      $txt =~ s/\035f\035F//g;
      $output .= "$txt\n";
      return;
    }

    if ($ef_IDL < 1) {
      $lefti = "";
    } else {
      $lefti = " " x $ef_IDL;
    }

    $maxlen = $ef_WIDTH - ($ef_IDL<0?0:$ef_IDL) - ($ef_IDR<0 ? 0:$ef_IDR) - 1;
    $lineone = 1;

    do {
      $ptxt = $txt;
      $ptxt =~ s/\035.//g;
      $plen = length $ptxt;
      if ($plen < $maxlen) {			# no line splitting needed
        $ptxt = $txt;
      } else {					# split line
        $ptxt = substr ($ptxt, 0, $maxlen - 1);
        if ($ptxt !~ / /) {			# no words - split
          $txt =~ s/\035.//g;			# FIXME: loses attributes
          $ptxt = substr ($txt, 0, $maxlen - 1);
          $txt = substr ($txt, $maxlen - 1);
        } else {				# wrap on a word
          $ptxt = "";
          $bar = 1;
          do {
            $foo = $ptxt;
            $foo =~ s/\035.//g;
            $baz = length $foo;
            $tmp = $txt;
            if (($baz < $maxlen) and ($txt =~ s/^ *([^ ]+) *//)) {
              $foo = $1;
              $bar = $foo;
              $bar =~ s/\035.//g;
              if (($baz + length $bar) < $maxlen) {
                $ptxt .= " " if ($ptxt ne "");
                $ptxt .= $foo;
                $bar = 1;
              } else {
                $txt = $tmp;
                $bar = 0;
              }
            } else {
              $txt = $tmp;
              $bar = 0;
            }
          } while (($baz < $maxlen) && ($bar == 1));
        }
      }
      if (($lineone == 0) or ($noind == 0)) {	# align
        if ($ef_CENTRE or ($ef_ALIGN eq "C")) {	# centre
          $foo = $ptxt;
          $foo =~ s/\035.//g;
          $foo = length $foo;
          $tmp = $ef_WIDTH - ($ef_IDL<0 ? 0:$ef_IDL) - ($ef_IDR<0 ? 0:$ef_IDR);
          $tmp = ($tmp - $foo) / 2;
          $ptxt = $lefti . $ptxt;
          if ($tmp > 0) {
            $foo = " " x $tmp;
            $ptxt = $foo . $ptxt;
          }
        } elsif ($ef_ALIGN eq "R") {		# right
          $foo = $ptxt;
          $foo =~ s/\035.//g;
          $foo = length $foo;
          $tmp = $ef_WIDTH - ($ef_IDL<0 ? 0:$ef_IDL) - ($ef_IDR<0 ? 0:$ef_IDR);
          $tmp -= $foo;
          $ptxt = $lefti . $ptxt;
          if ($tmp > 0) {
            $foo = " " x $tmp;
            $ptxt = $foo . $ptxt;
          }
        } else {				# left (FIXME: JUSTIFY = LEFT)
          $ptxt = $lefti . $ptxt;
        }
      }
      $ptxt =~ s/\035b\035u\035f\035B\035U\035F//g;
      $ptxt =~ s/\035b\035u\035B\035U//g;
      $ptxt =~ s/\035b\035f\035B\035F//g;
      $ptxt =~ s/\035u\035f\035U\035F//g;
      $ptxt =~ s/\035b\035B//g;
      $ptxt =~ s/\035u\035U//g;
      $ptxt =~ s/\035f\035F//g;
      $ptxt =~ s/\s*$//;
      $output .= "$ptxt\n";
      $lineone = 0;
    } while ($plen > $maxlen);
  }


  # Deal with <A> tag.
  #
  sub tag_A {
    my ($tag, $on, %args) = @_;
    my $num;

    if ($opt_body == 0) {
      return if (not in_tag ("BODY"));
    }

    return if ($opt_nl);
    return if (not defined $args{'HREF'});
    return if (not $opt_al and $args{'HREF'} !~ /^\S+:/);

    $num = 2 + $#links;

    process_text ("{$num}");
    push (@links, $args{'HREF'});
  }


  # Deal with <P> tag.
  #
  sub tag_P {
    my ($tag, $on, %args) = @_;

    force_para_break;
    $on_list_item = 0;

    $ef_ALIGN = "L";
    return if (not defined ($args{'ALIGN'}));

    if ($args{'ALIGN'} =~ /CENTER/i) {
      $ef_ALIGN = "C";
    } elsif ($args{'ALIGN'} =~ /LEFT/i) {
      $ef_ALIGN = "L";
    } elsif ($args{'ALIGN'} =~ /RIGHT/i) {
      $ef_ALIGN = "R";
    } elsif ($args{'ALIGN'} eq /JUSTIFY/i) {
      $ef_ALIGN = "J";
    }
  }


  # Deal with <OL> tag.
  #
  sub tag_OL {
    my ($tag, $on, %args) = @_;
    my ($t, $n);
    force_para_break;
    if ($on) {					# opening
      $n = 1;						# default start: 1
      $t = "1";						# default type: "1"
      $n = $args{'SEQNUM'} if (defined $args{'SEQNUM'});
      $n = $args{'START'} if (defined $args{'START'});
      $t = $args{'TYPE'} if (defined $args{'TYPE'});
      push (@ol_count, $n);
      push (@ol_type, $t);
    } else {					# closing
      pop @ol_count;
      pop @ol_type;
      $on_list_item = 0 if (not in_tag ('OL') and not in_tag ('UL'));
    }
  }


  # Deal with <UL> tag.
  #
  sub tag_UL {
    my ($tag, $on, %args) = @_;
    force_para_break;
    if (not $on) {
      $on_list_item = 0 if (not in_tag ('OL') and not in_tag ('UL'));
    }
  }


  # Deal with <LI> tag.
  #
  sub tag_LI {
    my ($tag, $on, %args) = @_;
    my ($n, $x, $t, $lefti, $num, $i);
    if (not $on) {				# </LI>
      $on_list_item = 0;
      return;
    }
    for ($i = $#containers; $i >= 0; $i --) {
      $tag = $containers[$i];

      if ($tag eq "OL") {			# <OL> list item
        $on_list_item = 1;
        force_line_break;
        $n = $ef_IDL - 5;
        if ($n < 1) {
          $lefti = "";
        } else {
          $lefti = " " x $n;
        }
        $output .= $lefti;
        $n = pop @ol_count;
        $x = $n;
        $t = pop @ol_type;
        $num = "$n";
        if ($t eq "1") {
          $num = "$n";
        } elsif ($t eq "A") {
          $num = "";
          do {
            $n --;
            $num = substr ("ABCDEFGHIJKLMNOPQRSTUVWXYZ", $n % 26, 1) . $num;
            $n = int ($n / 26);
          } while ($n > 0);
        } elsif ($t eq "a") {
          $num = "";
          do {
            $n --;
            $num = substr ("abcdefghijklmnopqrstuvwxyz", $n % 26, 1) . $num;
            $n = int ($n / 26);
          } while ($n > 0);
        } elsif ($t eq 'I') {			# FIXME: roman numeral support
        } elsif ($t eq 'i') {			# FIXME: roman numeral support
        }
        $output .= sprintf "%3s. ",$num;
        $x ++;
        push (@ol_count, $x);
        push (@ol_type, $t);
        return;

      } elsif ($tag eq "UL") {			# <UL> list item
        $on_list_item = 1;
        force_line_break;
        $n = $ef_IDL - 2;
        if ($n < 1) {
          $lefti = "";
        } else {
          $lefti = " " x $n;
        }
        $output .= $lefti;
        $output .= substr (">*+o-#\@=", $depth_ul % 8, 1) . " ";
        return;
      }
    }
  }


  # Deal with <IMG> tag.
  #
  sub tag_IMG {
    my ($tag, $on, %args) = @_;
    return if (not defined $args{'ALT'});
    return if ($args{'ALT'} eq "");
    process_text ($args{'ALT'});
  }


  # Deal with <DL> tag.
  #
  sub tag_DL {
    my ($tag, $on, %args) = @_;
    if ($on) {
      $ef_ALIGN = "L";
      push (@dl_idl, $ef_IDL);
      push (@dl_idr, $ef_IDR);
      force_para_break;
    } else {
      force_para_break;
      $ef_IDL = pop @dl_idl;
      $ef_IDR = pop @dl_idr;
    }
  }


  # Deal with <DT> tag.
  #
  sub tag_DT {
    my ($tag, $on, %args) = @_;
    force_line_break;
    return if (not $on);
    $ef_IDL = pop @dl_idl;
    $ef_IDR = pop @dl_idr;
    push (@dl_idl, $ef_IDL);
    push (@dl_idr, $ef_IDR);
  }


  # Deal with <DD> tag.
  #
  sub tag_DD {
    my ($tag, $on, %args) = @_;
    force_line_break;
    return if (not $on);
    $ef_IDL = pop @dl_idl;
    $ef_IDR = pop @dl_idr;
    push (@dl_idl, $ef_IDL);
    push (@dl_idr, $ef_IDR);
    $ef_IDL += 7;
  }


  # Deal with <HR> tag.
  #
  sub tag_HR {
    my ($tag, $on, %args) = @_;
    my ($linewidth, $oc, $oa, $line, $maxlen);

    force_line_break;

    $on_list_item = 0;
    $oc = $ef_CENTRE;
    $oa = $ef_ALIGN;
    $ef_CENTRE = 0;
    $ef_ALIGN = "C";

    $linewidth = 100;
    if (defined $args{'WIDTH'}) {
      $linewidth = $args{'WIDTH'};
      $linewidth =~ s/[^0-9]//g;
    }
    if (defined ($args{'ALIGN'})) {
      if ($args{'ALIGN'} =~ /CENTER/i) {
        $ef_ALIGN = "C";
      } elsif ($args{'ALIGN'} =~ /LEFT/i) {
        $ef_ALIGN = "L";
      } elsif ($args{'ALIGN'} =~ /RIGHT/i) {
        $ef_ALIGN = "R";
      } elsif ($args{'ALIGN'} eq /JUSTIFY/i) {
        $ef_ALIGN = "J";
      }
    }

    $maxlen = $ef_WIDTH - ($ef_IDL<0?0:$ef_IDL) - ($ef_IDR<0 ? 0:$ef_IDR) - 1;
    $linewidth = $linewidth * $maxlen / 100;
    $linewidth = $maxlen if ($linewidth > $maxlen);
    $line = "-" x $linewidth;
    process_text ($line);
    force_line_break;
    $ef_CENTRE = $oc;
    $ef_ALIGN = $oa;
  }
}

# EOF
