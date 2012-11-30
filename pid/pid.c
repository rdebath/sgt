/*
 * pid - find the pid of a process given its command name
 *
 * Same basic idea as Debian's "pidof", in that you type 'pid command'
 * and it finds a process running that command and gives you the pid;
 * but souped up with various pragmatic features such as recognising
 * well known interpreters (so you can search for, say, 'pid
 * script.sh' as well as 'pid bash' and have it do what you meant).
 *
 * Currently tested only on Linux using /proc directly, but I've tried
 * to set it up so that the logic of what processes to choose is
 * separated from the mechanism used to iterate over processes and
 * find their command lines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#define lenof(x) (sizeof((x))/sizeof(*(x)))

#define PIDMAX 32768

/* ----------------------------------------------------------------------
 * General-purpose code for storing a set of process ids, testing
 * membership, and iterating over them. Since pids have a very limited
 * range, we just do this as a giant bitmask.
 */

#define WORDBITS 32

struct pidset {
    unsigned long procbits[PIDMAX/WORDBITS];
    int next;
};

static void pidset_init(struct pidset *p)
{
    int i;
    for (i = 0; i < (int)lenof(p->procbits); i++)
        p->procbits[i] = 0L;
}

static void pidset_add(struct pidset *p, int pid)
{
    assert(pid >= 0 && pid < PIDMAX);
    p->procbits[pid / WORDBITS] |= 1 << (pid % WORDBITS);
}

static int pidset_in(const struct pidset *p, int pid)
{
    assert(pid >= 0 && pid < PIDMAX);
    return (p->procbits[pid / WORDBITS] & (1 << (pid % WORDBITS))) != 0;
}

static int pidset_size(const struct pidset *p)
{
    int word, count;

    count = 0;
    for (word = 0; word < (int)lenof(p->procbits); word++) {
        unsigned long mask = p->procbits[word];
        while (mask > 0) {
            count += (mask & 1);
            mask >>= 1;
        }
    }

    return count;
}

static int pidset_step(struct pidset *p)
{
    int word = p->next / WORDBITS;
    int bit = p->next % WORDBITS;
    while (word < (int)lenof(p->procbits) && p->procbits[word] >> bit == 0) {
        word++;
        bit = 0;
        p->next = WORDBITS * word + bit;
    }

    if (word >= (int)lenof(p->procbits))
        return -1;

    while (!((p->procbits[word] >> bit) & 1)) {
        bit++;
        p->next = WORDBITS * word + bit;
    }

    assert(bit < WORDBITS);
    return p->next++;
}

static int pidset_first(struct pidset *p)
{
    p->next = 0;
    return pidset_step(p);
}

static int pidset_next(struct pidset *p)
{
    return pidset_step(p);
}

/* ----------------------------------------------------------------------
 * Code to scan the list of processes and retrieve all the information
 * we'll want about each one. This may in future be conditional on the
 * OS's local mechanism for finding that information (i.e. if we want
 * to run on kernels that don't provide Linux-style /proc).
 */

struct procdata {
    int pid, ppid, uid;
    int argc;
    const char *const *argv;
    const char *exe;
};
static struct procdata *procs[PIDMAX];

static char *get_contents(const char *filename, int *returned_len)
{
    int len;
    char *buf = NULL;
    int bufsize = 0;

    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return NULL;

    len = 0;
    while (1) {
        int readret;

        if (len >= bufsize) {
            bufsize = len * 5 / 4 + 4096;
            buf = realloc(buf, bufsize);
            if (!buf) {
                fprintf(stderr, "pid: out of memory\n");
                exit(1);
            }
        }

        readret = fread(buf + len, 1, bufsize - len, fp);
        if (readret < 0) {
            fclose(fp);
            free(buf);
            return NULL;               /* I/O error */
        } else if (readret == 0) {
            fclose(fp);
            if (returned_len)
                *returned_len = len;
            buf = realloc(buf, len + 1);
            buf[len] = '\0';
            return buf;
        } else {
            len += readret;
        }
    }
}

static char *get_link_dest(const char *filename)
{
    char *buf;
    int bufsize;
    ssize_t ret;

    buf = NULL;
    bufsize = 0;

    while (1) {
        bufsize = bufsize * 5 / 4 + 1024;
        buf = realloc(buf, bufsize);
        if (!buf) {
            fprintf(stderr, "pid: out of memory\n");
            exit(1);
        }

        ret = readlink(filename, buf, (size_t)bufsize);
        if (ret < 0) {
            free(buf);
            return NULL;               /* I/O error */
        } else if (ret < bufsize) {
            /*
             * Success! We've read the full link text.
             */
            buf = realloc(buf, ret+1);
            buf[ret] = '\0';
            return buf;
        } else {
            /* Overflow. Go round again. */
        }
    }
}

