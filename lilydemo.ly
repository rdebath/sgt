% Small demonstration Lilypond file which shows off some glyphs
% which differ noticeably between Lilypond's default font and
% Gonville.

\version "2.10.33"

\layout {
  ragged-right = ##t
  tagline = ##f
}

\book {
  \score {
    \new ChoirStaff <<
      \time 4/4
      \new Staff {
        \clef alto
        r1
        \time 3/4
        f2^\fermata
        f'4^\trill
        \time 15/64
        g'8\noBeam
        ais'16\noBeam
        b'32\noBeam
        c''64\noBeam
      }
      \new GrandStaff <<
        \new Staff {
          \clef treble
          g'1
          r2^\fermata
          d''4
          r8
          r16
          r32
          r64
        }
        \new Staff {
          \clef bass
          c1
          c2_\fermata
          r4
          ges,8\noBeam
          f,16\noBeam
          e,32\noBeam
          g,64\noBeam
        }
      >>
    >>
    \layout {}
  }
}
