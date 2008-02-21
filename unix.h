/*
 * unix.h: header for uxcommon.c, shared between Unixy platforms.
 */

#ifndef ICK_PROXY_UNIX_H
#define ICK_PROXY_UNIX_H

int uxmain(int multiuser, int port, char *dropprivuser, char **singleusercmd,
	   char *oscript, char *oinpac, char *ooutpac, int endfd);

#endif /* ICK_PROXY_UNIX_H */