static struct pidset get_processes(void)
{
    struct dirent *de;
    struct pidset ret;
    DIR *d;

    pidset_init(&ret);

    d = opendir("/proc");
    if (!d) {
        perror("/proc: open\n");
        exit(1);
    }
    while ((de = readdir(d)) != NULL) {
        int pid;
        char *cmdline, *status, *exe;
        int cmdlinelen;
        const char **argv;
        char filename[256];
        struct procdata *proc;

        const char *name = de->d_name;
        if (name[strspn(name, "0123456789")])
            continue;

        /*
         * The filename is numeric, i.e. we've found a pid. Try to
         * retrieve all the information we want about it.
         *
         * We expect this will fail every so often for random reasons,
         * e.g. if the pid has disappeared between us fetching a list
         * of them and trying to read their command lines. In that
         * situation, we won't bother reporting errors: we'll just
         * drop this pid and silently move on to the next one.
         */
        pid = atoi(name);
        assert(pid >= 0 && pid < PIDMAX);

        sprintf(filename, "/proc/%d/cmdline", pid);
        if ((cmdline = get_contents(filename, &cmdlinelen)) == NULL)
            continue;

        sprintf(filename, "/proc/%d/status", pid);
        if ((status = get_contents(filename, NULL)) == NULL) {
            free(cmdline);
            continue;
        }

        sprintf(filename, "/proc/%d/exe", pid);
        exe = get_link_dest(filename);
        /* This may fail, if the process isn't ours, but we continue
         * anyway. */

        /*
         * Now we've got all our raw data out of /proc. Process it
         * into the internal representation we're going to use in the
         * process-selection logic.
         */
        proc = (struct procdata *)malloc(sizeof(struct procdata));
        if (!proc) {
            fprintf(stderr, "pid: out of memory\n");
            exit(1);
        }
        proc->pid = pid;
        proc->exe = exe;

        /*
         * cmdline contains a list of NUL-terminated strings. Scan
         * them to get the argv pointers.
         */
        {
            const char *p;
            int nargs;

            /* Count the arguments. */
            nargs = 0;
            for (p = cmdline; p < cmdline + cmdlinelen; p += strlen(p)+1)
                nargs++;

            /* Allocate space for the pointers. */
            argv = (const char **)malloc((nargs+1) * sizeof(char *));
            proc->argv = argv;
            if (!argv) {
                fprintf(stderr, "pid: out of memory\n");
                exit(1);
            }

            /* Store the pointers. */
            proc->argc = 0;
            for (p = cmdline; p < cmdline + cmdlinelen; p += strlen(p)+1)
                argv[proc->argc++] = p;

            /* Trailing NULL to match standard argv lists, just in case. */
            assert(proc->argc == nargs);
            argv[proc->argc] = NULL;
        }

        /*
         * Scan status for the uid and the parent pid. This file
         * contains a list of \n-terminated lines of text.
         */
        {
            const char *p;
            int got_ppid = 0, got_uid = 0;

            p = status;
            while (p && *p) {
                if (!got_ppid && sscanf(p, "PPid: %d", &proc->ppid) == 1)
                    got_ppid = 1;
                if (!got_uid && sscanf(p, "Uid: %*d %d", &proc->uid) == 1)
                    got_uid = 1;

                /*
                 * Advance to next line.
                 */
                p = strchr(p, '\n');
                if (p) p++;
            }

            if (!got_uid || !got_ppid) { /* arrgh, abort everything so far */
                free(cmdline);
                free(exe);
                free(status);
                free(argv);
                free(proc);
                continue;
            }
        }

        /*
         * If we get here, we've got everything we need. Add the
         * process to the list of things we can usefully work
         * with.
         */
        procs[pid] = proc;
        pidset_add(&ret, pid);
    }
    closedir(d);

    return ret;
}

static const struct procdata *get_proc(int pid)
{
    assert(pid >= 0 && pid < PIDMAX);
    assert(procs[pid]);
    return procs[pid];
}

/* ----------------------------------------------------------------------
 * Logic to pick out the set of processes we care about.
 */

static int is_an_interpreter(const char *basename, const char **stop_opt)
{
    if (!strcmp(basename, "perl") ||
        !strcmp(basename, "ruby")) {
        *stop_opt = "-e";
        return 1;
    }
    if (!strcmp(basename, "python") ||
        !strcmp(basename, "bash") ||
        !strcmp(basename, "sh") ||
        !strcmp(basename, "dash")) {
        *stop_opt = "-c";
        return 1;
    }
    if (!strcmp(basename, "rep") ||
        !strcmp(basename, "lua") ||
        !strcmp(basename, "java")) {
        *stop_opt = NULL;
        return 1;
    }
    return 0;
}

