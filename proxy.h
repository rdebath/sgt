/*
 * proxy.h: interface between platform front ends and proxy.c.
 */

struct listenctx;
struct connctx;

/* ----------------------------------------------------------------------
 * Functions supplied by the platform code and called by proxy.c.
 */

/*
 * Creates a new listening socket, either on an arbitrary port if
 * `port' is negative, or else on the specific port specified.
 * Return value is the port actually allocated, or negative on
 * failure.
 * 
 * When new connections arrive on this port, the given listenctx
 * is used to construct sockcctxes for them.
 * 
 * This platform function may be called from within calls to
 * configure_master or configure_new_user (the latter via
 * got_data).
 */
int create_new_listener(char **err, int port, struct listenctx *ctx);

/*
 * Finds and loads into memory the rewrite script and input PAC
 * for a given user.
 */
char *get_script_for_user(char **err, const char *username);
char *get_inpac_for_user(char **err, const char *username);

/*
 * Report a genuinely program-fatal error and terminate.
 */
void platform_fatal_error(const char *err);

/* ----------------------------------------------------------------------
 * Functions supplied by proxy.c and called by the platform code.
 */

/*
 * Called before anything else, to initialise proxy.c's internal
 * state.
 */
void ick_proxy_setup(void);

/*
 * Where it all begins, in single-user mode. This function is
 * called to set up a listening socket and rewrite configuration
 * for a given user. It returns the (dynamically allocated) text
 * of a .pac for that user.
 * 
 * In multi-user mode this is called from _within_ proxy.c when a
 * connection comes in to the master socket requesting a .pac.
 */
char *configure_for_user(char *username);

/*
 * Where it all begins, in multi-user mode. This function sets up
 * the master listening socket on a given port number.
 */
void configure_master(int port);

/*
 * Called by the platform code when a new connection arrives on a
 * listening socket. Returns a connctx for the new connection,
 * given a listenctx for the listener.
 */
struct connctx *new_connection(struct listenctx *lctx);

/*
 * Called by the platform code to free a defunct connctx.
 */
void free_connection(struct connctx *cctx);

/*
 * Called by the platform code when data comes in on a connection.
 * 
 * If this function returns NULL, the platform code continues
 * reading from the socket. Otherwise, it returns some dynamically
 * allocated data which the platform code will then write to the
 * socket before closing it.
 */
char *got_data(struct connctx *ctx, char *data, int length);

/*
 * Rewrite a single URL using the script for a given user. Used
 * directly by the main program for testing modes (e.g. -t on
 * Unix).
 */
char *rewrite_url(char **err, const char *user, const char *url);
