/*
 * rawmode.c: Implementation of rawmode.h.
 */

#include <errno.h>

#include <termios.h>

#include "rawmode.h"

int raw_mode(int fd, const char **syscall, struct termios *termios)
{
    struct termios opts;
    int ret;

    ret = tcgetattr(fd, &opts);
    if (ret < 0) {
	if (syscall)
	    *syscall = "tcgetattr";
	return ret;
    }

    if (termios)
	*termios = opts;	       /* structure copy */

    opts.c_cflag |= CLOCAL | CREAD;
    opts.c_cflag &= ~PARENB;
    opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG
#ifdef XCASE
		      | XCASE
#endif
#ifdef IEXTEN
		      | IEXTEN
#endif
		      );
    opts.c_iflag &= ~(IXON | IXOFF | ISTRIP | IGNCR | INLCR | ICRNL | INPCK
#ifdef IUCLC
		      | IUCLC
#endif
		      );
    opts.c_oflag &= ~(OPOST
#ifdef OLCUC
		      | OLCUC
#endif
#ifdef ONLCR
		      | ONLCR
#endif
#ifdef OCRNL
		      | OCRNL
#endif
#ifdef ONOCR
		      | ONOCR
#endif
#ifdef ONLRET
		      | ONLRET
#endif
#ifdef XTABS
		      | XTABS
#endif
		      );
    opts.c_cc[VMIN] = 1;
    opts.c_cc[VTIME] = 0;

    ret = tcsetattr(fd, TCSANOW, &opts);
    if (ret < 0) {
	if (syscall)
	    *syscall = "tcgetattr";
	return ret;
    }

    return 0;
}
