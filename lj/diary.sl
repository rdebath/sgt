% Mode for editing my diary markup files.

$1 = "Diary";
create_syntax_table ($1);
set_syntax_flags ($1, 0);
#ifdef HAS_DFA_SYNTAX
#ifexists dfa_define_highlight_rule
dfa_enable_highlight_cache("diary.dfa", $1);
dfa_define_highlight_rule("^.*`.*$", "error", $1);
dfa_define_highlight_rule("^.* _.*$", "error", $1);
dfa_define_highlight_rule("^.*_ .*$", "error", $1);
dfa_define_highlight_rule("^-*-.*$", "comment", $1);
dfa_define_highlight_rule("{[^ }]*", "keyword", $1);
dfa_define_highlight_rule("}", "keyword", $1);
dfa_define_highlight_rule("[^{}]*", "normal", $1);
dfa_build_highlight_table($1);
#else
enable_highlight_cache("diary.dfa", $1);
define_highlight_rule("^.*`.*$", "error", $1);
define_highlight_rule("^.* _.*$", "error", $1);
define_highlight_rule("^.*_ .*$", "error", $1);
define_highlight_rule("^-*-.*$", "comment", $1);
define_highlight_rule("{[^ }]*", "keyword", $1);
define_highlight_rule("}", "keyword", $1);
define_highlight_rule("[^{}]*", "normal", $1);
build_highlight_table($1);
#endif
#endif

define diary_mode() {
    variable mode = "Diary";
    % use_keymap (mode);
    set_mode (mode, 0x1 | 0x20);
    use_syntax_table (mode);
#ifexists use_dfa_syntax
    use_dfa_syntax(1);
#endif
    runhooks ("diary_mode_hook");
}
