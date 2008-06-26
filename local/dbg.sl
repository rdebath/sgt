% Jed code that uses an asynchronous subprocess to arrange for the
% Jed cursor to track a debugger's steps through source files.

variable dbg_pid;
variable dbg_data;

define dbg_exit_fn(pid, flags, status) {
    if ((flags & 12) != 0) {
        exit_jed();
    }
}

define dbg_input_fn(pid, data) {
    variable c, line;
    variable lineno, colno, filename;
    variable final_lineno, final_colno, final_filename, final;
    variable i;

    dbg_data = dbg_data + data;
    final = 0;
    while (1) {
        c = is_substr(dbg_data, "\n");
        if (c == 0)
            break;
        line = substr(dbg_data, 1, c-1);
        dbg_data = substr(dbg_data, c+1, -1);
        i = sscanf(line, "%d:%d:%s", &lineno, &colno, &filename);
        if (i == 3) {
            final_lineno = lineno;
            final_colno = colno;
            final_filename = filename;
            final = 1;
        } else {
            error(sprintf("i=%d <%s>", i, line));
        }
    }
    if (final) {
        () = find_file(final_filename);
        setbuf_info(getbuf_info | 8);
        goto_line(lineno);
        () = goto_column_best_try(colno);
        emacs_recenter();
    }
}

define dbg_tracker(fd) {
    dbg_pid = open_process("sh", "-c", sprintf("cat <&%d", fd), 2);
    if (dbg_pid == -1)
        error("Unable to start subprocess reading from debugger");

    dbg_data = "";

    set_process(dbg_pid, "output", &dbg_input_fn);
    set_process(dbg_pid, "signal", &dbg_exit_fn);
}
