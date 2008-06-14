/*
 * movefds.h: Completely rearrange a process's file descriptor
 * table.
 */

#ifndef UMLWRAP_MOVEFDS_H
#define UMLWRAP_MOVEFDS_H

/*
 * The input to this function is an array showing the target
 * layout of the fd table. For 0 <= n < maplen, map[n] gives the
 * number of the _currently_ existing fd (on entry) which is
 * expected to end up referenced by number base+n on exit. A
 * negative value in map is taken to mean that there should be no
 * fd in that slot on exit.
 *
 * All fds outside the range [base,base+maplen) are unchanged on
 * exit from this function. They may be used as targets.
 *
 * (Note that _indexes_ in the map array are offset by base - that
 * is, map[n] corresponds to file descriptor (base+n) - but the
 * _values_ in the map array are absolute.)
 *
 * If there is an error in any of the dup, dup2 or close
 * operations required to perform the operation, the return value
 * will be negative, and if "syscall" is non-NULL then *syscall
 * will be set to point at a (static, constant) string giving the
 * name of the failing system call (so that the client can give a
 * precise error report).
 */
int move_fds(int base, int *map, int maplen, const char **syscall);

#endif /* UMLWRAP_MOVEFDS_H */