static const char *find_basename(const char *path)
{
    const char *ret = path;
    const char *p;

    while (1) {
        p = ret + strcspn(ret, "/");
        if (*p) {
            ret = p+1;
        } else {
            return ret;
        }
    }
}

static int find_command(int pid_argc, const char *const *pid_argv,
                        const char *cmd)
{
    const char *base, *stop_opt;

    base = pid_argv[0];
    if (*base == '-')
        base++;                   /* skip indicator of login shells */
    base = find_basename(base);

    if (!strcmp(base, cmd)) {
        /*
         * argv[0] matches the supplied command name.
         */
        return 0;
    } else if (is_an_interpreter(base, &stop_opt)) {
        /*
         * argv[0] is an interpreter program of some kind. Look
         * along its command line for the program it's running,
         * and see if _that_ matches the command name.
         */
        int i = 1;
        while (i < pid_argc && pid_argv[i][0] == '-') {
            /*
             * Skip interpreter options, unless they're things which
             * make the next non-option argument not a script name
             * (e.g. sh -c, perl -e).
             */
            if (stop_opt && !strncmp(pid_argv[i], stop_opt, strlen(stop_opt)))
                return -1;             /* no match */
            i++;
        }
        if (i < pid_argc && !strcmp(find_basename(pid_argv[i]), cmd))
            return i;
    }
    return -1;                         /* no match */
}

static int strnullcmp(const char *a, const char *b)
{
    /*
     * Like strcmp, but cope with NULL inputs by making them compare
     * identical to each other and before any non-null string.
     */
    if (!a || !b)
        return (b != 0) - (a != 0);
    else
        return strcmp(a, b);
}

static int argcmp(const char *const *a, const char *const *b)
{
    while (*a && *b) {
        int ret = strcmp(*a, *b);
        if (ret)
            return ret;
        a++;
        b++;
    }

    return (*b != NULL) - (*a != NULL);
}

static struct pidset filter_out_self(struct pidset in)
{
    /*
     * Discard our own pid from a set. (Generally we won't want to
     * return ourself from any search.)
     */
    struct pidset ret;
    int pid;
    int our_pid = getpid();

    pidset_init(&ret);
    for (pid = pidset_first(&in); pid >= 0; pid = pidset_next(&in)) {
        if (pid != our_pid)
            pidset_add(&ret, pid);
    }
    return ret;
}

static struct pidset filter_by_uid(struct pidset in, int uid)
{
    /*
     * Return only those processes with a given uid.
     */
    struct pidset ret;
    int pid;

    pidset_init(&ret);
    for (pid = pidset_first(&in); pid >= 0; pid = pidset_next(&in)) {
        const struct procdata *proc = get_proc(pid);
        if (proc->uid == uid)
            pidset_add(&ret, pid);
    }
    return ret;
}

static struct pidset filter_by_command(struct pidset in, const char **words)
{
    /*
     * Look for processes matching the user-supplied command name and
     * subsequent arguments.
     */
    struct pidset ret;
    int pid;

    pidset_init(&ret);
    for (pid = pidset_first(&in); pid >= 0; pid = pidset_next(&in)) {
        const struct procdata *proc = get_proc(pid);
        int i, j;

        if (!proc->argv || proc->argc < 1)
            goto no_match;

        /* Find the command, whether it's a binary or a script. */
        i = find_command(proc->argc, proc->argv, words[0]);
        if (i < 0)
            goto no_match;

        /* Now check that subsequent arguments match. */
        for (j = 1; words[j]; j++)
            if (!proc->argv[i+j] || strcmp(proc->argv[i+j], words[j]))
                goto no_match;

        /* If we get here, we have a match! */
        pidset_add(&ret, pid);

      no_match:;
    }
    return ret;
}

static struct pidset filter_out_forks(struct pidset in)
{
    /*
     * Discard any process whose parent is also in our remaining match
     * set and looks sufficiently like it for us to decide this one's
     * an uninteresting fork (e.g. of a shell script executing a
     * complex pipeline).
     */
    struct pidset ret;
    int pid;

    pidset_init(&ret);
    for (pid = pidset_first(&in); pid >= 0; pid = pidset_next(&in)) {
        const struct procdata *proc = get_proc(pid);

        if (pidset_in(&in, proc->ppid)) {
            /* The parent is in our set too. Is it similar? */
            const struct procdata *parent = get_proc(proc->ppid);
            if (!strnullcmp(parent->exe, proc->exe) &&
                !argcmp(parent->argv, proc->argv)) {
                /* Yes; don't list it. */
                continue;
            }
        }

        pidset_add(&ret, pid);
    }
    return ret;
}

