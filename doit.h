/*
 * DoIt shared header file.
 */

#define DOIT_PORT 17481

typedef struct doit_ctx_tag doit_ctx;

#define NONCE_MAX_LEN 64

/*
 * Set up a crypto context.
 */
doit_ctx *doit_init_ctx(void *secret, int secret_len);

/*
 * Add perturbation data for nonce generation.
 */
void doit_perturb_nonce(doit_ctx *ctx, void *data, int len);

/*
 * Construct a nonce and return a ready-to-send buffer containing
 * it. Returns the length of the data to send. The buffer should be
 * freed after sending.
 */
void *doit_make_nonce(doit_ctx *ctx, int *output_len);

/*
 * Process incoming data on a DoIt connection. Buffers any
 * plaintext for later retrieval by doit_read. Returns <0 if
 * something bad happens (like a MAC failing). Otherwise, returns
 * the current amount of buffered plaintext.
 */
int doit_incoming_data(doit_ctx *ctx, void *buf, int len);

/*
 * Determine whether the incoming nonce has been received and the
 * keys have been set up.
 */
int doit_got_keys(doit_ctx *ctx);

/*
 * Read plaintext out of the buffered area. Returns the amount of
 * data actually read.
 */
int doit_read(doit_ctx *ctx, void *buf, int len);

/*
 * Construct a DoIt outgoing packet and return it in ready-to-send
 * form. The resulting packet should be freed after sending.
 */
void *doit_send(doit_ctx *ctx, void *buf, int len, int *output_len);
