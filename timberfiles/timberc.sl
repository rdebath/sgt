% timberh.sl - standalone mail-buffer highlighting for Timber

% Should highlight correct sig separators and quoted text. And "^From ",
% just to make it unlikely that I'll accidentally write a message that
% does it.

$1 = "TimberC";
create_syntax_table ($1);
set_syntax_flags ($1, 0);
#ifdef HAS_DFA_SYNTAX
#ifexists dfa_define_highlight_rule
dfa_enable_highlight_cache("timberc.dfa", $1);
dfa_define_highlight_rule("^-- $", "keyword", $1);
dfa_define_highlight_rule("^From ", "Qkeyword", $1);
dfa_define_highlight_rule("^> *> *>.*$", "preprocess", $1);
dfa_define_highlight_rule("^> *>.*$", "comment", $1);
dfa_define_highlight_rule("^>.*$", "string", $1);
dfa_define_highlight_rule(".*", "normal", $1);
dfa_build_highlight_table($1);
#else
enable_highlight_cache("timberc.dfa", $1);
define_highlight_rule("^-- $", "keyword", $1);
define_highlight_rule("^From ", "Qkeyword", $1);
define_highlight_rule("^> *> *>.*$", "preprocess", $1);
define_highlight_rule("^> *>.*$", "comment", $1);
define_highlight_rule("^>.*$", "string", $1);
define_highlight_rule(".*", "normal", $1);
build_highlight_table($1);
#endif
#endif