/* ----------------------------------------------------------------------
 * Main program.
 */

const char usagemsg[] =
    "usage: pid [options] <search-cmd> [<search-arg>...]\n"
    "where: -a                 report all matching pids, not just one\n"
    "       -U                 report pids of any user, not just ours\n"
    " also: pid --version      report version number\n"
    "       pid --help         display this help text\n"
    "       pid --licence      display the (MIT) licence text\n"
    ;

void usage(void) {
    fputs(usagemsg, stdout);
}

const char licencemsg[] =
    "pid is copyright 2012 Simon Tatham.\n"
    "\n"
    "Permission is hereby granted, free of charge, to any person\n"
    "obtaining a copy of this software and associated documentation files\n"
    "(the \"Software\"), to deal in the Software without restriction,\n"
    "including without limitation the rights to use, copy, modify, merge,\n"
    "publish, distribute, sublicense, and/or sell copies of the Software,\n"
    "and to permit persons to whom the Software is furnished to do so,\n"
    "subject to the following conditions:\n"
    "\n"
    "The above copyright notice and this permission notice shall be\n"
    "included in all copies or substantial portions of the Software.\n"
    "\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n"
    "EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n"
    "MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND\n"
    "NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS\n"
    "BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n"
    "ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN\n"
    "CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
    "SOFTWARE.\n"
    ;

void licence(void) {
    fputs(licencemsg, stdout);
}

void version(void) {
#define SVN_REV "$Revision$"
    char rev[sizeof(SVN_REV)];
    char *p, *q;

    strcpy(rev, SVN_REV);

    for (p = rev; *p && *p != ':'; p++);
    if (*p) {
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        for (q = p; *q && *q != '$'; q++);
        if (*q) *q = '\0';
        printf("pid revision %s\n", p);
    } else {
        printf("pid: unknown version\n");
    }
}

int main(int argc, char **argv)
{
    const char **searchwords;
    int nsearchwords;
    int all = 0, all_uids = 0;
    int doing_opts = 1;

    /*
     * Allocate enough space in 'searchwords' that we could shovel the
     * whole of our argv into it if we had to. Then we won't have to
     * worry about it later.
     */
    searchwords = (const char **)malloc((argc+1) * sizeof(const char *));
    nsearchwords = 0;

    /*
     * Parse the command line.
     */
    while (--argc > 0) {
        char *p = *++argv;
        if (doing_opts && *p == '-') {
            if (!strcmp(p, "-a") || !strcmp(p, "--all")) {
                all = 1;
            } else if (!strcmp(p, "-U") || !strcmp(p, "--all-uids")) {
                all_uids = 1;
            } else if (!strcmp(p, "--version")) {
                version();
                return 0;
            } else if (!strcmp(p, "--help")) {
                usage();
                return 0;
            } else if (!strcmp(p, "--licence") || !strcmp(p, "--license")) {
                licence();
                return 0;
            } else if (!strcmp(p, "--")) {
                doing_opts = 0;
            } else {
                fprintf(stderr, "pid: unrecognised option '%s'\n", p);
                return 1;
            }
        } else {
            searchwords[nsearchwords++] = p;
            doing_opts = 0; /* further optionlike args become search terms */
        }
    }

    if (!nsearchwords) {
        fprintf(stderr, "pid: expected a command to search for; "
                "type 'pid --help' for help\n");
        return 1;
    }
    searchwords[nsearchwords] = NULL;  /* terminate list */

    {
        struct pidset procs;
        int uid, pid, npids;
        /*
         * Construct our list of processes.
         */
        procs = get_processes();
        uid = getuid();
        if (uid > 0 && !all_uids)
            procs = filter_by_uid(procs, uid);
        procs = filter_out_self(procs);
        procs = filter_by_command(procs, searchwords);
        if (!all)
            procs = filter_out_forks(procs);

        /*
         * Output.
         */
        npids = pidset_size(&procs);
        if (npids == 0) {
            printf("NONE\n");
        } else if (all) {
            const char *sep = "";
            for (pid = pidset_first(&procs); pid >= 0;
                 pid = pidset_next(&procs)) {
                printf("%s%d", sep, pid);
                sep = " ";
            }
            putchar('\n');
        } else {
            if (npids == 1) {
                printf("%d\n", pidset_first(&procs));
            } else {
                printf("MULTIPLE\n");
            }
        }
    }

    return 0;
}
