/*
 * mboxstore.c: Timber mail store in mbox format.
 * 
 * Within this mail store, I'm going to adopt the mbox format that
 * _should_ be. That is:
 * 
 *  - `From ' at the start of a line is _completely_ reserved as a
 *    message separator. It occurs in no other context. At all.
 * 
 *  - Any line beginning with one or more `>' followed by `From '
 *    represents the same line with one fewer `>' on it.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "timber.h"

static int mbox_lockfd = -1;

static int mbox_write_lock(void)
{
    struct flock lock;
    char *lockname = smalloc(200 + strlen(dirpath));
    sprintf(lockname, "%s/lockfile", dirpath);

    mbox_lockfd = open(lockname, O_RDWR);
    if (mbox_lockfd < 0) {
	error(err_perror, lockname, "open");
	return FALSE;
    }

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = lock.l_len = 0L;

    if (fcntl(mbox_lockfd, F_SETLKW, &lock) < 0) {
	error(err_perror, lockname, "F_SETLKW");
	sfree(lockname);
	return FALSE;
    }

    sfree(lockname);
    return TRUE;
}

static void mbox_release_lock(void)
{
    struct flock lock;
    assert(mbox_lockfd >= 0);

    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = lock.l_len = 0L;
    fcntl(mbox_lockfd, F_SETLK, &lock);
    close(mbox_lockfd);
    mbox_lockfd = -1;
}

static int write_mbox(int fd, char *data, int length, int beginning)
{
    char *queue;
    int qlen;

    /*
     * We begin at the start of a line. Therefore, I can assume we
     * are at the start of a line every time we come back to the
     * top of this loop.
     */
    queue = data;
    qlen = 0;
    while (length > 0) {
	char *p;

	/*
	 * Check for `From ' preceded by zero or more `>' signs,
	 * unless this is the very start of the message, in which
	 * case it's OK.
	 */
	if (!beginning) {
	    char *p = data;
	    int l = length;
	    while (l > 0 && *p == '>')
		p++, l--;
	    if (l >= 5 && !memcmp(p, "From ", 5)) {
		/*
		 * We have a `From ', which we need to escape.
		 * Flush the queue...
		 */
		if (qlen > 0) {
		    if (!write_wrapped(fd, queue, qlen) < 0)
			return FALSE;
		}
		/*
		 * ... write a leading `>' ...
		 */
		if (!write_wrapped(fd, ">", 1))
		    return FALSE;
		/*
		 * ... and restart queuing from the beginning of
		 * this line.
		 */
		queue = data;
		qlen = 0;
	    }
	}
	beginning = FALSE;

	/*
	 * Now advance to the next newline.
	 */
	p = memchr(data, '\n', length);
	if (p) {
	    p++;		       /* eat the newline as well */
	    length -= (p-data), qlen += (p-data), data = p;
	} else
	    qlen += length, length = 0;
    }

    /*
     * Finally flush the queue.
     */
    if (qlen > 0) {
	if (!write_wrapped(fd, queue, qlen) < 0)
	    return FALSE;
    }

    return TRUE;
}

