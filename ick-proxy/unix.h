/*
 * unix.h: header for uxcommon.c, shared between Unixy platforms.
 */

#ifndef ICK_PROXY_UNIX_H
#define ICK_PROXY_UNIX_H

int configure_single_user(void);

int uxmain(int multiuser, int port, char *dropprivuser, char **singleusercmd,
	   char *oscript, char *oinpac, char *ooutpac, int clientfd,
	   int (*clientfdread)(int fd));

#endif /* ICK_PROXY_UNIX_H */
