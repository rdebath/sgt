#!/usr/bin/perl -n

# Simple Morse decoder. Expects standard input to contain
# whitespace-separated strings of . and - characters, and outputs
# the decoded message on standard output. Inter-word spacing is
# denoted by a / character in the input.

BEGIN {
    %z = (
          ".-" => "A",
          "-..." => "B",
          "-.-." => "C",
          "-.." => "D",
          "." => "E",
          "..-." => "F",
          "--." => "G",
          "...." => "H",
          ".." => "I",
          ".---" => "J",
          "-.-" => "K",
          ".-.." => "L",
          "--" => "M",
          "-." => "N",
          "---" => "O",
          ".--." => "P",
          "--.-" => "Q",
          ".-." => "R",
          "..." => "S",
          "-" => "T",
          "..-" => "U",
          "...-" => "V",
          ".--" => "W",
          "-..-" => "X",
          "-.--" => "Y",
          "--.." => "Z",
          "-----" => "0",
          ".----" => "1",
          "..---" => "2",
          "...--" => "3",
          "....-" => "4",
          "....." => "5",
          "-...." => "6",
          "--..." => "7",
          "---.." => "8",
          "----." => "9",
          ".-.-.-" => ".",
          "--..--" => ",",
          "..--.." => "?",
          "/" => " ",
          );
}

split; foreach $i (@_) { $k = $z{$i}; print $k if defined $k }
