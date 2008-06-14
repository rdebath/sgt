/*
 * rawmode.h: Put a tty device in and out of raw mode.
 */

#ifndef UMLWRAP_RAWMODE_H
#define UMLWRAP_RAWMODE_H

#include <termios.h>

/*
 * Given an fd pointing to a terminal device, put it into raw mode
 * (effectively turning it into an 8-bit-clean pipe with no line
 * buffering).
 * 
 * If successful, returns 0, and if "termios" is not NULL writes
 * into it the struct termios that the tty previously had, so it
 * can be restored later with tcsetattr.
 *
 * If an error occurs, returns <0; errno is set by the failing
 * system call, and if "syscall" is not NULL then it is set to a
 * (static read-only) string containing the name of the failing
 * system call (so that the caller can produce an accurate error
 * report).
 */
int raw_mode(int fd, const char **syscall, struct termios *termios);

#endif /* UMLWRAP_RAWMODE_H */
