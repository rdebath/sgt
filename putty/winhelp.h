/*
 * winhelp.h - define Windows Help context names for the controls
 * in the PuTTY config box.
 */

#define HELPCTX(x) P(WINHELP_CTX_ ## x)

#define WINHELP_CTX_no_help NULL

#define WINHELP_CTX_session_hostname "session.hostname"
#define WINHELP_CTX_session_saved "session.saved"
#define WINHELP_CTX_session_coe "session.coe"
#define WINHELP_CTX_logging_main "logging.main"
#define WINHELP_CTX_logging_filename "logging.filename"
#define WINHELP_CTX_logging_exists "logging.exists"
#define WINHELP_CTX_logging_ssh_omit_password "logging.ssh.omitpassword"
#define WINHELP_CTX_logging_ssh_omit_data "logging.ssh.omitdata"
#define WINHELP_CTX_keyboard_backspace "keyboard.backspace"
#define WINHELP_CTX_keyboard_homeend "keyboard.homeend"
#define WINHELP_CTX_keyboard_funkeys "keyboard.funkeys"
#define WINHELP_CTX_keyboard_appkeypad "keyboard.appkeypad"
#define WINHELP_CTX_keyboard_appcursor "keyboard.appcursor"
#define WINHELP_CTX_keyboard_nethack "keyboard.nethack"
#define WINHELP_CTX_keyboard_compose "keyboard.compose"
#define WINHELP_CTX_keyboard_ctrlalt "keyboard.ctrlalt"
#define WINHELP_CTX_features_application "features.application"
#define WINHELP_CTX_features_mouse "features.mouse"
#define WINHELP_CTX_features_resize "features.resize"
#define WINHELP_CTX_features_altscreen "features.altscreen"
#define WINHELP_CTX_features_retitle "features.retitle"
#define WINHELP_CTX_features_qtitle "features.qtitle"
#define WINHELP_CTX_features_dbackspace "features.dbackspace"
#define WINHELP_CTX_features_charset "features.charset"
#define WINHELP_CTX_features_arabicshaping "features.arabicshaping"
#define WINHELP_CTX_features_bidi "features.bidi"
#define WINHELP_CTX_terminal_autowrap "terminal.autowrap"
#define WINHELP_CTX_terminal_decom "terminal.decom"
#define WINHELP_CTX_terminal_lfhascr "terminal.lfhascr"
#define WINHELP_CTX_terminal_bce "terminal.bce"
#define WINHELP_CTX_terminal_blink "terminal.blink"
#define WINHELP_CTX_terminal_answerback "terminal.answerback"
#define WINHELP_CTX_terminal_localecho "terminal.localecho"
#define WINHELP_CTX_terminal_localedit "terminal.localedit"
#define WINHELP_CTX_terminal_printing "terminal.printing"
#define WINHELP_CTX_bell_style "bell.style"
#define WINHELP_CTX_bell_taskbar "bell.taskbar"
#define WINHELP_CTX_bell_overload "bell.overload"
#define WINHELP_CTX_window_size "window.size"
#define WINHELP_CTX_window_resize "window.resize"
#define WINHELP_CTX_window_scrollback "window.scrollback"
#define WINHELP_CTX_window_erased "window.erased"
#define WINHELP_CTX_behaviour_closewarn "behaviour.closewarn"
#define WINHELP_CTX_behaviour_altf4 "behaviour.altf4"
#define WINHELP_CTX_behaviour_altspace "behaviour.altspace"
#define WINHELP_CTX_behaviour_altonly "behaviour.altonly"
#define WINHELP_CTX_behaviour_alwaysontop "behaviour.alwaysontop"
#define WINHELP_CTX_behaviour_altenter "behaviour.altenter"
#define WINHELP_CTX_appearance_cursor "appearance.cursor"
#define WINHELP_CTX_appearance_font "appearance.font"
#define WINHELP_CTX_appearance_title "appearance.title"
#define WINHELP_CTX_appearance_hidemouse "appearance.hidemouse"
#define WINHELP_CTX_appearance_border "appearance.border"
#define WINHELP_CTX_connection_termtype "connection.termtype"
#define WINHELP_CTX_connection_termspeed "connection.termspeed"
#define WINHELP_CTX_connection_username "connection.username"
#define WINHELP_CTX_connection_keepalive "connection.keepalive"
#define WINHELP_CTX_connection_nodelay "connection.nodelay"
#define WINHELP_CTX_connection_tcpkeepalive "connection.tcpkeepalive"
#define WINHELP_CTX_proxy_type "proxy.type"
#define WINHELP_CTX_proxy_main "proxy.main"
#define WINHELP_CTX_proxy_exclude "proxy.exclude"
#define WINHELP_CTX_proxy_dns "proxy.dns"
#define WINHELP_CTX_proxy_auth "proxy.auth"
#define WINHELP_CTX_proxy_command "proxy.command"
#define WINHELP_CTX_proxy_socksver "proxy.socksver"
#define WINHELP_CTX_telnet_environ "telnet.environ"
#define WINHELP_CTX_telnet_oldenviron "telnet.oldenviron"
#define WINHELP_CTX_telnet_passive "telnet.passive"
#define WINHELP_CTX_telnet_specialkeys "telnet.specialkeys"
#define WINHELP_CTX_telnet_newline "telnet.newline"
#define WINHELP_CTX_rlogin_localuser "rlogin.localuser"
#define WINHELP_CTX_ssh_nopty "ssh.nopty"
#define WINHELP_CTX_ssh_ciphers "ssh.ciphers"
#define WINHELP_CTX_ssh_protocol "ssh.protocol"
#define WINHELP_CTX_ssh_command "ssh.command"
#define WINHELP_CTX_ssh_compress "ssh.compress"
#define WINHELP_CTX_ssh_auth_privkey "ssh.auth.privkey"
#define WINHELP_CTX_ssh_auth_agentfwd "ssh.auth.agentfwd"
#define WINHELP_CTX_ssh_auth_changeuser "ssh.auth.changeuser"
#define WINHELP_CTX_ssh_auth_tis "ssh.auth.tis"
#define WINHELP_CTX_ssh_auth_ki "ssh.auth.ki"
#define WINHELP_CTX_selection_buttons "selection.buttons"
#define WINHELP_CTX_selection_shiftdrag "selection.shiftdrag"
#define WINHELP_CTX_selection_rect "selection.rect"
#define WINHELP_CTX_selection_charclasses "selection.charclasses"
#define WINHELP_CTX_selection_linedraw "selection.linedraw"
#define WINHELP_CTX_selection_rtf "selection.rtf"
#define WINHELP_CTX_colours_bold "colours.bold"
#define WINHELP_CTX_colours_system "colours.system"
#define WINHELP_CTX_colours_logpal "colours.logpal"
#define WINHELP_CTX_colours_config "colours.config"
#define WINHELP_CTX_translation_codepage "translation.codepage"
#define WINHELP_CTX_translation_cyrillic "translation.cyrillic"
#define WINHELP_CTX_translation_linedraw "translation.linedraw"
#define WINHELP_CTX_ssh_tunnels_x11 "ssh.tunnels.x11"
#define WINHELP_CTX_ssh_tunnels_x11auth "ssh.tunnels.x11auth"
#define WINHELP_CTX_ssh_tunnels_portfwd "ssh.tunnels.portfwd"
#define WINHELP_CTX_ssh_tunnels_portfwd_localhost "ssh.tunnels.portfwd.localhost"
#define WINHELP_CTX_ssh_bugs_ignore1 "ssh.bugs.ignore1"
#define WINHELP_CTX_ssh_bugs_plainpw1 "ssh.bugs.plainpw1"
#define WINHELP_CTX_ssh_bugs_rsa1 "ssh.bugs.rsa1"
#define WINHELP_CTX_ssh_bugs_hmac2 "ssh.bugs.hmac2"
#define WINHELP_CTX_ssh_bugs_derivekey2 "ssh.bugs.derivekey2"
#define WINHELP_CTX_ssh_bugs_rsapad2 "ssh.bugs.rsapad2"
#define WINHELP_CTX_ssh_bugs_dhgex2 "ssh.bugs.dhgex2"
#define WINHELP_CTX_ssh_bugs_pksessid2 "ssh.bugs.pksessid2"
