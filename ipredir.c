#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

/*
 * Small shared library for Linux, to be loaded with LD_PRELOAD.
 * Overrides connect() so that connections to one IP:port
 * combination turn out to go to another.
 *
 * compilation (linux):
 *   gcc -fpic -c ipredir.c && ld -shared -o ipredir.so ipredir.o
 * use:
 *   export LD_PRELOAD=/full/path/name/to/ipredir.so
 *   export IPREDIR=123.45.67.89:12345:198.76.54.32:54321
 *   telnet 123.45.67.89 12345 # goes to 198.76.54.32 54321
 */

int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
    typedef int (*connect_t)(int, const struct sockaddr *, socklen_t);
    connect_t realconnect;
    const struct sockaddr_in *si = (const struct sockaddr_in *)serv_addr;
    struct sockaddr_in oursi;
    char *p = getenv("IPREDIR");

    if (p &&
	addrlen >= sizeof(struct sockaddr_in) &&
	si->sin_family == AF_INET) {
	char *q, us[40];
	int uslen;
	uslen = sprintf(us, "%s:%d", inet_ntoa(si->sin_addr),
			ntohs(si->sin_port));
	while (*p) {
	    if (!strncmp(p, us, uslen) && p[uslen] == ':' &&
		(q = strchr(p+uslen+1, ':')) != NULL) {
		p += uslen+1;
		sprintf(us, "%.*s", (q-p > 16 ? 16 : q-p), p);
		memset(&oursi, 0, sizeof(oursi));
		oursi.sin_family = AF_INET;
		oursi.sin_port = ntohs(atoi(q+1));
		oursi.sin_addr.s_addr = inet_addr(us);
		serv_addr = (const struct sockaddr *)&oursi;
		addrlen = sizeof(oursi);
	    } else {
		p += uslen;
		p += strcspn(p, ":");
		if (*p) p++;
		p += strcspn(p, ":");
		if (*p) p++;
	    }
	}
    }

    realconnect = (connect_t)dlsym(RTLD_NEXT, "connect");
    realconnect(sockfd, serv_addr, addrlen);
}
