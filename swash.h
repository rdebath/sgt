void swash_text(int x, int y, char *text,
		void (*plot)(void *ctx, int x, int y), void *plotctx);
void swash_centre(int xmin, int xmax, int y, char *text,
		  void (*plot)(void *ctx, int x, int y), void *plotctx);
int swash_width(char *text);
