#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/vt.h>

#define lenof(x) ( sizeof(x) / sizeof(*(x)) )

int main(void)
{
    struct vt_stat stat;
    static const char *const filenames[] = {
	"/dev/tty",
	"/dev/tty0",
	"/dev/console"
    };
    char errbuf[4096], *errpos = errbuf;
    int i, fd;

    for (i = 0; i < lenof(filenames); i++) {
	fd = open(filenames[i], O_RDONLY);

	if (fd < 0) {
	    errpos += sprintf(errpos, "%s: open: %.256s\n",
			      filenames[i], strerror(errno));
	    continue;
	}
	if (ioctl(fd, VT_GETSTATE, &stat) < 0) {
	    errpos += sprintf(errpos, "%s: ioctl(VT_GETSTATE): %.256s\n",
			      filenames[i], strerror(errno));
	    close(fd);
	    continue;
	}
	close(fd);
	printf("%d\n", stat.v_active);
	return 0;
    }

    fputs(errbuf, stderr);
    return 1;
}
