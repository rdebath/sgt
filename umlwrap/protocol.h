/*
 * protocol.h: Definitions and supporting functions for the wire
 * protocol spoken down the various serial lines that connect
 * umlwrap to its init process.
 */

#ifndef UMLWRAP_PROTOCOL_H
#define UMLWRAP_PROTOCOL_H

#include <stddef.h>
#include "sel.h"

/*
 * The protocol itself consists of a sequence of binary packets.
 * Each packet consists of a 32-bit big-endian length word L, then
 * L-4 bytes of data (so that L gives the full packet length). The
 * first byte of the packet data after the length is a command
 * code, indicating the type of packet.
 */

/*
 * Packet command code.
 */
enum {
    /*
     * Inbound control connection.
     */
    CMD_UNION,     /* Here's something to unionfs-overlay over the root */
    CMD_ROOTRW,    /* The root fs, or top level of the union, should be RW */
    CMD_ROOT,      /* Here is where the root fs should be */
    CMD_WRITABLE,  /* Mount this directory writably */
    CMD_HOSTNAME,  /* Here is the hostname for the virtual machine */
    CMD_UID,       /* Please run the command under the following uid */
    CMD_GID,       /* Please run the command under the following gid */
    CMD_GROUPS,    /* Please run the command with the following supp groups */
    CMD_TTYGID,    /* Please set tty devices' group to this */
    CMD_CHDIR,     /* Please change directory to here */
    CMD_ENVIRON,   /* Please set an environment variable ("var=value") */
    CMD_PTY,       /* Please set up a pty (termios and winsize enclosed) */
    CMD_PTYC,      /* Like CMD_PTY, but this one is also the controlling tty */
    CMD_OPIPE,     /* Please set up a pipe for outbound data */
    CMD_IPIPE,     /* Please set up a pipe for inbound data */
    CMD_IOPIPE,    /* Please set up a bidirectional pipe (i.e. socketpair) */
    CMD_ASSIGNFD,  /* Please assign fd N of the program to the ith pty/pipe */
    CMD_OSERIAL,   /* Please assign outbound side of ttySN to ith pty/pipe */
    CMD_ISERIAL,   /* Please assign inbound side of ttySN to ith pty/pipe */
    CMD_SETUPCMD,  /* Please system() this as root before dropping privs */
    CMD_COMMAND,   /* Here is a word of the command I want you to exec */
    CMD_GO,        /* Right, you've got everything. Run the command! */
    CMD_WINSIZE,   /* Controlling tty sent SIGWINCH; here's a struct winsize */
    /*
     * Outbound control connection.
     */
    CMD_HELLO,     /* I have initialised; you may start talking to me */
    CMD_FAILURE,   /* I have failed to perform some command (reason given) */
    CMD_GONE,      /* CMD_GO received, everything in order, command started */
    CMD_EXITCODE,  /* Main process terminated; here is a 32-bit exit code */
    /*
     * Individual data channel (either direction).
     */
    CMD_DATA,      /* the whole of the rest of the packet payload is data */
    CMD_EOF,       /* end of file on data channel */
};

/*
 * Routines to parse a data stream and split it into packets. 
 */
typedef struct protoread protoread;
typedef void (*protoread_pkt_fn_t)(void *ctx, int type,
				   void *data, size_t len);
protoread *protoread_new(void);
void protoread_free(protoread *pr);
void protoread_data(protoread *pr, void *data, size_t len,
		    protoread_pkt_fn_t gotpkt, void *ctx);

/*
 * Routine to construct a packet from arbitrarily many pieces of
 * data and send it off down a wfd.
 * 
 * Interleave size_t lengths with void * pointers, ending with a
 * null pointer.
 *
 * Returns the buffer size of the wfd (i.e. passes along the
 * return from sel_write).
 */
size_t protowrite(sel_wfd *wfd, int type, const void *data1, ...);

/*
 * Encapsulated mechanisms to copy between a raw data channel and
 * a protocol-run data channel. Each of these will construct a
 * self-contained context structure containing an rfd and a wfd,
 * add those to the given sel, and arrange to have data copied
 * from the one to the other as necessary.
 */
typedef struct protocopy_encode protocopy_encode;
typedef struct protocopy_decode protocopy_decode;
protocopy_encode *protocopy_encode_new(sel *sel, int infd, int outfd,
				       int flags);
protocopy_decode *protocopy_decode_new(sel *sel, int infd, int outfd,
				       int flags);
/* Flags stating what to do with the fds once the protocopy terminates */
#define PROTOCOPY_CLOSE_RFD 1
#define PROTOCOPY_CLOSE_WFD 2
#define PROTOCOPY_SHUTDOWN_WFD 4

/* Close the rfd for a protocopy_encode and send EOF on the wfd as if
 * it had closed naturally. */
void protocopy_encode_cutoff(protocopy_encode *pe);
/* Completely and drastically destroy a protocopy_decode, closing both
 * its fds and abandoning buffered data. */
void protocopy_decode_destroy(protocopy_decode *pd);

/*
 * Simple doubly-linked-list mechanism for keeping track of the
 * above protocopy objects. Both of the above types can be safely
 * cast to and from "listnode", so you can keep them on a linked
 * list.
 *
 * When one of the above objects dies a natural death, it will
 * automatically remove itself from the list it's on, if any.
 */
typedef enum {
    LISTNODE_PROTOCOPY_ENCODE,
    LISTNODE_PROTOCOPY_DECODE,
    LISTNODE_USER
} listnodetype;
typedef struct listnode listnode;
typedef struct list list;
struct listnode {
    listnodetype type;
    list *list;
    listnode *next, *prev;
};
struct list {
    listnode *head, *tail;
};
void list_init(list *list);
void list_add(list *list, listnode *listnode);
void list_del(listnode *listnode);
/*
 * Handy idiom for iterating over one of the above lists, in such
 * a way that it allows you to delete the current list item in the
 * loop body without the iteration getting confused.
 */
#define LOOP_OVER_LIST(iterationvariable, tempvariable, list) \
    for ((iterationvariable) = (list)->head; \
	 ((tempvariable) = (iterationvariable) ? \
          (iterationvariable)->next : NULL), \
	 (iterationvariable); \
	 (iterationvariable) = (tempvariable))

#endif /* UMLWRAP_PROTOCOL_H */
