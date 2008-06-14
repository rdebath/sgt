/*
 * isdup.c: Implementation of isdup.h.
 */

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <stdlib.h>

#define __USE_GNU

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include "isdup.h"

#ifdef TEST_ISDUP
int reason;
#endif

int isdup(int fd1, int fd2, int approx)
{
    int ret, fl1, fl2, i, hits;
    struct stat st1, st2;

#ifdef TEST_ISDUP
    reason = -1;
#endif

    /*
     * To begin with, weed out obvious non-duplicates by using
     * fstat and F_GETFL.
     */
    if (fstat(fd1, &st1) == 0 && fstat(fd2, &st2) == 0) {
	if (st1.st_dev != st2.st_dev ||
	    st1.st_ino != st2.st_ino) {
#ifdef TEST_ISDUP
	    reason = 1;
#endif
	    return 0;		       /* different files => not duplicates */
	}
    }
    if ((fl1 = fcntl(fd1, F_GETFL)) >= 0 &&
	(fl2 = fcntl(fd2, F_GETFL)) >= 0) {
	/*
	 * Race conditions: another process might have done
	 * F_SETFL in between those two calls. The Linux man page
	 * for fcntl says that the only flags settable by F_SETFL
	 * are O_APPEND, O_NONBLOCK, O_ASYNC, and O_DIRECT. So we
	 * exclude those from consideration, and check everything
	 * else.
	 */
	fl1 &= ~(O_APPEND | O_NONBLOCK | O_ASYNC | O_DIRECT);
	fl2 &= ~(O_APPEND | O_NONBLOCK | O_ASYNC | O_DIRECT);
	if (fl1 != fl2) {
#ifdef TEST_ISDUP
	    reason = 2;
#endif
	    return 0;		       /* not duplicates */
	}
    }

    /*
     * The most reliable way to determine duplicateness is using
     * flock. An flock() lock is associated with an open file; so
     * if we can successfully take out an exclusive lock on fd1,
     * then a subsequent attempt to exclusively lock fd2 will
     * succeed if and only if fd1 and fd2 are duplicates.
     *
     * (This is assuming, of course, that our fstat work above has
     * already disposed of the case where fd1 and fd2 are not even
     * pointing to the same _file_. Naturally if they're different
     * files we can exclusively lock each of them independently!
     * But if they're the same file, and we lock one of them, then
     * we can lock the other iff the fds are duplicates.)
     */
    ret = flock(fd1, LOCK_EX | LOCK_NB);
    if (ret == 0) {
	ret = flock(fd2, LOCK_EX | LOCK_NB);
	flock(fd1, LOCK_UN | LOCK_NB);

#ifdef TEST_ISDUP
	reason = 3;
#endif
	return (ret == 0);
    }

    /*
     * The above will be inconclusive if we weren't able to
     * exclusively lock fd1 in the first place, perhaps because
     * another process had the file locked already. In that case,
     * we must fall back to approximate methods.
     */
    if (!approx) {
#ifdef TEST_ISDUP
	reason = 4;
#endif
	return -1;		       /* couldn't say for sure */
    }

    /*
     * The nonblocking status of a file is a property of the open
     * file. So switch it back and forth randomly on one fd for a
     * bit and see whether the other fd keeps pace.
     * 
     * This is subject to race conditions (if another process is
     * changing the nonblocking status of the file for its own
     * purposes) and to deliberate subversion (if another process
     * by dint of great effort is duplicating our changes to one
     * fd into the other in an effort to convince us they're the
     * same). However, in the absence of outside interference it's
     * reliable.
     */
    fl1 = fcntl(fd1, F_GETFL);
    hits = 0;
    for (i = 0; i < 128; i++) {
	int r = rand();
	fl2 = fl1 & ~O_NONBLOCK;
	if (r >= RAND_MAX/2)
	    fl2 |= O_NONBLOCK;
	fcntl(fd1, F_SETFL, fl2);
	if (fcntl(fd2, F_GETFL) == fl2)
	    hits++;
    }
    fcntl(fd1, F_SETFL, fl1);
#ifdef TEST_ISDUP
    reason = 5;
#endif
    return hits == i;
}

