/*
 * isdup.h: Function to detect whether two fds are duplicates of
 * each other. That is, not only do they address the same file or
 * device, but they have their origin in the same call to open()
 * and hence share a file pointer, nonblocking status etc.
 */

#ifndef UMLWRAP_ISDUP_H
#define UMLWRAP_ISDUP_H

/*
 * Returns 1 if the fds are duplicates, or 0 if they're not.
 * 
 * If "approx" is true, approximate methods will be used if exact
 * methods are inconclusive, and the return value will always be
 * either 0 or 1. If "approx" is false, then the return value
 * might instead be -1, for "don't know".
 */
int isdup(int fd1, int fd2, int approx);

#endif /* UMLWRAP_ISDUP_H */
