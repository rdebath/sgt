#include <stdio.h>
#include <stdlib.h>
#include <termcap.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv) {
    char buf[40] = "\0\0\0";	       /* safety */
    char *p, *q;
    struct termios t_old, t_new;
    int lines, cols;
    int fd;
    char *tty, *term;
    char tbuf[1024], tbuf2[1024];

    if ( (tty = ttyname (0)) == NULL ) {
	fprintf(stderr, "%s: stdin is not a terminal\n", argv[0]);
	return 0;
    }
    fd = open(tty, O_WRONLY);	       /* ensure we can write to it! */

    if ((term = getenv("TERM")) == NULL ||
	tgetent (tbuf, term) <= 0) {
	fprintf(stderr, "%s: unable to get termcap entry\n", argv[0]);
	return 1;
    }

    p = tbuf2;
    if ((q = tgetstr("ho", &p)) == NULL ||
	strcmp(q, "\033[H")) {
	fprintf(stderr, "%s: terminal type `%s' does not seem to be"
		" VT100-series\n", argv[0], term);
	return 0;
    }

    tcgetattr (0, &t_old);
    t_new = t_old;
    t_new.c_lflag &= ~ISIG & ~ICANON & ~ECHO;
    t_new.c_cc[VMIN] = 1;
    t_new.c_cc[VTIME] = 10;	       /* == 1 second I hope */
    tcsetattr (0, TCSADRAIN, &t_new);

    write (fd, "\0337\033[999;999H\033[6n\0338", 18);
    close (fd);			       /* be tidy */

    p = buf;
    while (read (0, p, 1) == 1 && *p++ != 'R');

    tcsetattr (0, TCSADRAIN, &t_old);

    if (p == buf || p[-1] != 'R') {
	fprintf(stderr, "Unable to parse return string\n");
	return 1;
    }

    if (sscanf(buf, "\033[%d;%dR", &lines, &cols) == 2) {
	struct winsize ws;

	fprintf(stderr, "Screen size = %d columns by %d rows\n", cols, lines);
	if (!ioctl (0, TIOCGWINSZ, &ws)) {
	    ws.ws_row = lines;
	    ws.ws_col = cols;
	    if (!ioctl (0, TIOCSWINSZ, &ws))
		return 0;
	    else {
		perror ("stdin: ioctl TIOCSWINSZ");
		return 1;
	    }
	} else {
	    perror ("stdin: ioctl TIOCGWINSZ");
	    return 1;
	}
    } else {
	fputs("Unable to parse return string\n", stderr);
	return 1;
    }
}