static char *mbox_store_literal_inner(char *boxname, char *message, int msglen)
{
    int boxnum, need_set;
    int ret, fd, beginning;
    struct stat st;
    int maxsize, size;
    off_t oldlen, newlen;

    boxnum = cfg_get_int("current-mbox");
    maxsize = cfg_get_int("mbox-maxsize");

    boxnum--;			       /* to counter first `boxnum++' below */
    need_set = FALSE;
    do {
	boxnum++;
	sprintf(boxname, "%s/mbox%d", dirpath, boxnum);
	if (need_set)
	    cfg_set_int("current-mbox", boxnum);
	need_set = TRUE;

	/*
	 * See if our current mbox has space in it.
	 */
	ret = stat(boxname, &st);
	if (ret < 0) {
	    if (errno == ENOENT)
		size = 0;	       /* current mbox is empty */
	    else {
		error(err_perror, boxname, "stat");
		return NULL;	       /* failed to store message */
	    }
	} else
	    size = st.st_size;
	/*
	 * Special case: a single message that's too big always goes in
	 * its own mbox.
	 */
    } while (size > 0 && size + msglen > maxsize);

    /*
     * So now we know which mbox it's going into. Open the mbox and
     * put it there.
     */
    fd = open(boxname, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
	error(err_perror, boxname, "open");
	return NULL;		       /* failed to store message */
    }
    oldlen = lseek(fd, 0, SEEK_END);
    if (oldlen < 0) {
	error(err_perror, boxname, "lseek");
	return NULL;		       /* failed to store message */
    }

    /*
     * Invent a `From ' line if there isn't one already.
     */
    if (msglen < 5 || memcmp(message, "From ", 5)) {
	char fromline[80];
	struct tm tm;
	time_t t;

	t = time(NULL);
	tm = *gmtime(&t);
	strftime(fromline, 80,
		 "From timber-mbox-storage %a %b %d %H:%M:%S %Y\n", &tm);

	if (!write_mbox(fd, fromline, strlen(fromline), TRUE)) {
	    if (ftruncate(fd, oldlen) < 0)
		error(err_perror, boxname, "ftruncate");
	    return NULL;
	}
	beginning = FALSE;
    } else
	beginning = TRUE;

    /*
     * Now write the message.
     */
    if (!write_mbox(fd, message, msglen, beginning)) {
	if (ftruncate(fd, oldlen) < 0)
	    error(err_perror, boxname, "ftruncate");
	return NULL;
    }

    /*
     * Find out where we ended up.
     */
    newlen = lseek(fd, 0, SEEK_CUR);
    if (newlen < 0) {
	error(err_perror, boxname, "lseek");
	return NULL;		       /* failed to store message */
    }

    /*
     * Done. Sync and close the mbox, then return success.
     */
#ifndef NOSYNC
    if (fsync(fd) < 0) {
	error(err_perror, boxname, "fsync");
	if (ftruncate(fd, oldlen) < 0)
	    error(err_perror, boxname, "ftruncate");
	return NULL;
    }
#endif
    if (close(fd) < 0) {
	error(err_perror, boxname, "close");
	if (ftruncate(fd, oldlen) < 0)
	    error(err_perror, boxname, "ftruncate");
	return NULL;
    }

    /*
     * Construct a string telling us where the message is kept.
     */
    {
	char *ret;
	ret = smalloc(80);
	sprintf(ret, "mbox%d:%d:%d",
		boxnum, (int)oldlen, (int)(newlen - oldlen));
	return ret;
    }
}

/*
 * Surrounding the above with a small wrapper function is the
 * simplest way to ensure our mess is cleared up on every exit
 * path.
 */
static char *mbox_store_literal(char *message, int msglen)
{
    char *ret;
    char *boxname = smalloc(200 + strlen(dirpath));

    if (!mbox_write_lock()) {
	return NULL;
    }
    ret = mbox_store_literal_inner(boxname, message, msglen);
    mbox_release_lock();
    sfree(boxname);

    return ret;
}

static char *mbox_store_retrieve(const char *location, int *msglen)
{
    char *locmod = NULL, *boxname = NULL, *message = NULL, *p, *q;
    int offset, length, msgoff, fd;

    locmod = smalloc(1+strlen(location));
    strcpy(locmod, location);
    q = strrchr(locmod, ':');
    if (!q)
	return NULL;
    *q++ = '\0';
    p = strrchr(locmod, ':');
    if (!p)
	return NULL;
    *p++ = '\0';
    offset = atoi(p);
    length = atoi(q);
    boxname = smalloc(200 + strlen(locmod) + strlen(dirpath));
    sprintf(boxname, "%s/%s", dirpath, locmod);

    fd = open(boxname, O_RDONLY);
    if (fd < 0) {
	error(err_perror, boxname, "open");
	goto cleanup;
    }
    if (lseek(fd, offset, SEEK_SET) < 0) {
	error(err_perror, boxname, "lseek");
	goto cleanup;
    }
    message = smalloc(length);
    msgoff = 0;
    while (msgoff < length) {
	int ret;
	ret = read(fd, message + msgoff, length - msgoff);
	if (ret <= 0) {
	    if (ret < 0)
		error(err_perror, boxname, "read");
	    else
		error(err_internal, "mail store file ends abruptly");
	    goto cleanup;
	}
	msgoff += ret;
    }

    /*
     * FIXME: We must reverse From quoting here. (And also decide
     * what to do about the mbox separator.)
     */
    *msglen = length;
    goto cleanexit;

    cleanup:
    sfree(message);
    message = NULL;

    cleanexit:
    sfree(boxname);
    sfree(locmod);
    return message;
}

static void mbox_store_init(void)
{
    char *lockname = smalloc(200 + strlen(dirpath));
    sprintf(lockname, "%s/lockfile", dirpath);

    if (creat(lockname, 0600) < 0)
	fatal(err_perror, lockname, "creat");

    sfree(lockname);
}

const struct storage mbox_store = {
    mbox_store_literal,
    mbox_store_init,
    mbox_store_retrieve,
};
