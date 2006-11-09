/*
 * Prefix each line read from stdin by the result of
 * gettimeofday(2) when that line began coming in.
 * 
 * I originally wrote this to use as a post-processor on the output
 * from strace(1), before discovering `strace -ttt' which has an
 * identical effect. However, it might come in useful for something
 * other than strace, so I'm keeping it just in case.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

int main(void)
{
    char buf[4096];
    struct timeval tv;
    char timestr[40];
    int linestart = 1;
    int ret, i, j;

    while (1) {
	ret = read(0, buf, sizeof(buf));
	if (ret < 0) {
	    perror("stopwatch: standard input");
	    return 1;
	}
	if (ret == 0)
	    return 0;

	if (gettimeofday(&tv, NULL) < 0) {
	    perror("stopwatch: gettimeofday");
	    return 1;
	}

	sprintf(timestr, "%10d.%06d ", tv.tv_sec, tv.tv_usec);

	if (linestart) {
	    write(1, timestr, strlen(timestr));
	    linestart = 0;
	}

	i = j = 0;
	while (j < ret) {
	    char c = buf[j++];
	    if (c == '\n') {
		if (linestart) {
		    write(1, timestr, strlen(timestr));
		    linestart = 0;
		}
		write(1, buf+i, j-i);
		i = j;
		linestart = 1;
	    }
	}
	if (i < ret) {
	    if (linestart) {
		write(1, timestr, strlen(timestr));
		linestart = 0;
	    }
	    write(1, buf+i, ret-i);
	}
    }
}
