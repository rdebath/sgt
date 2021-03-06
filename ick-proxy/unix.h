/*
 * unix.h: header for uxcommon.c, shared between Unixy platforms.
 */

#ifndef ICK_PROXY_UNIX_H
#define ICK_PROXY_UNIX_H

extern char *override_inpac;
extern char *override_script;
extern char *override_outpac;

int configure_single_user(void);

int uxmain(int multiuser, int port, char *dropprivuser, char **singleusercmd,
	   int clientfd, int (*clientfdread)(int fd), int daemon);

#endif /* ICK_PROXY_UNIX_H */
