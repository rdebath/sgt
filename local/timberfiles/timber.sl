% timber.sl    -*- mode: slang; mode: fold -*-
%
% a mail user agent embedded in jed

% A buffer whose title begins with `[T]' will be a Timber buffer. Such
% a buffer is subject to its own set of key definitions and its own
% rules of line hiding/showing. It contains an entire mail folder, in
% a processed form from which the original form may be reconstructed.

%{{{ TODO list

% fix: hitting backspace on last (blank) line of mime message will never
%   fold message however often you do it
% some form of address book
% Give forwarding some serious testing, interoperating it with all
%   mailers I can find (mutt; pine; vm; Eudora; netscrapemail; others?)
% get the message size counting right (exclude From line? CRLF?)
%   message size counting is also not corrected after mimedestroy
% maybe postpone a composed message
% maybe fcc
% bulk-select of messages, perhaps
% recode buffer translations in C for speed
% produce a buffer-modified tag to avoid forcing save on Q
% put in the promised `Press ? for help'
% better mailbox locking: option to open a locked box readonly
% better mailbox locking: research atomicity under NFS
% better mailbox locking: let user specify something about how it's done

%}}}
%{{{ MIME nested multiparts: design thoughts

% We need:
%  - a mailbuffer syntax for nesting multiparts
%    (this is easy; M lines and ! lines must just carry a depth indicator)
%  - everything to support it! Particular items follow.
%  - timber_enbuf() wants to create nested M and ! lines
%    (wait until it's redone in Perl probably)
%    (!! timber_enbuf() may also want to decode QP/base64'ed message/rfc822
%    parts so as to be able to create nested M! lines for the interiors)
%  - timber_unfold() wants to be useful about unfolding a fold containing
%    other folds
%  - timber_fold() wants to be sensible about how to find the `next level up'
%    to fold
%  - timber_mimedestroy() is going to have one HELL of a time working on
%    attachments to a forwarded message/rfc822, although it should be OK on
%    simple nested multiparts
%  - timber_expunge() wants to be sane about which bits of the world to keep
%    when it saves back out. (Redo this in Perl and we can ensure it continues
%    to be inverse to enbuf()...)

% Pre-nested-multipart mailbuffer syntax was:
%
% buffer:
%  - Buffer begins with an F line
%  - Each message begins with a * line
%  - Buffer ends with a * [end] line.
%
% non-multipart-message:
%  - After the * line come the headers, on | lines
%  - Then an optional decoded message body on nbsp lines
%  - Then the original message body on space lines
%
% multipart-message:
%  - After the * line come the headers, on | lines
%  - Then a blank space line
%  - Then an M line (well, an M0 line) followed by some + lines of
%    intro crud
%  - Each attachment begins with a ! line (well, a !0 line)
%  - Finally, a ! [end] line (well, !0 [end]) followed by some + lines
%    of outro crud
%  - Then a blank space line
%
% attachment:
%  - After the ! line come the MIME headers on + lines
%  - Then an optional decoded attachment body on nbsp lines
%  - Then the original attachment body on space lines
%
% This has the pleasant property that the unprocessed form of the mail
% buffer is easily recovered by taking all {|,+,space} lines and stripping
% the first character. All other lines are Timber markers {F,*,M,!} and
% decoded attachments (nbsp).

% A nested-multipart mailbuffer syntax clearly cannot have such a pleasant
% property if we wish to be able to use space lines for un-decoded bodies
% of attachments to a decoded message/rfc822 attachment, and _certainly_
% can't if we want to be able to do mimedestroy on attachments to
% attachments. So we must resign ourselves to having a much less pleasant
% syntax.

% So let's look at the possibilities.
%
% The `buffer' and `non-multipart-message' syntaxes are fine as they are.
% The `multipart-message' syntax is fine as well provided we incorporate
% level-zero indicators in the M line and the ! and ! [end] lines.
%
% The `attachment' bit is the fun bit. The syntax above is fine for a
% leaf-attachment, but multipart-attachments must now come in two much
% more entertaining forms: multipart-attachment and
% rfc822-multipart-attachment.
%
% After the initial ![n] line must still come the MIME headers. Then an
% optional text section (on space or nbsp lines; I'm not sure it matters)
% containing the header section of an rfc822 message (empty in a simple
% nested multipart). Then an M[n+1] line and some attachments, ending up
% with an ![n+1][end] and its potential + lines.
%
% This is the `obvious' layout. What does it imply for enbuf and expunge?
%
% For nested multiparts which are not specially encoded, all this is very
% easy: enbuf must recurse a bit but that's not too bad, and expunge must
% still do the simple thing of keeping {|,+,space} lines and throwing out
% the rest.
%
% The tricky bit happens when we have an encoded nested multipart. (I haven't
% seen this happen in simple nested multiparts, only in message/rfc822
% multiparts, but that's no excuse not to allow for both cases.) In this case,
% enbuf must decode the multipart and then do its normal recursion, having
% left an `encode this again when you clean up' marker for expunge. expunge
% must do the normal clearup and then re-encode the multipart. So the big
% question is, what should the magic marker be?
%
% Answer: it should be nothing we've yet encountered. It must be within
% the attachment space, so that mimedestroy on the whole attachment can
% correctly trash the whole thing and not leave the marker around. It must
% have a _new_ LHS character. Something like `[', perhaps.
%
%   ![1] message/rfc822
%   [ base64 decoded automatically ]
%   +Content-Type: ...
%
% Then the requirements for getting all of this right are fairly simple.
% We need to introduce utility functions which will not only spot M and !
% lines but also tell us their level; we need to update routines like
% fold and unfold so that they ignore lines of level lower than the level
% they're actually dealing with. We need to ensure mimedestroy works on
% both inner and outer attachments. (We need to look out for Content-Length
% headers when we re-encode!)

%}}}
%{{{ User-configurable variables

% This variable may be reassigned in `.timberrc'. It gives
% the name of the special mail folder where new unread mail is
% transferred on Timber initialisation.
variable timber_inbox = "mbox";

% This variable may be reassigned in `.timberrc'. It gives
% the pathname of the directory where Timber mail folders are kept.
variable timber_folders = "~/mail";

% This variable may be reassigned in `.timberrc'. It gives
% the pathname of the file containing the desired signature file.
variable timber_sig = "~/.signature";

% This variable may be reassigned in `.timberrc'. It gives
% the pathname of a mime.types file.
variable timber_mimetypes = "/etc/mime.types";

% This variable may be reassigned in `.timberrc'. It gives
% the text to be prepended to quoted messages in replies.
variable timber_quote_str = "> ";

% This variable must be reassigned in `.timberrc' if needed. It
% gives the address to Bcc messages to in `timber_bcc_self'.
variable timber_bcc_addr = "";

% This variable gets inserted at the top of any message you
% start to compose. It may be reassigned in `.timberrc'. Remember
% the \n on the end of each header!
variable timber_custom_headers = "";

% This variable may be reassigned in `.timberrc'. It provides the
% location of the Timber scripts directory.
variable timber_scripts_dir = dircat(getenv("HOME"), "lib/timber");

% This variable may, and almost certainly should, be reassigned
% in `.timberrc'. It gives a comma-separated list of e-mail
% addresses that Timber views as belonging to the user (which will
% therefore be treated differently when seen in To: and From: fields
% of messages that `Reply' is invoked on).
%
% FIXME: how do we deal with sgt-*@arachsys.com being valid for any
% value of *?
%
% FIXME: we should invent a sane value of this for those without
% a .timberrc.
variable timber_addresses = "";

% This variable may be reassigned in `.timberrc'. It gives a colon-
% separated list of header keywords that are considered `boring' and
% left hidden in an unfolded message until `h' is pressed. It must
% begin and end with a colon. The special separator "From " can be
% given by "...:From :...".
variable timber_boringhdrs =
":From :Return-path:Envelope-to:Delivery-date:Received:X-Priority:" +
":X-MSMail-Priority:X-Mailer:MIME-Version:Content-Type:In-Reply-To:" +
":Message-Id:X-UIDL:Status:Return-Path:X-ESMTP-Daemon:X-PMFLAGS:" +
":X-Attachments:X-Status:Sender:Organization:Comments:X-Sender:" +
":References:Priority:X-MailServer:X-Article:Content-Length:X-Loop:" +
":Content-Id:Posted-Date:Received-Date:Lines:X-Newsreader:Precedence:" +
":Errors-To:X-X-Sender:X-URL:X-Juno-Line-Breaks:X-Confirm-Reading-To:" +
":X-pmrqc:Return-receipt-to:X-Newsgroups:X-Followup-To:Keywords:Summary:" +
":X-Automatically-Sent-By:X-MIME-Autoconverted:X-MimeOLE:X-Envelope-To:" +
":X-Originating-IP:X-VM-VHeader:X-VM-Last-Modified:X-VM-IMAP-Retrieved:" +
":X-VM-POP-Retrieved:X-VM-Bookmark:X-VM-v5-Data:X-VM-Labels:" +
":X-VM-Summary-Format:X-Lotus-FromDomain:Content-Disposition:" +
":X-MIMETrack:Content-Class:Thread-Topic:Thread-Index:List-Id:" +
":Content-Transfer-Encoding:X-MS-Has-Attach:X-MS-TNEF-Correlator" +
":X-Mailman-Version:X-BeenThere:";

%}}}
%{{{ Non-user-configurable variables

% The version number of Timber.
variable timber_version = "v0.2";

% This is used in a couple of places so it's kept here for reusability.
variable timber_headerline =
"Flags Lin  Size  Date  From                 Subject\n";

% These variables get set up again, properly, at the end of this file.
variable timber_sendmail = "notsetupyet";
variable timber_fetch_prog = "notsetupyet";
variable timber_enbuf_prog = "notsetupyet";
variable timber_mime_prog = "notsetupyet";
variable timber_prefix_prog = "notsetupyet";
variable timber_mimetype_prog = "notsetupyet";

%}}}

%{{{ Buffer-tracking mechanism for `timber_only'

% This is used to determine whether Timber was invoked using `timber_only',
% in which case closing the last Timber buffer should trigger exit of Jed.
% We also count the Timber buffers here.
variable timber_is_only = 0;
variable timber_buffers = 0;

% Increment timber_buffers
define timber_buffers_more() {
    timber_buffers ++;
}

% Decrement timber_buffers, and quit if necessary
define timber_buffers_fewer() {
    timber_buffers --;
    if (timber_buffers <= 0 and timber_is_only)
	exit_jed();
}

%}}}

%{{{ The keymap for a Timber buffer

$1 = "Timber";
!if (keymap_p($1)) {
    make_keymap($1);

    % Undefine stuff which might accidentally allow buffer modification.
    % We don't guard against _deliberate_ buffer modification.
    for ($2 = 32; $2 < 127; $2++) {
	definekey("timber_undefined", char($2), $1);
    }
    definekey("timber_undefined", "^D", $1);
    definekey("timber_undefined", "^I", $1);
    definekey("timber_undefined", "^K", $1);
    definekey("timber_undefined", "^O", $1);
    definekey("timber_undefined", "^Q", $1);
    definekey("timber_undefined", "^T", $1);
    definekey("timber_undefined", "^W", $1);
    definekey("timber_undefined", "^Y", $1);

    % Return unfolds a message, _or_ moves to and unfolds the first
    % unread message if it's pressed at the top of the buffer. ^C^S
    % just unfolds a message.
    definekey("timber_press_ret", "^J", $1);
    definekey("timber_press_ret", "^M", $1);
    definekey("timber_unfold", "^C^S", $1);

    % Space pages down; Minus and B page up.
    definekey("page_down", " ", $1);
    definekey("page_up", "-", $1);
    definekey("page_up", "B", $1);
    definekey("page_up", "b", $1);

    % Backspace folds a message. So does ^C^X.
    % definekey("timber_fold", "^H", $1); (^H doesn't 'cos it's Help)
    definekey("timber_fold", "^?", $1);
    definekey("timber_fold", "^C^X", $1);

    % N (next) and P (prev) fold the current message and unfold the
    % next or previous in line.
    definekey("timber_nextmsg", "N", $1);
    definekey("timber_nextmsg", "n", $1);
    definekey("timber_prevmsg", "P", $1);
    definekey("timber_prevmsg", "p", $1);

    % I (index) folds all messages.
    definekey("timber_foldall", "I", $1);
    definekey("timber_foldall", "i", $1);

    % D (delete) and U (undelete) play with the status flags on the
    % message summary lines.
    definekey("timber_delete", "D", $1);
    definekey("timber_delete", "d", $1);
    definekey("timber_undelete", "U", $1);
    definekey("timber_undelete", "u", $1);

    % X (expunge) removes all messages with a D flag in their summary
    % lines, and also checkpoints the folder to disk.
    definekey("timber_expunge", "X", $1);
    definekey("timber_expunge", "x", $1);

    % Q (quit / qlosefolder) does an expunge and then kills the buffer.
    definekey("timber_qlose", "Q", $1);
    definekey("timber_qlose", "q", $1);

    % C (compose) opens a composer window, in which ^X^S sends the
    % message. R (reply) and F (forward) do the same thing, but include
    % the current message, in different ways, into the initial contents
    % of the composer window.
    %
    % R and r are distinct: R sends a cc to all recipients who aren't
    % ourself, whereas r sends the message _only_ to the sender.
    definekey("timber_compose", "C", $1);
    definekey("timber_compose", "c", $1);
    definekey("timber_reply_all", "R", $1);
    definekey("timber_reply", "r", $1);
    definekey("timber_forward", "F", $1);
    definekey("timber_forward", "f", $1);

    % G (goto) opens another Timber buffer on a different folder. V
    % (moVe - more usefully, it's by analogy with ^X^V) replaces the
    % current Timber buffer with one on a different folder.
    definekey("timber_goto", "G", $1);
    definekey("timber_goto", "g", $1);
    definekey("timber_moveto", "V", $1);
    definekey("timber_moveto", "v", $1);

    % S (save) and E (export) append the current message to the end of
    % a given file. `save' assumes the file is a folder, whereas `export'
    % doesn't.
    definekey("timber_save", "S", $1);
    definekey("timber_save", "s", $1);
    definekey("timber_export", "E", $1);
    definekey("timber_export", "e", $1);

    % H (headers) shows all headers in the current message, rather than
    % hiding the boring ones.
    definekey("timber_fullhdr", "H", $1);
    definekey("timber_fullhdr", "h", $1);

    % MH (mime-headers) shows all the headers of a MIME part.
    definekey("timber_fullmime", "MH", $1);
    definekey("timber_fullmime", "Mh", $1);
    definekey("timber_fullmime", "mH", $1);
    definekey("timber_fullmime", "mh", $1);

    % MD (mime-decode) pipes a MIME part through a decoding program.
    definekey("timber_mimedec", "MD", $1);
    definekey("timber_mimedec", "Md", $1);
    definekey("timber_mimedec", "mD", $1);
    definekey("timber_mimedec", "md", $1);

    % MR (mime-revert) removes any decoded variant of a MIME part.
    definekey("timber_mimerev", "MR", $1);
    definekey("timber_mimerev", "Mr", $1);
    definekey("timber_mimerev", "mR", $1);
    definekey("timber_mimerev", "mr", $1);

    % M^D (mime-destroy) replaces a MIME part with a text/x-rcpt-removed
    % part giving the previous set of MIME headers, so that you can save
    % a message without its attachment. This is VERY DANGEROUS, so we also
    % define a Timber undo function on ^Xu and ^_.
    definekey("timber_mimedestroy", "M^D", $1);
    definekey("timber_mimedestroy", "m^D", $1);
    definekey("timber_undo", "^Xu", $1);
    definekey("timber_undo", "^XU", $1);
    definekey("timber_undo", "^_", $1);

    % A (attachment) saves a MIME part to a file, decoding if required.
    definekey("timber_saveattach", "A", $1);
    definekey("timber_saveattach", "a", $1);

    % M-r (for replying) selects the current MIME part.
    definekey("timber_selattach", "^[R", $1);
    definekey("timber_selattach", "^[r", $1);

    % M-up and M-down move the current message about.
    definekey("timber_movemsgup", "^[^[[A", $1);
    definekey("timber_movemsgdown", "^[^[[B", $1);
    definekey("timber_movemsgup", "^[^[OA", $1);
    definekey("timber_movemsgdown", "^[^[OB", $1);

    % We'll make / a search function, just because it is in so many
    % readonly text viewing applications.
    definekey("search_forward", "/", $1);

    % ^X c counts the messages in the folder.
    definekey("timber_count", "^XC", $1);
    definekey("timber_count", "^Xc", $1);
}

%}}}
%{{{ The highlighting mode for Timber
create_syntax_table ($1);
set_syntax_flags ($1, 0);
#ifdef HAS_DFA_SYNTAX
#ifexists dfa_define_highlight_rule
dfa_enable_highlight_cache("timber.dfa", $1);
dfa_define_highlight_rule("^[F:].*$", "comment", $1);
dfa_define_highlight_rule("^\\|[^: \t]*[: ]", "Qkeyword", $1);
dfa_define_highlight_rule("^\\+.*$", "preprocess", $1);
dfa_define_highlight_rule("^[ \240]-- $", "keyword", $1);
dfa_define_highlight_rule("^[ \240]> *> *>.*$", "preprocess", $1);
dfa_define_highlight_rule("^[ \240]> *>.*$", "comment", $1);
dfa_define_highlight_rule("^[ \240]>.*$", "string", $1);
dfa_define_highlight_rule("^[ \240].*$", "normal", $1);
dfa_define_highlight_rule("^\\* ..D.*$", "dollar", $1);
dfa_define_highlight_rule("^\\! +[^\\[ ].*$", "delimiter", $1);
dfa_define_highlight_rule("^\\* [^\\[].*$", "string", $1);
dfa_define_highlight_rule("^[\\*\\!] +\\[end\\]$", "comment", $1);
dfa_define_highlight_rule(".*$", "normal", $1);
dfa_build_highlight_table($1);
#else
enable_highlight_cache("timber.dfa", $1);
define_highlight_rule("^[F:].*$", "comment", $1);
define_highlight_rule("^\\|[^: \t]*[: ]", "Qkeyword", $1);
define_highlight_rule("^\\+.*$", "preprocess", $1);
define_highlight_rule("^[ \240]-- $", "keyword", $1);
define_highlight_rule("^[ \240]> *> *>.*$", "preprocess", $1);
define_highlight_rule("^[ \240]> *>.*$", "comment", $1);
define_highlight_rule("^[ \240]>.*$", "string", $1);
define_highlight_rule("^[ \240].*$", "normal", $1);
define_highlight_rule("^\\* ..D.*$", "dollar", $1);
define_highlight_rule("^\\! +[^\\[ ].*$", "delimiter", $1);
define_highlight_rule("^\\* [^\\[].*$", "string", $1);
define_highlight_rule("^[\\*\\!] +\\[end\\]$", "comment", $1);
define_highlight_rule(".*$", "normal", $1);
build_highlight_table($1);
#endif
#endif
%}}}
%{{{ The keymap for a Timber composer buffer

$1 = "TimberC";
!if (keymap_p($1)) {
    make_keymap($1);
    definekey("timber_sendmsg", "^X^S", $1);
    definekey("timber_killmsg", "^Xk", $1);
    definekey("timber_killmsg", "^XK", $1);
    definekey("timber_attach", "^Xa", $1);
    definekey("timber_attach", "^XA", $1);
    definekey("timber_bcc_self", "^Os", $1);
    definekey("timber_bcc_self", "^OS", $1);
}
%}}}
%{{{ The highlighting mode for Timber composers
#ifndef TIMBER_TIMBERC_LOADED
() = evalfile("timberc.sl");
#endif
%}}}
%{{{ timber_exitfunc: protect against exiting Jed with active Timber buffers

% Timber expects a special kind of exit hook to be defined. Simon
% Tatham's personal .jedrc defines this, but Timber must define it if
% it's not already defined. The global variable sgtatham_exit_hook is
% defined if the correct exit hook is installed.

#ifnexists sgtatham_exit_hook
% This is a generally useful exit hook. On an attempt to exit Jed,
% it calls `eval' on the string variable `exit_hook_code'. If this calls
% error() at any point then exit will be abandoned. This allows multiple
% exit hooks to be registered by appending them to the string.
variable sgtatham_exit_hook = 1; % flag it as defined
variable exit_hook_code = ".";
define exit_hook() {
    eval(exit_hook_code);
}
#endif

exit_hook_code += " timber_exitfunc";

define timber_exitfunc() {
    variable buf;
    loop (buffer_list()) {
        buf = ();
        setbuf(buf);
        if (
#ifexists blocal_var_exists
            blocal_var_exists("timber_foldername")
#else
            substr(buf, 1, 3) == "[T]"
#endif
            ) {
                error("Timber is still running in this Jed!");
            }
    }
}

%}}}
%{{{ timber_create_blocal_var: hack to support change to create_blocal_var

% In a staggeringly irritating manoeuvre, Mr Davis in his infinite wisdom
% decided to make an incompatible change to create_blocal_var() in 0.99.
% We correct for it here.
define timber_create_blocal_var(name, type) {
    if (_jed_version < 9900)
        create_blocal_var(name, type);
    else
        create_blocal_var(name);
}

%}}}
%{{{ timber_strchopr: hack to support change to strchopr

define timber_strchopr(s, delim, quote) {
    variable a, alen, i;

    if (_jed_version >= 9900) {
        a = strchopr (s, delim, quote);
        (alen,,) = array_info(a); alen = alen[0];
        for (i = 0; i < alen; i++)
            a[i];
        alen;
    } else
        strchopr (s, delim, quote);
}

%}}}
%{{{ timber_remove: hack to support 0.98's lack of remove()
define timber_remove(file) {
#ifexists remove
    remove(file);
#else
    run_shell_cmd("rm -f " + file);
#endif
}
%}}}
%{{{ timber_undefined(): for undefined keys

% Used when a key is pressed which has no effect in Timber.
define timber_undefined() {
    error("Key undefined. Press `?' for help");
}

%}}}
%{{{ timber_tmpnam() (neither Jed nor S-Lang provides tmpnam :-( )
variable timber_tmpnam_counter = 0;
variable timber_tmpdir = NULL;
define timber_tmpnam() {
    if (timber_tmpdir == NULL) {
        timber_tmpdir = getenv("TEMP");
        if (timber_tmpdir == NULL) {
            timber_tmpdir = getenv("TMP");
            if (timber_tmpdir == NULL) {
                timber_tmpdir = getenv("TMPDIR");
                if (timber_tmpdir == NULL) {
                    timber_tmpdir = "/tmp";
                }
            }
        }
    }
    timber_tmpnam_counter += 1;
    return dircat(timber_tmpdir, sprintf("%dtimber.%d", getpid(),
                                         timber_tmpnam_counter));
}
%}}}
%{{{ timber_ro() and timber_rw()

% Utility routines: toggle the buffer's read-only status (and other flags)
define timber_ro() { setbuf_info((getbuf_info() | 0x129) & ~0x2); }
define timber_rw() { setbuf_info((getbuf_info() | 0x20) & ~8); }

%}}}
%{{{ timber_undo()

define timber_undo() { timber_rw(); call("undo"); timber_ro(); }

%}}}
%{{{ timber_fetchmail(): the fetch-mail interface

% Fetch mail from /var/spool/mail/$USER, with proper locking, and transfer
% it to the given folder. This will be done by calling an external process.
define timber_fetchmail() {
    push_mark();
    pipe_region(Sprintf("%s %s",
			timber_fetch_prog,
			dircat(timber_folders, timber_inbox), 2));
}
%}}}
%{{{ timber_acquire_lock() and timber_release_lock(): mailbox locking

% Acquire a lock on a filename. Returns 1 if successful, 0 if locked already.
define timber_acquire_lock(fname) {
    variable fs;
    fname = fname + ".LCK";
    if (file_status(fname) != 1)
	fname = dircat(timber_folders, fname);
    if (_jed_version >= 9900)
        fs = (mkdir(fname, 0700) < 0);
    else
        fs = (mkdir(fname) == 0);
    if (fs)
        return 0;
    else
        return 1;
}

% Release a lock on a mail folder.
define timber_release_lock(fname) {
    fname = fname + ".LCK";
    if (file_status(fname) != 1)
	fname = dircat(timber_folders, fname);
    rmdir(fname);
}
%}}}
%{{{ timber_la() and timber_ila(): text processing routines

% Replacement for `looking_at' which is always case sensitive.
define timber_la(s) {
    variable oldcs;
    oldcs = CASE_SEARCH;
    ERROR_BLOCK {
	CASE_SEARCH = oldcs;
    }
    EXIT_BLOCK {
	CASE_SEARCH = oldcs;
    }
    CASE_SEARCH = 1;
    return looking_at(s);
}

% Replacement for `looking_at' which is always case insensitive.
define timber_ila(s) {
    variable oldcs;
    oldcs = CASE_SEARCH;
    ERROR_BLOCK {
	CASE_SEARCH = oldcs;
    }
    EXIT_BLOCK {
	CASE_SEARCH = oldcs;
    }
    CASE_SEARCH = 0;
    return looking_at(s);
}

%}}}
%{{{ timber_getheader(): return the contents of a header field

% Return the contents of a header field. Cursor is assumed to be positioned
% just beyond the colon.
% `processed' is 1 if the buffer has already been processed.
define timber_getheader(processed) {
    variable result;
    variable string1, string2;

    if (processed) {
	string1 = "| ";
	string2 = "|\t";
    } else {
	string1 = " ";
	string2 = "\t";
    }

    result = "";

    skip_white();
    push_mark();
    eol();
    result = bufsubstr();
    go_right_1();
    while (looking_at(string1) or looking_at(string2)) {
	go_right_1();
	skip_white();
	push_mark();
	eol();
	result += " " + bufsubstr();
	go_right_1();
    }
    return result;
}

%}}}
%{{{ timber_issep(): detect a message separator line

% Detect a message separator line. Theoretically we need only require that
% the line begins "From ", but in practice some mail systems aren't strict
% about quoting other such lines, so we'll detect the exact date format.
define timber_issep() { %{{{
    variable s;
    !if (timber_la("From "))
        return 0;
    push_spot(); bol(); push_mark(); eol(); s = bufsubstr(); pop_spot();
    if (strlen(s) < 24)
        return 0;
    s = substr(s, strlen(s)-23, 24);
    !if (string_match(substr(s, 1, 12),
                      "[A-Z][a-z][a-z] [A-Z][a-z][a-z] [0-9 ][0-9] [0-9 ]", 1)
         or
         string_match(substr(s, 13, 8),
                      "[0-9]:[0-9][0-9]:[0-9][0-9] ", 1))
        return 0;
    return 1;
} %}}}

%}}}
%{{{ timber_enbuf() to process just-loaded mailbox data

% Convert a buffer containing a raw mail folder into a buffer containing
% a Timber-style mail folder.
%
% Format of a Timber mail folder buffer:
%
% Each line has one header character denoting the type of line.
% `*' denotes a message summary line: one of these exists for each
% message. `|' denotes a message header line. ` ' denotes a message body
% line. `!' denotes a summary line for a MIME attachment. `X' denotes guff at
% the end of the folder. `F' denotes a summary line right at the top.
% `\240' (the other space char) denotes a decoded MIME attachment (eg
% decoded QP). `M' denotes a heading line for MIME summaries. `+' denotes
% a MIME-component header or admin line (including the separator lines
% themselves, the headers in each component, and the `This message is MIME'
% section before the components begin).
%
% Summary lines with nothing but ` [end]' after the initial character
% denote the end of the things they summarise.
%
% This function does not take care of moving to the start of the buffer and
% inserting the header line: that is assumed to be done by other code.
define timber_enbuf() {
    variable file;

    push_mark();
    push_mark();
    eob();
    file = timber_tmpnam();
    pipe_region(timber_enbuf_prog + " " + file);
    del_region();
    insert_file(file);
    timber_remove(file);
}
%}}}
%{{{ timber_blankfolder(): make an empty folder buffer

% Given an empty buffer, fabricate an empty Timber folder inside it.
define timber_blankfolder() {
    bob();
    erase_buffer();
    insert(timber_headerline);
    insert("* [end]\n");    
}

%}}}
%{{{ timber_foldall(): fold all messages up

% Given a Timber mail-folder buffer, fold everything right up so just
% the message summary lines are visible.
define timber_foldall() {
    variable c;

    bob();
    while (not eobp()) {
	c = what_char();
	if (c == 'F' or c == '*')
	    set_line_hidden(0);
	else
	    set_line_hidden(1);
	eol();
	go_right_1();
    }
    bob();
}

%}}}
%{{{ timber_updatemail(): update a folder with new mail

% Update a mail folder (ought to be `mbox') with extra data.
define timber_updatemail() {
    variable mb, fullinbox;

    push_spot();
    eob();
    push_mark();
    go_up_1();
    bol();
    !if (looking_at("* [end]")) {
        error("Mail folder is in wrong format!");
        pop_mark();
        pop_spot();
        return;
    }
    mb = get_blocal_var("timber_foldername");
    fullinbox = dircat(timber_folders, timber_inbox);
    if (strcmp(mb, timber_inbox) and strcmp(mb, fullinbox)) {
        error("This is not your inbox!");
        pop_mark();
        pop_spot();
        return;
    }
    timber_rw();

    del_region();
    push_spot();
    push_spot();
    run_shell_cmd(Sprintf("%s %s -c",
                          timber_fetch_prog,
                          dircat(timber_folders, timber_inbox), 2));
    pop_spot();
    timber_enbuf();
    pop_spot();
    if (looking_at("* [end]"))
        message("No new mail in " + timber_inbox);

    timber_foldall(); % fold up everything
    bob();

    timber_ro(); % make buffer read-only
}

%}}}
%{{{ timber_header_key(): return the name of the header under the cursor

% Return the header field present on the line.
define timber_header_key() {
    % Separator header is special.
    if (timber_la("|From "))
	return "From ";

    push_spot();
    go_right_1();
    push_mark();
    skip_chars("^: \t\n");
    bufsubstr();
    pop_spot();
}

%}}}
%{{{ timber_unfold(): unfold either a message or a MIME part

% Unfold the message or attachment under the cursor. If it's a message,
% we must ensure the Status line says RO.
define timber_unfold() {
    variable header, h2, c, showing, firstpart, leadchr;

    bol();
    while (is_line_hidden() and not bobp()) {
	go_up_1();
	bol();
    }
    header = "";
    if (what_char() == '*' and not timber_la("* [end]")) {
	% Unfold a message.
	timber_rw();
	push_spot();
	go_down_1();
	showing = 1;
	firstpart = 1;
        leadchr = '\0';
	while (what_char() != '*' and not eobp()) {
	    c = what_char();
	    if (c == '|') {
		h2 = timber_header_key();
		if (strlen(h2))
		    header = h2;
		!if (is_substr(strlow(timber_boringhdrs),
			       strlow(":"+header+":")))
		    set_line_hidden(0);
	    } else if (c == '!' or c == ':') {
		if (c == '!' and firstpart) {
		    firstpart = 0;
		    showing = 1;
                    leadchr = '\0';
		} else if (timber_la("! [end]")) {
		    showing = 1;
                    leadchr = '\0';
		} else
		    showing = 0;
                if (c == ':')
                    firstpart = 1;
		set_line_hidden(0);
	    } else if (c != '+' and showing) {
                if (leadchr == '\0' and (c == '\240' or c == ' ')) {
                    leadchr = c;
                    set_line_hidden(0);
                } else if (c == leadchr) {
                    set_line_hidden(0);
                } else if (c == ' ' and leadchr == '\240') {
                    % Show the first space line after a decoded part.
                    set_line_hidden(0);
                    % Then inhibit showing of the rest.
                    leadchr = '\1';
                }
            }
	    if (timber_ila("|Status: ")) {
		go_right(9);
		push_mark(); eol(); del_region();
		insert("RO");
	    }
	    eol();
	    go_right_1();
	}
	pop_spot();
	go_right(2);
	deln(1);
	insert(" ");
	bol();
	recenter(1);
	timber_ro();
    } else if (what_char() == '!') {
	% Unfold an attachment.
	push_spot();
	go_down_1();
	showing = 1;
	firstpart = 1;
        leadchr = '\0';
	while (what_char() != '!' and what_char() != '*' and not eobp()) {
	    c = what_char();
            if (leadchr == '\0' and (c == '\240' or c == ' ')) {
                leadchr = c;
                set_line_hidden(0);
            } else if (c == leadchr) {
		set_line_hidden(0);
            } else if (c == ' ' and leadchr == '\240') {
                % Show the first space line after a decoded part.
                set_line_hidden(0);
                % Then inhibit showing of the rest.
                leadchr = '\1';
            }
	    eol();
	    go_right_1();
	}
	pop_spot();
	bol();
    }
}

%}}}
%{{{ timber_press_ret(): RET was pressed. Unfold or read-first-unread

% RETURN was pressed. If at top, move to the first unread message; then
% call timber_unfold.
define timber_press_ret() {
    variable c;

    if (what_line() == 1) {
        go_down_1();
        bol();
        c = 'x';                       % error indicator
        while (what_char() == '*') {
            go_right(2);
            c = what_char();
            if (c == 'N' or c == 'U')
                break;
            do {
                go_down_1();
            } while (is_line_hidden() and not eobp());
        }
        !if (c == 'N' or c == 'U') {
            message("No unread messages in folder.");
            bob();
            return;
        }
        bol();
    }
    timber_unfold();
}

%}}}
%{{{ timber_bom(): go to beginning of current message

% Move to the top of the current message.
define timber_bom() {
    bol();
    while (what_char() != '*' and not bobp()) {
	go_up_1();
	bol();
    }
}

%}}}
%{{{ timber_fullhdr(): display full headers in current message

% Unhide _all_ headers in the current message.
define timber_fullhdr() {
    timber_bom();
    push_spot();
    eol();
    go_right_1();
    while (what_char == '|' and bolp()) {
	set_line_hidden(0);
	eol();
	go_right_1();
    }
    pop_spot();
    recenter(1);
}

%}}}
%{{{ timber_fullmime(): display full headers for current MIME part

% Unhide _all_ of the current MIME part.
define timber_fullmime() {
    variable mark;

    mark = create_user_mark();
    bol();
    while (what_char() != '!' and what_char() != ':'
	   and what_char() != '*' and not bobp()) {
	go_up_1();
	bol();
    }
    if (what_char() == '*') {
	goto_user_mark(mark);
	error("Not in a MIME attachment.");
	return;
    }

    push_spot();
    eol();
    go_right_1();
    while (what_char != '!' and what_char != ':'
	   and what_char != '*' and not eolp()) {
	set_line_hidden(0);
	eol();
	go_right_1();
    }
    pop_spot();
}

%}}}
%{{{ timber_get_mimepart(): position user marks at each end of MIME part

% Place user marks at each end of the current MIME part. Will use a
% decoded variant if `use_decoded' is TRUE, otherwise will always go
% for the original transfer encoding.
%
% Returns (markattop, markatbottom, type, encoding).
define timber_get_mimepart(use_decoded) {
    variable encoding = "7BIT";	       % default
    variable type = "text/plain";      % default
    variable headerchr, leadchr;
    variable mark, top, bottom;

    mark = create_user_mark();

    bol();
    while (what_char() != '!' and what_char() != ':'
	   and what_char() != '*' and not bobp()) {
	go_up_1();
	bol();
    }

    if (what_char() == ':' or bobp()) {
	goto_user_mark(mark);
	error("Not in a MIME attachment.");
	return (NULL, NULL, "");
    }

    if (what_char() == '*')
	headerchr = '|';
    else
	headerchr = '+';	

    eol();
    go_right_1();
    while (what_char == headerchr and not eolp()) {
	go_right_1();
	if (timber_ila("Content-Transfer-Encoding: ")) {
	    go_right(27);
	    push_mark();
	    eol();
	    encoding = strup(bufsubstr());
	}
	if (timber_ila("Content-Type: ")) {
	    go_right(14);
	    push_mark();
	    skip_chars("^; \n");
	    type = strlow(bufsubstr());
	}
	eol();
	go_right_1();
    }

    if (use_decoded) {
        % This might be decoded or it might not be: select our lead char
        % accordingly.
        leadchr = what_char();             % either ' ' or '\240'
    } else {
        % Skip the decoded version if any.
        while (what_char == '\240' and not eolp()) {
            eol();
            go_right_1();
        }
        leadchr = ' ';
    }

    % Now we should be at the top of the message text. Skip the
    % initial blank line.
    eol();
    if (what_column() == 2)
	go_right_1();
    else
	bol();

    top = create_user_mark();

    % Now move down to the end of the message text.
    while (what_char == leadchr and not eolp()) {
	eol();
	go_right_1();
    }

    % Trash the trailing blank line.
    go_left_1();
    if (what_column() == 2)
	bol();
    else
	go_right_1();

    bottom = create_user_mark();

    !if (strncmp(type, "text", 4) and strncmp(type, "message", 7))
        encoding += "+";

    return (top, bottom, type, encoding);
}

%}}}
%{{{ timber_yesno (): standardised yes-or-no minibuffer input

% Get a yes/no response from the user. The one in site.sl is ugly.
define timber_yesno (prompt)
{
    variable c;

    flush (prompt);
    EXIT_BLOCK {
        clear_message();
    }
    while (1) {
	c = tolower(getkey());
	if (c == 'y') return 1;
	if (c == 'n') return 0;
	if (c != '\r' and c != '\n') beep();
    }
}

%}}}
%{{{ timber_saveattach(): save a MIME attachment to a file

% Save the current MIME part (or whole message, if non-multipart) to a file.
define timber_saveattach() {
    variable mark, top, bot;
    variable file, encoding;

    file = read_file_from_mini("File to save attachment to:");
    !if (strlen (extract_filename(file))) return;
    if (file_status(file) != 0) {
	!if (timber_yesno("Really overwrite " + file + "? [yn] "))
	    return;
    }
    mark = create_user_mark();
    (top, bot, , encoding) = timber_get_mimepart(0);
    !if (top == NULL) {
        goto_user_mark(top);
        push_mark();
        goto_user_mark(bot);
        % We have an attachment in the marked region. Save it.
        pipe_region(timber_mime_prog + " - " + encoding + " " + file);
    }
    goto_user_mark(mark);
}

%}}}
%{{{ timber_selattach(): select the current MIME part (eg for replying)

% Select the current MIME part.
define timber_selattach() {
    variable mark, top, bot;

    mark = create_user_mark();
    (top, bot, , ) = timber_get_mimepart(1);
    if (top == NULL) {
        goto_user_mark(mark);
    } else {
        goto_user_mark(top);
        set_mark_cmd();
        goto_user_mark(bot);
    }
}

%}}}
%{{{ timber_fold(): fold up the current MIME part or message

% Fold up the message or attachment under the cursor.
define timber_fold() {
    variable attach, c;

    push_spot();
    bol();

    if (what_char() == 'F') {
        pop_spot();
        error("Not on a message or attachment");
        return;
    } else {
        pop_spot();
        bol();
    }

    % work out what we're folding. If the line is a ! line, we need to know
    % whether the corresponding attachment is already folded or not. If not,
    % fold that. If so, fold the message.
    % Otherwise, search backwards for the nearest ! or * line, and fold
    % whatever we hit first.
    if (what_char() == '!') {
	timber_la("! [end]");
	push_spot();
	eol();
	go_right_1();
	attach = 0;
	if (()) {
	    % Special case: for the `! [end]' line, only consider
	    % headers after it to be potential-fold-stuff.
	    while (what_char() == '+' and not eobp()) {
		!if (is_line_hidden()) {
		    attach = 1;
		    break;
		}
		eol();
		go_right_1();
	    }
	} else {
	    while (what_char() != '!' and what_char() != '*' and not eobp()) {
		!if (is_line_hidden()) {
		    attach = 1;
		    break;
		}
		eol();
		go_right_1();
	    }
	}
	pop_spot();
	!if (attach) {
	    while (what_char() != '*' and not bobp()) {
		go_up_1();
		bol();
	    }
	}
    } else {
	while (what_char() != '*' and what_char() != '!' and not bobp()) {
	    go_up_1();
	    bol();
	}
    }

    % Special case: if we're trying to fold the `! [end]' line, only hide
    % headers after it. The blank line after that we want to keep.
    if (timber_la("! [end]")) {
	push_spot();
	go_down_1();
	while (what_char() == '+' and not eobp()) {
	    set_line_hidden(1);
	    eol();
	    go_right_1();
	}
	pop_spot();
	return;
    }

    c = what_char();
    push_spot();
    go_down_1();
    while (what_char() != c and not eobp()) {
	set_line_hidden(1);
	eol();
	go_right_1();
    }
    pop_spot();
}

%}}}
%{{{ timber_fold_msg(): fold up the current message

% Fold up the message under the cursor.
define timber_fold_msg() {
    variable attach, c;

    push_spot();
    bol();

    if (what_char() == 'F') {
        pop_spot();
        error("Not on a message");
        return;
    } else {
        pop_spot();
        bol();
    }

    % Search backwards for the nearest * line.
    while (what_char() != '*' and not bobp()) {
	go_up_1();
	bol();
    }

    % Fold the message.
    push_spot();
    go_down_1();
    while (what_char() != '*' and not eobp()) {
	set_line_hidden(1);
	eol();
	go_right_1();
    }
    pop_spot();
}

%}}}
%{{{ timber_mimedec(): decode a MIME part

define timber_mimedec() {
    variable mark, top, bottom, type, encoding, tmp, decode_cmd;

    mark = create_user_mark();
    (top, bottom, type, encoding) = timber_get_mimepart(1);
    if (top == NULL) {
        goto_user_mark(mark);
    } else {
        goto_user_mark(top);
        tmp = timber_tmpnam();
        if (timber_la(" ")) {
            % Decode from content transfer encoding
            decode_cmd = timber_mime_prog + " - " + encoding + " " + tmp;
        } else {
            % Decode from content type
            decode_cmd = timber_mime_prog + " - " + strup(type) + " " + tmp;
        }
        % Perform the decode to a temporary file
        goto_user_mark(top);
        push_mark();
        goto_user_mark(bottom);
        pipe_region(decode_cmd);

        % Remove any existing decoded part
        goto_user_mark(top);
        if (timber_la("\240")) {
            push_mark();
            while (timber_la("\240")) {
                go_down_1();
                bol();
            }
            timber_rw();
            del_region();
            timber_ro();
        }

        timber_rw();
        % Put on an initial blank \240 line
        go_up_1();
        bol();
        insert("\240\n");
        % Read the file back in as a decoded part
        run_shell_cmd(timber_prefix_prog + " 160 " + tmp);
        % This is for display/reply purposes, so we should insert a
        % newline here in case there wasn't one on the end of the
        % decoded data
        !if (bolp()) insert("\n");
        timber_ro();

        % Get the new folding correct
        timber_fold();
        timber_unfold();
    }
}

%}}}
%{{{ timber_mimerev(): revert to fully encoded MIME form

define timber_mimerev() {
    variable mark, top;

    mark = create_user_mark();
    (top,,,) = timber_get_mimepart(1);
    if (top == NULL) {
        goto_user_mark(mark);
    } else {
        % Remove any decoded part
        goto_user_mark(top);
        go_up_1();
        bol();
        if (timber_la("\240")) {
            push_mark();
            while (timber_la("\240")) {
                go_down_1();
                bol();
            }
            timber_rw();
            del_region();
            timber_ro();

            % Get the new folding correct
            timber_fold();
            timber_unfold();
        }
    }
}

%}}}
%{{{ timber_mimedestroy(): destroy a MIME part

define timber_mimedestroy() {
    variable mark, type;

    mark = create_user_mark();

    bol();
    while (what_char() != '!' and what_char() != ':'
	   and what_char() != '*' and not bobp()) {
	go_up_1();
	bol();
    }

    if (what_char() != '!' or bobp()) {
	goto_user_mark(mark);
	error("Not in a MIME attachment.");
	return;
    }

    (,,type,) = timber_get_mimepart(1);
    goto_user_mark(mark);
    !if (timber_yesno("Really destroy attachment of type "+type+"? [yn] ")) {
        return;
    }

    timber_rw();

    push_mark();
    eol();
    del_region();
    insert("! text/x-rcpt-removed"); % replace the Timber marker line
    go_right_1(); % now we're on the separator line
    eol();
    go_right_1(); % and now we're at the start of the attachment headers

    insert("+Content-Type: text/x-rcpt-removed; charset=\"us-ascii\"\n \n");
    insert(" This attachment was removed by the recipient after delivery.\n");
    insert(" The MIME headers for the original attachment follow.\n \n");

    while (what_char() == '+' and not eolp()) {
        deln(1);
        insert_char(' ');
        eol();
        go_right_1(); % next header
    }
    insert(" \n");
    push_mark();
    while (what_char() != '!' and not eolp()) {
        eol();
        go_right_1();
    }
    del_region();

    timber_ro();
    go_up_1();
    bol();

    while (what_char() != '!') {
        set_line_hidden(1);
        go_up_1();
        bol();
    }
}

%}}}
%{{{ timber_open_folder(): load a message folder into a Timber buffer

% Create a new buffer, in Timber mode, and open a mail folder therein.
define timber_open_folder(name) {
    variable modename = "Timber";
    variable fname, dname, bname, flags;

    timber_buffers_more();

    fname = dircat(timber_folders, extract_filename(name));
    if (fname == name)
	fname = extract_filename(name);
    else
	fname = name;
    sw2buf(Sprintf("[T] %s", fname, 1));

    set_mode(modename, 0);
    use_keymap(modename);
    use_syntax_table(modename);
#ifexists use_dfa_syntax
    use_dfa_syntax(1);
#endif

    set_status_line("(Jed %v) Timber: %b   (%p%n)   Press `?' for help", 0);

    fname = name;
    if (file_status(fname) != 1)
	fname = dircat(timber_folders, name);

    if (insert_file(fname)) {
        bob();
        insert(timber_headerline);
	timber_enbuf();
    } else {
	timber_blankfolder();
	message(Sprintf("Inventing empty folder `%s'.", name, 1));
    }

    timber_create_blocal_var("timber_foldername", 's');
    set_blocal_var(fname, "timber_foldername");

    timber_foldall(); % fold up everything
    bob();

    timber_ro(); % make buffer read-only
}

%}}}
%{{{ timber_nextmsg(): go to next message

% Move to the next message, by folding whatever we're currently on and
% unfolding the next.
define timber_nextmsg() {
    timber_bom();
    if (what_char() == '*')
        timber_fold();
    do {
	go_down_1();
    } while (is_line_hidden() and not eobp());
    if (timber_la("* ") and not timber_la("* [end]"))
        timber_unfold();
}

%}}}
%{{{ timber_prevmsg(): go to previous message

% Move to the previous message, by folding whatever we're currently on
% and unfolding the one before.
define timber_prevmsg() {
    timber_bom();
    if (what_char() == '*')
        timber_fold();
    do {
	go_up_1();
	bol();
    } while (what_char() != '*' and not bobp());
    if (timber_la("* ") and not timber_la("* [end]"))
        timber_unfold();
}

%}}}
%{{{ timber_count(): count messages in a Timber buffer

define timber_count() {
    variable count = 0, deleted = 0;
    variable current = 0;
    variable prefix, msg;
    variable line = what_line();

    push_spot();
    EXIT_BLOCK { pop_spot(); }
    bob();
    while (not eobp()) {
	go_down_1();
	if (what_char() == '*') {
	    !if (timber_la("* [end]")) {
		count++;
		push_spot();
		push_mark();
		go_right(5);
		prefix = bufsubstr();
		pop_spot();
		if (prefix[4] == 'D')
		    deleted++;
	    }
	    if (what_line() == line)
		current = count;
	}
    }
    msg = sprintf("You are on message %d of %d", current, count);
    if (deleted > 0) {
	msg = msg + sprintf(" (%d deleted, %d not deleted)",
			    deleted, count-deleted);
    }
    message(msg);
}

%}}}
%{{{ timber_delete(): mark message as deleted

% Mark the current message as deleted. Fold it as well (deleted messages
% are unlikely to be ones people are interested in).
define timber_delete() {
    timber_bom();
    if (timber_la("* ") and not timber_la("* [end]")) {
	timber_rw();
	go_right(4);
	deln(1);
	insert("D");
	timber_ro();
        timber_fold();
    }
}

%}}}
%{{{ timber_undelete(): mark message as not deleted after all

% Mark the current message as not deleted.
define timber_undelete() {
    push_spot();
    timber_bom();
    if (timber_la("* ") and not timber_la("* [end]")) {
	timber_rw();
	go_right(4);
	deln(1);
	insert(" ");
	timber_ro();
    }
    pop_spot();
}

%}}}
%{{{ timber_expunge(): checkpoint Timber buffer to disk and remove deletia

% Expunge all deleted messages and checkpoint a buffer to disk.
define timber_expunge() {
    variable from, to, c, text;
    variable fbuf, tbuf, fname;

    bob();
    while (1) {
	% Find a message.
	while (not timber_la("* ") and not eobp()) {
	    eol();
	    go_right_1();
	} 
	if (timber_la("* [end]") or eobp())
	    break;
	go_right(4);
	if (timber_la("D")) {
	    % This message is deleted. Trash it.
	    bol();
	    push_mark();
	    do {
		eol();
		go_right_1();
	    } while (not timber_la("* ") and not eobp());
	    timber_rw();
	    del_region();
	    timber_ro();
	} else {
	    eol();
	    go_right_1();
	}
    }

    % Now we have expunged things, we should checkpoint.
    fname = get_blocal_var("timber_foldername");
    from = create_user_mark();
    fbuf = whatbuf();
    tbuf = "*timbertmp*";
    setbuf(tbuf);
    erase_buffer();
    to = create_user_mark();
    setbuf(fbuf); goto_user_mark(from);
    bob();
    while (not eobp()) {
	move_user_mark(from);
	c = what_char();
	if (c == '|' or c == ' ' or c == '+') {
	    go_right(1);
	    push_mark();
	    eol();
	    go_right_1();
	    text = bufsubstr();
	    move_user_mark(from);
	    setbuf(tbuf); goto_user_mark(to);
	    insert(text);
	    move_user_mark(to);
	    setbuf(fbuf); goto_user_mark(from);
	} else {
	    eol();
	    go_right_1();
	}
    }
    setbuf(tbuf);
    bob(); push_mark(); eob();
    if (bobp()) {
	delete_file(fname);
	message(Sprintf("Empty folder `%s' removed.", fname, 1));
    } else
	write_region_to_file(fname);
    setbuf_info(getbuf_info & ~1); % clear modified bit to make delbuf silent
    setbuf(fbuf);
    delbuf(tbuf);

    timber_foldall();
    bob();
}

%}}}
%{{{ timber_qlose(): close Timber buffer (daft name 'cos it's bound to `q')

% Quit a Timber buffer, expunging and closing the associated folder.
define timber_qlose() {
    timber_expunge();
    timber_release_lock(get_blocal_var("timber_foldername"));
    setbuf_info(getbuf_info & ~1); % clear modified bit to make delbuf silent
    delbuf(whatbuf());
    timber_buffers_fewer();
}

%}}}
%{{{ timber_readfolder(): get a folder name from the minibuffer

% Read a folder name.
define timber_readfolder(prompt) {
    variable f, d, b, flags;
    (f,d,b,flags) = getbuf_info();
    ERROR_BLOCK {
	setbuf_info(f,d,b,flags);
    }
    setbuf_info(f, expand_filename(timber_folders), b, flags);
    read_file_from_mini (prompt);
    setbuf_info(f,d,b,flags);
}

%}}}
%{{{ timber_goto(): prompt for a folder name then open that folder

% Open a new Timber folder-buffer.
define timber_goto() {
    variable file;

    file = timber_readfolder("Open new folder:");
    !if (strlen (extract_filename(file))) return;
    if (timber_acquire_lock(file)) {
        timber_open_folder(file);
    } else {
	error(file + " is locked by another Timber!");
    }
}

%}}}
%{{{ timber_moveto(): prompt for a folder name then move to that folder

% Move to another folder.
define timber_moveto() {
    variable file, oldbuf, newbuf;

    file = timber_readfolder("Move to new folder:");
    !if (strlen (extract_filename(file))) return;
    if (timber_acquire_lock(file)) {
        oldbuf = whatbuf();
        timber_open_folder(file);
        newbuf = whatbuf();
        setbuf(oldbuf);
        timber_qlose();
        setbuf(newbuf);
    } else {
	error(file + " is locked by another Timber!");
    }
}

%}}}
%{{{ timber_movemsgup(): exchange a message with the one above it

define timber_movemsgup() {
    variable here, top, bot, fbuf, tbuf;

    here = create_user_mark();
    %ERROR_BLOCK { goto_user_mark(here); }
    %EXIT_BLOCK { goto_user_mark(here); }
    % Mark the current message.
    bol();
    while (what_char != '*' and not bobp()) { go_up_1(); bol(); }
    if (bobp()) { error("Not on a message."); return; }
    top = create_user_mark();
    eol(); go_right_1();
    while (what_char != '*' and not eobp()) { eol(); go_right_1(); }
    if (eobp()) { error("Not on a message."); return; }    
    bot = create_user_mark();
    % Find where we want to move it to.
    goto_user_mark(top); go_up_1(); bol();
    while (what_char != '*' and not bobp()) { go_up_1(); bol(); }
    if (bobp()) { error("Message is at top already."); return; }

    % Set up a temporary buffer.
    fbuf = whatbuf();
    tbuf = "*timbertmp*";
    setbuf(tbuf);
    erase_buffer();
    setbuf(fbuf);

    % Move the other message. (We want to move the other one rather than
    % our one so as to preserve `here'.
    timber_rw();
    push_mark();
    push_mark(); 
    goto_user_mark(top);
    copy_region(tbuf);
    del_region();
    goto_user_mark(bot);
    insbuf(tbuf);
    timber_ro();

    % Remove the temporary buffer.
    setbuf(tbuf);
    setbuf_info(getbuf_info & ~1); % clear modified bit to make delbuf silent
    setbuf(fbuf);
    delbuf(tbuf);

    % Fold the message we have just moved.
    go_up_1();
    timber_fold_msg();
}

%}}}
%{{{ timber_movemsgdown(): exchange a message with the one below it

define timber_movemsgdown() {
    variable here, top, bot, fbuf, tbuf;
    variable specialcase = 0;

    here = create_user_mark();
    ERROR_BLOCK { goto_user_mark(here); }
    EXIT_BLOCK { goto_user_mark(here); }
    % Mark the current message.
    if (bolp() and what_char == '*') specialcase = 1;
    bol();
    while (what_char != '*' and not bobp()) { go_up_1(); bol(); }
    if (bobp()) { error("Not on a message."); return; }
    top = create_user_mark();
    eol(); go_right_1();
    while (what_char != '*' and not eobp()) { eol(); go_right_1(); }
    if (eobp()) { error("Not on a message."); return; }    
    bot = create_user_mark();
    % Find where we want to move it to.
    goto_user_mark(bot); eol(); go_right_1();
    while (what_char != '*' and not eobp()) { eol(); go_right_1(); }
    if (eobp()) { error("Message is at bottom already."); return; }

    % Set up a temporary buffer.
    fbuf = whatbuf();
    tbuf = "*timbertmp*";
    setbuf(tbuf);
    erase_buffer();
    setbuf(fbuf);

    % Move the other message. (We want to move the other one rather than
    % our one so as to preserve `here'.
    timber_rw();
    push_mark();
    push_mark(); 
    goto_user_mark(bot);
    copy_region(tbuf);
    del_region();
    goto_user_mark(top);
    insbuf(tbuf);
    timber_ro();
    if (specialcase)
        here = create_user_mark();

    % Remove the temporary buffer.
    setbuf(tbuf);
    setbuf_info(getbuf_info & ~1); % clear modified bit to make delbuf silent
    setbuf(fbuf);
    delbuf(tbuf);

    % Fold the message we have just moved.
    go_up_1();
    timber_fold_msg();
}

%}}}
%{{{ timber_savetobuf(): save the message under the cursor to another buffer

% Save a message to another Timber buffer.
define timber_savetobuf(buffer) {
    variable frombuf;

    frombuf = whatbuf();
    setbuf(buffer);
    timber_rw();
    push_spot();
    eob();
    bol();
    while (not timber_la("* [end]") and not bobp()) {
	go_up_1();
	bol();
    }

    setbuf(frombuf);

    timber_bom();
    push_mark();
    go_down_1();
    while (what_char() != '*' and not eobp()) {
	eol();
	go_right_1();
    }
    copy_region(buffer);

    setbuf(buffer);
    go_up_1();
    timber_fold();
    timber_ro();
    pop_spot();

    setbuf(frombuf);
}

%}}}
%{{{ timber_appendmsg(): append the message under the cursor to a file

% Write a message, in non-Timber-buffer form, to the end of a file.
define timber_appendmsg(file) {
    variable from, to, fbuf, tbuf, c, text;

    from = create_user_mark();
    fbuf = whatbuf();
    tbuf = "*timbertmp*";
    setbuf(tbuf);
    erase_buffer();
    to = create_user_mark();
    setbuf(fbuf); goto_user_mark(from);
    timber_bom();
    go_down_1();
    while (what_char != '*' and not eobp()) {
	move_user_mark(from);
	c = what_char();
	if (c == '|' or c == ' ' or c == '+') {
	    go_right(1);
	    push_mark();
	    eol();
	    go_right_1();
	    text = bufsubstr();
	    move_user_mark(from);
	    setbuf(tbuf); goto_user_mark(to);
	    insert(text);
	    move_user_mark(to);
	    setbuf(fbuf); goto_user_mark(from);
	} else {
	    eol();
	    go_right_1();
	}
    }
    setbuf(tbuf);
    bob(); push_mark(); eob();
    append_region_to_file(file);
    setbuf_info(getbuf_info & ~1); % clear modified bit to make delbuf silent
    setbuf(fbuf);
    delbuf(tbuf);
}

%}}}
%{{{ timber_save(): save a message to another folder

% Save a message to another Timber folder. Will check whether the folder
% is already open in another buffer.
define timber_save() {
    variable file;
    variable buffer;

    push_spot();

    file = timber_readfolder("Folder to save to:");
    !if (strlen (extract_filename(file))) return;
    buffer = strcat("[T] ", file);
    if (bufferp(buffer)) {
	timber_savetobuf(buffer);
    } else {
	buffer = strcat("[T] ", extract_filename(file));
	if (bufferp(buffer))
	    timber_savetobuf(buffer);
    }

    pop_spot();
    push_spot();
    timber_appendmsg(file);
    pop_spot();

    timber_delete();
    message(Sprintf("Saved message to folder %s.", file, 1));
}

%}}}
%{{{ timber_export(): export a message to a file

% Export a message to a file.
define timber_export() {
    variable file;
    variable buffer;

    file = read_file_from_mini("File to export to:");
    !if (strlen (extract_filename(file))) return;

    push_spot();
    timber_appendmsg(file);
    pop_spot();

    message(Sprintf("Exported message to file %s.", file, 1));
}

%}}}
%{{{ timber_sendmsg(): send the message in a composer buffer (prompts first)

% Send a message.
define timber_sendmsg() {
    % FIXME: we should at least slightly validate the message here. In
    % particular, we should check to see whether a From: header has been
    % used and whether it contains one of the addresses of the user.
    % We should also (potentially) munge Date headers until they're not
    % lies. More prosaically, we should check for the blank line
    % separating headers from body.

    !if (timber_yesno("Really send message? [yn] "))
	return;

    bob();
    push_mark();
    eob();
    if (pipe_region(timber_sendmail))
	error("Error sending message!");
    else {
	setbuf_info(getbuf_info & ~1); % clear mod bit to make delbuf silent
	delbuf(whatbuf());
	timber_buffers_fewer();
    }
}

%}}}
%{{{ timber_killmsg(): destroy a composer buffer (prompts first)

% Kill (cancel) a composer buffer.
define timber_killmsg() {
    !if (timber_yesno("Abandon composition of this message? [yn] "))
	return;

    setbuf_info(getbuf_info & ~1); % clear mod bit to make delbuf silent
    delbuf(whatbuf());
    timber_buffers_fewer();
}

%}}}
%{{{ timber_open_composer(): create a composer buffer

% Open a composer buffer.
define timber_open_composer() {
    variable bufname, number;

    timber_buffers_more();

    bufname = "*composer*";
    number = 0;
    while (bufferp(bufname)) {
	number++;
	bufname = Sprintf("*composer%d*", number, 1);
    }
    sw2buf(bufname);
    erase_buffer();
    text_mode();
    use_keymap("TimberC");
    use_syntax_table("TimberC");
#ifexists use_dfa_syntax
    use_dfa_syntax(1);
#endif

    timber_create_blocal_var("is_reply", 'i');

    insert("X-Mailer: Jed/Timber " + timber_version + "\n");
    insert(timber_custom_headers);
}

%}}}
%{{{ timber_real_addr(): parse From: lines etc to get actual address part

% Get the actual address part out of a freeform address segment. Ie
% return only what's in angle brackets if anything is, and otherwise
% remove what's in round brackets if anything is.
define timber_real_addr(addr) {
    variable i, j;

    addr = strtrim(addr);

    % FIXME: quoting and stuff is probably wrong here
    if (string_match(addr, "^.*<\\(.*\\)>", 1)) {
	(i,j) = string_match_nth(1);
	return substr(addr, i+1, j);
    } else if (string_match(addr, "^\\(.+\\) (.*)", 1)) {
	(i,j) = string_match_nth(1);
	return substr(addr, i+1, j);
    } else
	return addr;
}

%}}}
%{{{ timber_contains_addr(): search for an address in a comma-sep list

% Return nonzero if the address in `addr' is somewhere in the comma-
% separated list `list'.
define timber_contains_addr(list, addr) {
    variable i, a1, a2;
    
    a2 = timber_real_addr(addr);
    i = timber_strchopr (list, ',', '\\');
    while (i) {
	i--;
	a1 = timber_real_addr(());
	!if (strcmp(a1,a2)) {
	    while (i) {
		i--;
		pop();
	    }
	    return 1;
	}
    }
    return 0;
}

%}}}
%{{{ timber_insert_hdr(): insert header into composer buffer, with wrapping

% Insert a header line into a buffer. Wrap at commas if possible and
% desirable.
define timber_insert_hdr(header) {
    variable j;

    while (strlen(header) > 71) {
	% Split at the last comma before col 71 if one exists. Note
	% that this relies on the regexp matching being greedy.
	if (string_match(substr(header,1,75), "^\\(.*,\\)", 1)) {
	    (,j) = string_match_nth(1);
	    insert(substr(header, 1, j) + "\n");
	    header = "\t" + substr(header, j+1, -1);
	} else {
	    % Split at the first comma (will be _after_ col 71) otherwise.
	    j = is_substr(header, ",");
	    if (j) {
		insert(substr(header, 1, j) + "\n");
		header = "\t" + substr(header, j+1, -1);
	    } else {
		% If we can't even do _that_, give up.
		break;
	    }
	}
    }
    insert(header + "\n");
}

%}}}
%{{{ timber_bcc_self(): insert `Bcc: self' header in a composer buffer

% Add a Bcc-to-self line in a composition.
define timber_bcc_self() {
    push_spot();
    bob();
    while (not eolp()) {
	eol();
	go_right_1();
    }
    insert("Bcc: " + timber_bcc_addr + "\n");
    pop_spot();
}

%}}}
%{{{ timber_attach(): add an Attach: header in a composer buffer

% Add an Attach: header line in a composition.
define timber_attach() {
    variable fname, type, fbuf, tbuf;

    fname = read_file_from_mini ("File to attach:");
    % Try to determine MIME type.
    push_mark();
    push_mark();
    run_shell_cmd(timber_mimetype_prog + " " +
                  expand_filename(timber_mimetypes) + " " + fname);
    type = bufsubstr();
    del_region();
    if (type == "")
        type = "application/octet-stream"; % sensible default MIME type
    type = read_mini("MIME type:", "", type);

    push_spot();
    bob();
    while (not eolp()) {
	eol();
	go_right_1();
    }
    insert("Attach: " + fname + " " + type + "\n");
    pop_spot();
}

%}}}
%{{{ timber_reply*(): generate composer buffer with reply template

% Begin composition of a reply message. `all' is 1 if reply-to-all
% is enabled.
define timber_reply_common(all) {
    variable from, replyto, to, cc, subj, mid, attribution;
    variable orig_subj, orig_from, orig_replyto, orig_to;
    variable outto, outcc;
    variable i, j, addr, raddr;
    variable got_quote = 0;
    variable fbuf, tbuf;

    if (is_visible_mark()) {
	fbuf = whatbuf();
	tbuf = "*timbertmp*";
	setbuf(tbuf);
	erase_buffer();
	setbuf(fbuf);
	copy_region(tbuf);
	got_quote = 1;
    }

    push_spot();

    timber_bom();

    from = "";
    replyto = "";
    to = "";
    cc = "";
    mid = "";
    subj = "message with no subject";
    orig_subj = "";
    eol();
    go_right_1();
    while (1) {
	if (bolp() and timber_la("|")) {
	    if (timber_ila("|From:")) {
		go_right(6);
		from = timber_getheader(1);
	    } else if (timber_ila("|To:")) {
		go_right(4);
		to = timber_getheader(1);
	    } else if (timber_ila("|Reply-To:")) {
		go_right(10);
		replyto = timber_getheader(1);
	    } else if (timber_ila("|Cc:")) {
		go_right(4);
		cc = timber_getheader(1);
	    } else if (timber_ila("|Subject:")) {
		go_right(9);
		subj = timber_getheader(1);
		orig_subj = subj;
	    } else if (timber_ila("|Message-Id:")) {
		go_right(12);
		mid = timber_getheader(1);
	    } else {
		eol();
		go_right_1();
	    }
	} else
	    break;
    }
    orig_to = to;
    orig_from = from;
    orig_replyto = replyto;

    pop_spot();

    if (strlen(from))
	attribution = from;
    else if (strlen(replyto))
	attribution = replyto;
    else
	attribution = "You";

    % So. There are several cases:
    %
    % (1) If a Reply-To header exists, use it in place of From, and
    % continue processing from (2). Except that attributions come
    % from the From line rather than Reply-To...
    %
    % (2) If the From header contains one of _our_ addresses, then we
    % sent this message in the first place, so we should simply use
    % the To and Cc fields from the last time. (This applies _whether
    % or not_ `all' is set.)
    %
    % (3) Otherwise, we copy the whole of the From header into the
    % outgoing To header, and if `all' is set then any addresses in
    % To or Cc that aren't either us or already in outgoing To should
    % be placed in outgoing Cc.

    % Point (1).
    if (strlen(replyto))
	from = replyto;

    % FIXME: James will take the piss until I expand this test to be
    % able to cope with multiple addresses in the From header.
    if (timber_contains_addr(timber_addresses, from)) {
	% Case (2).
	outto = to;
	outcc = cc;
    } else {
	% Case (3).
	outto = from;
	outcc = "";
	if (all) {
	    i = timber_strchopr (to + "," + cc, ',', '\\');
	    while (i) {
		i--;
		addr = ();
		raddr = timber_real_addr(addr);
		!if (strlen(raddr))
		    continue;
		!if (timber_contains_addr(outto, addr)
		     or timber_contains_addr(timber_addresses, addr)) {
		    if (strlen(outcc))
			outcc += ",";
		    outcc += addr;
		}
	    }
            !if (outcc == "") {
                % Reply-to-all and there is at least one address involved.
                % Get confirmation first.
                !if (timber_yesno("Really reply to everybody? [yn] "))
                    outcc = "";
            }
	}
    }

    % Fabricate the subject line.
    if (string_match(subj, "^[Rr][eE]: \\(.*\\)$", 1)) {
	(i,j) = string_match_nth(1);
	subj = substr(subj, i+1, j);
    }
    subj = "Re: " + subj;

    % So now open a composer window.
    timber_open_composer();

    timber_create_blocal_var("orig_to", 's');
    set_blocal_var(orig_to, "orig_to");
    timber_create_blocal_var("orig_replyto", 's');
    set_blocal_var(orig_replyto, "orig_replyto");
    timber_create_blocal_var("orig_from", 's');
    set_blocal_var(orig_from, "orig_from");
    timber_create_blocal_var("orig_subj", 's');
    set_blocal_var(orig_subj, "orig_subj");
    set_blocal_var(1, "is_reply");

    if (strlen(mid)) insert("In-Reply-To: " + mid + "\n");
    timber_insert_hdr("To: " + outto);
    if (strlen(outcc)) timber_insert_hdr("Cc: " + outcc);
    insert("Subject: " + subj + "\n");

    insert("\n");
    push_spot();

    if (got_quote) {
	insert(attribution + " wrote:\n\n");
	push_spot();
	insbuf(tbuf);
	fbuf = whatbuf();
	setbuf(tbuf);
	setbuf_info(getbuf_info & ~1); % clear mod bit to make delbuf silent
	delbuf(tbuf);
	setbuf(fbuf);
	pop_spot();
	while (not eobp()) {
	    deln(1);
	    insert(timber_quote_str);
	    eol();
	    go_right_1();
	}
	!if (bolp())
	    insert("\n");
	insert("\n");
    }

    if (file_status(expand_filename(timber_sig)) == 1) {
	insert("\n-- \n");
	insert_file(expand_filename(timber_sig));
	!if (bolp()) insert("\n");
    }

    runhooks("timber_compose_hook");

    bob();
    recenter(1);
    pop_spot();

    setbuf_info( (getbuf_info() & ~0x303) | 0x20 );
}

define timber_reply() { timber_reply_common(0); }
define timber_reply_all() { timber_reply_common(1); }

%}}}
%{{{ timber_forward(): generate composer buffer with MIME-forwarding template

% Begin composition of a forward message.
define timber_forward() {
    variable subj;
    variable i, j, addr, raddr;
    variable got_quote = 0;
    variable fbuf, tbuf;

    push_spot();

    timber_bom();

    subj = "message with no subject";
    push_mark();
    eol();
    go_right_1();
    while (1) {
	if (bolp() and not timber_la("*")) {
	    if (timber_ila("|Subject:")) {
		go_right(9);
		subj = timber_getheader(1);
	    } else {
		eol();
		go_right_1();
	    }
	} else
	    break;
    }

    fbuf = whatbuf();
    tbuf = "*timbertmp*";
    setbuf(tbuf);
    erase_buffer();
    setbuf(fbuf);
    copy_region(tbuf);
    setbuf(tbuf);
    bob();
    % we expect two lines of crud: the Timber header and the "|From " line
    push_mark(); go_down(2); bol(); del_region();
    % now debuf it
    while (what_char != '*' and not eobp()) {
	i = what_char();
	if (i == '|' or i == ' ' or i == '+') {
            deln(1);
            eol();
            go_right_1();
        } else {
            push_mark();
            eol();
            go_right_1();
            del_region();
        }
    }
    % remove trailing blank line
    eob();
    go_left_1();
    if (bolp()) deln(1);
    % message is now ready to be forwarded
    setbuf(fbuf);

    % Fabricate the subject line.
    if (string_match(subj, "^\\(.*\\) (fwd)$", 1)) {
	(i,j) = string_match_nth(1);
	subj = substr(subj, i+1, j);
    }
    subj = subj + " (fwd)";

    % So now open a composer window.
    timber_open_composer();

    set_blocal_var(0, "is_reply");

    insert("To: ");
    push_spot();
    insert("\nSubject: " + subj);
    insert("\nAttach-Enclosed: message/rfc822 \"forwarded message\"");
    insert("\n\n");

    if (file_status(expand_filename(timber_sig)) == 1) {
	insert("\n-- \n");
	insert_file(expand_filename(timber_sig));
	!if (bolp()) insert("\n");
    }

    insert("\006orwarded Message --------------------------------\n");

    insbuf(tbuf);
    fbuf = whatbuf();
    setbuf(tbuf);
    setbuf_info(getbuf_info & ~1); % clear mod bit to make delbuf silent
    delbuf(tbuf);
    setbuf(fbuf);
    !if (bolp())
        insert("\n");

    runhooks("timber_compose_hook");

    bob();
    recenter(1);
    pop_spot();

    setbuf_info( (getbuf_info() & ~0x303) | 0x20 );
}

%}}}
%{{{ timber_compose(): open a fresh composer buffer to write a new message

% Begin composition of a brand new message.
define timber_compose_given(to_list) {
    timber_open_composer();

    insert("To: " + to_list);
    if (to_list == "") push_spot();
    insert("\nSubject: ");
    if (to_list != "") push_spot();
    insert("\n\n");

    if (file_status(expand_filename(timber_sig)) == 1) {
	insert("\n-- \n");
	insert_file(expand_filename(timber_sig));
	!if (bolp()) insert("\n");
    }
    set_blocal_var(0, "is_reply");

    runhooks("timber_compose_hook");

    pop_spot();

    setbuf_info( (getbuf_info() & ~0x303) | 0x20 );
}

define timber_compose() {
    timber_compose_given("");
}

%}}}
%{{{ timber_open_mbox(): fetch mail and open `mbox' as a Timber buffer

% Create the first Timber buffer, pointing at `mbox'.
define timber_open_mbox() {
    runhooks("timber_hook");
    if (timber_acquire_lock(timber_inbox)) {
        timber_fetchmail();
        timber_open_folder(timber_inbox);
    } else {
	error(timber_inbox + " is locked by another Timber!");
    }
}

%}}}
%{{{ timber(): either start Timber or check for new mail

% The starting command: `M-x timber' either calls timber_open_mbox, if
% mbox is not already open in a buffer, or moves to the mbox folder and
% updates the mail in it.
define timber() {
    variable b = Sprintf("[T] %s", timber_inbox, 1);
    if (bufferp(b)) {
        sw2buf(b);
        timber_updatemail();
    } else
        timber_open_mbox();
}

%}}}
%{{{ timber_only() and friends: complex command line capability

% Hooks for invocation of Timber from the Jed command line.

% This sets timber-only mode, in which Jed quits when the last Timber-
% related buffer is closed.
define timber_set_only() {
    timber_is_only = 1;
}

% This causes timber_goto with a provided argument.
define timber_goto_given(folder) {
    if (timber_acquire_lock(folder)) {
        timber_open_folder(folder);
    } else {
	error(folder + " is locked by another Timber!");
    }
}

% An alternative starting command, `timber_only', designed to be
% invoked as `jed -f timber_only'.
define timber_only() {
    timber_set_only();
    timber();
}

%}}}

%{{{ Now load the user's .timberrc

% Now run the user's .timberrc file, wherein they can change the
% various variables and fiddle with things.
if (file_status(expand_filename("~/.timberrc")) == 1) {
    () = evalfile(expand_filename("~/.timberrc"));
}
%}}}
%{{{ Then set up all the script commands properly

% This variable may be reassigned in `.timberrc'. It gives
% the command used to send mail. This should have the following
% properties:
% (1) Not require further command line arguments (eg addresses)
% because it won't be given them. `sendmail -t' does this, by
% making Sendmail read the headers for its addresses.
% (2) Not fall over when it sees a dot at the beginning of a line.
% `sendmail -oi' does this.
% (3) Not produce anything on stdout/stderr that might be
% important, because we can't get it back. `sendmail -oem' makes
% Sendmail mail-bounce instead of printing errors.
% (4) Filter through the Timber attachment processing script
% before doing anything else. (This converts Attach: headers and
% Attach-Enclosed: headers into proper MIME encapsulation, and
% deals with Queue: headers.)
variable timber_sendmail = "perl " + dircat(timber_scripts_dir,"timbera.pl");

% This variable may be reassigned in `.timberrc'. It gives
% the full pathname of the program which fetches the user's new mail
% into a file specified on the command line.
variable timber_fetch_prog = "perl " + dircat(timber_scripts_dir,"timber.pl");

% This variable may be reassigned in `.timberrc'. It gives
% the full pathname of the program which sets up Timber buffers.
variable timber_enbuf_prog = "perl " + dircat(timber_scripts_dir,"timberb.pl");

% This variable may be reassigned in `.timberrc'. It gives
% the full pathname of the program which decodes MIME encodings.
variable timber_mime_prog = "perl " + dircat(timber_scripts_dir,"timberm.pl");

% This variable may be reassigned in `.timberrc'. It gives
% the full pathname of the program which adds line prefixes.
variable timber_prefix_prog = "perl " + dircat(timber_scripts_dir,"timbere.pl");

% This variable may be reassigned in `.timberrc'. It gives
% the full pathname of the program which looks up MIME types.
variable timber_mimetype_prog = "perl " + dircat(timber_scripts_dir,"timbert.pl");

% The directory containing Timber scripts.
#ifexists putenv
putenv("TIMBER_SCRIPTS=" + timber_scripts_dir);
#endif

%}}}

%{{{ Miscellaneous notes
%
% Crib to `Status:' headers.
%
% O means opened: the user has seen the existence of the message. All
% messages processed in a Timber folder should therefore have this flag
% added immediately, along with the entire Status: header if not already
% present.
%
% R means read: the user has seen the body of the message. This should
% be added when the user unfolds a message, if not already present.
%
% Pine adds X-Status: A when a message has been answered. We can't be
% bothered with this. It probably uses X-Status: D as well.

% Crib to sendmail usage.
%
% sendmail -oem will mail-bounce errors back to you rather than putting
%          them on stdout
% sendmail -t will read recipients from To: Cc: and Bcc: headers rather
%          than needing them on command line
% sendmail -oi will avoid [ret][.][ret] terminating messages
%
% Only header _needed_ is To/Cc/Bcc (at least one address). Subject is
% also considered good.
%
% Pine also puts in Date, From, Message-ID (all fabricated by sendmail
% if you can't be bothered), Mime-Version and Content-Type (we'll have
% to do this at some stage).

%}}}
