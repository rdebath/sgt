#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "putty.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

static Socket s = NULL;

static void raw_size(void);

static void c_write(char *buf, int len)
{
    from_backend(0, buf, len);
}

static int raw_closing(Plug plug, char *error_msg, int error_code,
		       int calling_back)
{
    if (s) {
        sk_close(s);
        s = NULL;
    }
    if (error_msg) {
	/* A socket error has occurred. */
	connection_fatal(error_msg);
    }				       /* Otherwise, the remote side closed the connection normally. */
    return 0;
}

static int raw_receive(Plug plug, int urgent, char *data, int len)
{
    c_write(data, len);
    return 1;
}

/*
 * Called to set up the raw connection.
 * 
 * Returns an error message, or NULL on success.
 *
 * Also places the canonical host name into `realhost'. It must be
 * freed by the caller.
 */
static char *raw_init(char *host, int port, char **realhost)
{
    static struct plug_function_table fn_table = {
	raw_closing,
	raw_receive
    }, *fn_table_ptr = &fn_table;

    SockAddr addr;
    char *err;

    /*
     * Try to find host.
     */
    addr = sk_namelookup(host, realhost);
    if ((err = sk_addr_error(addr)))
	return err;

    if (port < 0)
	port = 23;		       /* default telnet port */

    /*
     * Open socket.
     */
    s = sk_new(addr, port, 0, 1, &fn_table_ptr);
    if ((err = sk_socket_error(s)))
	return err;

    sk_addr_free(addr);

    return NULL;
}

/*
 * Called to send data down the raw connection.
 */
static void raw_send(char *buf, int len)
{

    if (s == NULL)
	return;

    sk_write(s, buf, len);
}

/*
 * Called to set the size of the window
 */
static void raw_size(void)
{
    /* Do nothing! */
    return;
}

/*
 * Send raw special codes.
 */
static void raw_special(Telnet_Special code)
{
    /* Do nothing! */
    return;
}

static Socket raw_socket(void)
{
    return s;
}

static int raw_sendok(void)
{
    return 1;
}

static int raw_ldisc(int option)
{
    if (option == LD_EDIT || option == LD_ECHO)
	return 1;
    return 0;
}

Backend raw_backend = {
    raw_init,
    raw_send,
    raw_size,
    raw_special,
    raw_socket,
    raw_sendok,
    raw_ldisc,
    1
};
