#include <stdio.h>
#include <stdlib.h>

struct dumpstate {
    int lower;                         /* display in lower case? */
    int squash;                        /* coalesce repeated lines? */
    int ascii;                         /* display ASCII on RHS of hex? */
    long offset;                       /* offset at which to start new line */
    long start;                        /* add this to all displayed offsets */
    long linewidth;                    /* bytes to dump per display line */
};

void dump (struct dumpstate state, FILE *fp) {   
    long addr=0;
    int i, j;
    unsigned char *buffer;
    char *prl, *oldprl;
    char *fmt, *hex;
    int etcflag = 0;
    int byteoffset;

    if (state.lower) {
        hex = "0123456789abcdef";
        fmt = "%08lx  %s\n";
    } else {
        hex = "0123456789ABCDEF";
        fmt = "%08lX  %s\n";
    }

    buffer = malloc (state.linewidth);
    prl = malloc (state.linewidth*4+17); /* Should be enough :) */
    oldprl = malloc (state.linewidth*4+17);

    if (!buffer || !prl || !oldprl) {
	fprintf (stderr, "Out of memory.\n");
	exit (1);
    }

    if (state.offset)
	byteoffset = state.linewidth - state.offset;
    else
	byteoffset = 0;

    while (1) {
        i = fread(buffer + byteoffset, 1, state.linewidth - byteoffset, fp);
        if (i <= 0)                    /* finished reading data */
            break;

	if (i) {
            i += byteoffset;

	    memset(prl, ' ', state.linewidth*4+17);

	    for (j = byteoffset; j < i; j++) {
		prl[j*3]=hex[buffer[j]/16];
		prl[j*3+1]=hex[buffer[j]%16];
	    }

	    if (state.ascii) {
		for (j = byteoffset; j < i; j++) {
		    if (buffer[j] >= 32 && buffer[j] <= 126)
			prl[j+state.linewidth*3+1]=buffer[j];
		    else
			prl[j+state.linewidth*3+1]='.';
		}
		prl[i+state.linewidth*3+1]='\0';
	    } else
		prl[3*i-1] = '\0';

	    if (state.squash && !strcmp(oldprl, prl) && !feof(fp)) {
		if (!etcflag) {
		    etcflag = 1;
		    printf("etc...\n");
		}
	    } else {
		etcflag = 0;
		printf (fmt, addr + state.start, prl);
		strcpy (oldprl, prl);
	    }
	    addr += state.linewidth - byteoffset;
	}
	byteoffset = 0;
    }
    free (buffer);
}

void help(void) {
    static char *helptext[] = {
	"usage: dump [-afhl] [-o offset] [-s start] [-w width]"
	    " [file [file...]]\n",
	"       -a disables ASCII-mode display\n",
	"       -f forces full display (disables `etc...' lines)\n",
	"       -h displays this text\n",
	"       -l displays hex numbers in lower case\n",
	"       -o ensures that a new line starts <offset> bytes into"
	    " the file\n",
	"       -s adds <start> to all the displayed offsets\n",
	"       -w chooses how many bytes to dump per display line\n",
	NULL
    };
    char **p = helptext;

    while (*p)
	fputs (*p++, stderr);
}

int main(int argc, char **argv) {
    FILE *fp;
    int i, j;
    int flag;
    char **names;
    struct dumpstate state;

    flag = 0;
    names = malloc(argc*sizeof(*names));
    if (!names) {
	perror("malloc");
	return 1;
    }

    state.lower = 0;
    state.squash = 1;
    state.ascii = 1;
    state.offset = 0;
    state.start = 0;
    state.linewidth = 16;

    j = 0;
    while (--argc > 0) {
	char c, *v, *p = *++argv;
        if (*p == '-') {
	    if (!strcmp(p, "--help")) {
		help();
		return 0;
	    }
	    p++;
	    while ( (c = *p++) ) switch (c) {
	      case 'l':		       /* lower case */
		state.lower = 1;
		break;
	      case 'f':		       /* force full display */
		state.squash = 0;
		break;
	      case 'a':		       /* disable ascii display */
		state.ascii = 0;
		break;
	      case 'h':		       /* help text */
	      case '?':
		help();
		return 0;
	      case 'o':		       /* options with arguments */
	      case 's':
	      case 'w':
		if (*p)
		    v = p, p = "";
		else if (--argc > 0)
		    v = *++argv;
		else {
		    fprintf(stderr,
			    "dump: option `-%c' requires an argument\n", c);
		    return 1;
		}
		switch (c) {
		  case 'o':
		    state.offset = strtoul(v,NULL,0);
		    break;
		  case 's':
		    state.start = strtoul(v,NULL,0);
		    break;
		  case 'w':
		    state.linewidth = strtoul(v,NULL,0);
		    break;
		}
		break;
	      default:
		fprintf(stderr, "dump: unrecognised option `-%c'\n", c);
		help();
		return 1;
	    }
	} else
	    names[j++] = p;
    }

    state.offset %= state.linewidth;

    for (i = 0; i < j; i++) {
	flag=1;
	fp = fopen (names[i], "r");
	if (fp == NULL)
	    printf ("dump: %s: unable to open\n", names[i]);
	else {
	    dump (state, fp);
	    fclose (fp);
	}
    }
    if (!flag)
	dump (state, stdin);

    return 0;
}	    
