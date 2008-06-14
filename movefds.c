/*
 * movefds.c: Implementation of movefds.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <unistd.h>

#include "movefds.h"
#include "malloc.h"

int move_fds(int base, int *map, int maplen, const char **syscall)
{
    int i, ret, *srccount, *tmpfds, ntmpfds;

    /*
     * Keep a count of how many times each fd in the range is used
     * as the source of a dup.
     */
    srccount = snewn(maplen, int);
    for (i = 0; i < maplen; i++)
	srccount[i] = 0;
    for (i = 0; i < maplen; i++)
	if (map[i] >= base && map[i] < base+maplen)
	    srccount[map[i]-base]++;

    /*
     * We'll set srccount[i]=-1 to indicate that an fd is now in
     * its correct final state and needs no further action.
     * Special case: start by setting that for any fd which is
     * actually mapped to itself.
     */
    for (i = 0; i < maplen; i++)
	if (map[i] == base+i)
	    srccount[i] = -1;

    /*
     * Allocate space for storing temporary fds in emergencies.
     */
    tmpfds = snewn(maplen, int);
    ntmpfds = 0;

    /*
     * Now repeatedly look for fds with source counts of zero
     * (which means we don't need that fd's original value any
     * more but we haven't set up its final value), and finish
     * them.
     */
    while (1) {
	int done_something = 0, something_to_do = 0, oldfd = -1, newfd;

	for (i = 0; i < maplen; i++) {
	    if (srccount[i] >= 0) {
		something_to_do = 1;
		oldfd = base + i;
	    }

	    if (srccount[i] == 0) {
		if (map[i] < 0) {
		    ret = close(base+i);
		    if (ret < 0) {
			if (syscall) *syscall = "close";
			return ret;
		    }
		} else {
		    /*
		     * If the fd we're duping into here is also in
		     * our range, decrement its source count. (If
		     * it was a self-assigned fd, its source count
		     * may be negative, but it can't be _zero_.)
		     */
		    if (map[i] >= base && map[i] < base + maplen) {
			assert(srccount[map[i]-base] != 0);
			if (srccount[map[i]-base] > 0)
			    srccount[map[i]-base]--;
		    }
		    ret = dup2(map[i], base+i);
		    if (ret < 0) {
			if (syscall) *syscall = "dup2";
			return ret;
		    }
		}
		srccount[i] = -1;
		done_something = 1;
	    }
	}

	/*
	 * If we managed to do something in the above loop, simply
	 * go round the loop again: we may well have rendered more
	 * fds usable by virtue of having finished duping their
	 * original values elsewhere.
	 */
	if (done_something)
	    continue;

	/*
	 * If we haven't done anything because there was nothing
	 * left that needed doing, we're done.
	 */
	if (!something_to_do)
	    break;

	/*
	 * That leaves the tricky case: we have become wedged,
	 * because the remaining fds to be permuted are deadlocked
	 * due to every fd being both a source and a target.
	 * 
	 * We solve this by picking an arbitrary problematic fd
	 * and duping it completely out of the range, then
	 * modifying the fd map to point to the new duplicate
	 * wherever it previously pointed to the problem fd. This
	 * will render that fd completable, so we can make further
	 * progress.
	 *
	 * This means we also have to keep a list of the external
	 * duplicates we are creating, and close them before
	 * returning.
	 * 
	 * (There is also the possibility that duping our problem
	 * fd will return something _inside_ our range, if there
	 * were closed fds there. Do we need to worry about dup
	 * returning the index of a slot we haven't finished with
	 * and thus allowing us to accidentally trash that fd by
	 * duping something else over it before we dup it in turn?
	 * We do not, and here's why. Consider our range of fd
	 * slots to be vertices of a directed graph, with an edge
	 * from A to B if map[B]==A. A basic constraint on the
	 * graph is that every vertex has an in-degree of at most
	 * one: map[B] might be negative, or point to something
	 * outside our range, but it can't take two values at
	 * once. Now if we're _here_ in the code because the above
	 * loop has got wedged, it must be because every vertex
	 * has an out-degree of at _least_ one, i.e. everything is
	 * the source for something else and isn't safe to clobber
	 * yet. But the graph is a closed system, so the sum of
	 * all the in-degrees must equal the sum of all the
	 * out-degrees - and the only way to arrange that given
	 * these constraints is for every vertex to have both an
	 * in-degree and an out-degree of _exactly_ one. Hence, by
	 * the time we get here, any fds in our range that are not
	 * alreay dealt with must be part of some closed cycle, so
	 * any that are closed are going to _stay_ closed and we
	 * needn't worry about them. [])
	 */
	assert(oldfd >= 0);

	newfd = dup(oldfd);
	if (newfd < 0) {
	    if (syscall) *syscall = "dup";
	    return newfd;
	}

	assert(ntmpfds < maplen);
	tmpfds[ntmpfds++] = newfd;

	for (i = 0; i < maplen; i++)
	    if (map[i] == oldfd)
		map[i] = newfd;

	srccount[oldfd - base] = 0;

	/*
	 * Now we can go back round to the loop at the top, and it
	 * will have been unwedged.
	 */
    }

    /*
     * When we get here, all the fds in the target range will be
     * correct. Now all we have to do is clean up our working
     * space.
     */
    while (ntmpfds > 0) {
	ret = close(tmpfds[--ntmpfds]);
	if (ret < 0) {
	    if (syscall) *syscall = "close";
	    return ret;
	}
    }
    sfree(tmpfds);
    sfree(srccount);

    return 0;
}

