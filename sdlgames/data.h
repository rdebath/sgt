/*
 * Header implementing the `data'-type font so popular in the
 * 1980s, which I (perhaps ill-advisedly) used in Rocket Attack.
 */

int data_width(char *text);
int data_text(char *text, int x, int y,
	      void (*plot)(void *ctx, int x, int y, int on), void *ctx);