#ifdef TEST_ISDUP

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
    char filename[40];
    char filename2[40];
    int passes = 0, fails = 0;
    int fd1, fd2, fd3, fd4;

    strcpy(filename, "/tmp/isduptestXXXXXX");
    if (!mkstemp(filename)) {
	perror("mkstemp");
	return 1;
    }

    strcpy(filename2, "/tmp/isduptestXXXXXX");
    if (!mkstemp(filename2)) {
	perror("mkstemp");
	return 1;
    }

    /*
     * Test reason 1 (fstat).
     */
    fd1 = open(filename, O_RDONLY);
    fd2 = open(filename, O_RDONLY);
    fd3 = open(filename2, O_RDONLY);
    TEST(isdup(fd1, fd2, 0), ==, 0);
    TEST(reason, >, 1);
    TEST(isdup(fd1, fd3, 0), ==, 0);
    TEST(reason, ==, 1);
    close(fd1);
    close(fd2);
    close(fd3);

    /*
     * Test reason 2 (F_GETFL).
     */
    fd1 = open(filename, O_RDONLY);
    fd2 = open(filename, O_RDONLY);
    fd3 = open(filename, O_RDWR);
    TEST(isdup(fd1, fd2, 0), ==, 0);
    TEST(reason, >, 2);
    TEST(isdup(fd1, fd3, 0), ==, 0);
    TEST(reason, ==, 2);
    close(fd1);
    close(fd2);
    close(fd3);

    /*
     * Test reason 3 (flock).
     */
    fd1 = open(filename, O_RDONLY);
    fd2 = open(filename, O_RDONLY);
    fd3 = dup(fd2);
    TEST(isdup(fd1, fd2, 0), ==, 0);
    TEST(reason, ==, 3);
    TEST(isdup(fd1, fd3, 0), ==, 0);
    TEST(reason, ==, 3);
    TEST(isdup(fd2, fd3, 0), >, 0);
    TEST(reason, ==, 3);
    close(fd1);
    close(fd2);
    close(fd3);

    /*
     * Test reason 4 (unwillingness to give an approximate answer).
     */
    fd1 = open(filename, O_RDONLY);
    fd2 = open(filename, O_RDONLY);
    fd3 = dup(fd2);
    fd4 = open(filename, O_RDONLY);
    flock(fd4, LOCK_EX);
    TEST(isdup(fd1, fd2, 0), <, 0);
    TEST(reason, ==, 4);
    TEST(isdup(fd1, fd3, 0), <, 0);
    TEST(reason, ==, 4);
    TEST(isdup(fd2, fd3, 0), <, 0);
    TEST(reason, ==, 4);
    close(fd1);
    close(fd2);
    close(fd3);
    close(fd4);

    /*
     * Test reason 5 (approximate by nonblocking).
     */
    fd1 = open(filename, O_RDONLY);
    fd2 = open(filename, O_RDONLY);
    fd3 = dup(fd2);
    fd4 = open(filename, O_RDONLY);
    flock(fd4, LOCK_EX);
    TEST(isdup(fd1, fd2, 1), ==, 0);
    TEST(reason, ==, 5);
    TEST(isdup(fd1, fd3, 1), ==, 0);
    TEST(reason, ==, 5);
    TEST(isdup(fd2, fd3, 1), >, 0);
    TEST(reason, ==, 5);
    close(fd1);
    close(fd2);
    close(fd3);
    close(fd4);

    /*
     * Remove temporary files.
     */
    remove(filename);
    remove(filename2);

    printf("Passed %d, failed %d\n", passes, fails);

    return fails != 0;
}

#endif