#ifdef TEST_MOVEFDS

#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

/*
 * gcc -g -O0 -DTEST_MOVEFDS -o movefds movefds.c malloc.c
 */

void fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

#define NTEMP 14

#define TEST(a, rel, b) do { \
    int aval = a; \
    int bval = b; \
    if (aval rel bval) \
	passes++; \
    else { \
	printf("FAIL: line %d: %s == %d (not %s %d)\n", \
		   __LINE__, #a, aval, #rel, bval); \
	fails++; \
    } \
} while (0)

int main(void)
{
    int min, max, base, maplen, fd;
    int i;
    int *map, *map2;
    char tempname[40];
    const char *syscall;
    int passes = 0, fails = 0;

    min = max = -1;

    for (i = 0; i < NTEMP; i++) {
	strcpy(tempname, "/tmp/movefds-test-XXXXXX");
	fd = mkstemp(tempname);
	if (min < 0) {
	    min = max = fd;
	} else {
	    if (min > fd)
		min = fd;
	    if (max < fd)
		max = fd;
	}
	/* Set the file's length equal to its fd index */
	assert(fd < 52);
	write(fd, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", fd);
	/* And pre-emptively clean up the file */
	unlink(tempname);
    }

    assert(min > 2);		       /* make sure std{in,out,err} are safe */
    assert(max - min == NTEMP - 1);    /* make sure we're all contiguous */

    base = min + 1;		       /* keep first fd outside the space */
    maplen = max - min + 1;	       /* have one empty slot in the space */
    map = snewn(maplen, int);

    /* Self-mapped fd */
    map[0] = base;
    /* 2-cycle */
    map[1] = base + 2;
    map[2] = base + 1;
    /* 3-cycle */
    map[3] = base + 4;
    map[4] = base + 5;
    map[5] = base + 3;
    /* Closure */
    map[6] = -1;
    /* Closure but use the fd first */
    map[7] = -1;
    map[8] = base + 7;
    /* Use an fd twice */
    map[9] = base + 7;
    /* Use an fd twice, once on itself */
    map[10] = base + 10;
    map[11] = base + 10;
    /* Map an fd from completely outside the space */
    map[12] = base - 1;
    /* Map an fd into a closed space */
    map[13] = base + 12;
    assert(maplen == 14);

    /* Keep a copy for reference, since move_fds may overwrite map */
    map2 = snewn(maplen, int);
    memcpy(map2, map, maplen * sizeof(int));

    if (move_fds(base, map, maplen, &syscall) < 0) {
	perror(syscall);
	exit(1);
    }

    for (i = 0; i < maplen; i++) {
	struct stat st;
	int ret;

	ret = fstat(base + i, &st);
	if (map2[i] < 0) {
	    TEST(ret, <, 0);
	    TEST(errno, ==, EBADF);
	} else {
	    TEST(ret, ==, 0);
	    TEST(st.st_size, ==, map2[i]);
	}
    }

    printf("Passed %d, failed %d\n", passes, fails);

    return fails != 0;
}

#endif
