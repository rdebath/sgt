/*
 * game256.h: a header file defining types and functions which
 * mimic my old Borland Pascal Game256 API.
 */

typedef unsigned char *Image;

typedef struct Sprite {
    Image background;
    int back_size;
    int x, y, mask;
    int drawn;
} *Sprite;

#define XOFFSET(image) ( (short) ( (image)[1] * 256 + (image)[0] ) )
#define YOFFSET(image) ( (short) ( (image)[3] * 256 + (image)[2] ) )
#define XSIZE(image) ( (image)[5] * 256 + (image)[4] )
#define YSIZE(image) ( (image)[7] * 256 + (image)[6] )
#define IMGDATA(image) ( (image)+8 )

Sprite makesprite(int max_width, int max_height, int mask);
void freesprite(Sprite spr);
void putsprite(Sprite spr, Image image, int x, int y);
void erasesprite(Sprite spr);

void initframe(int microseconds);
void endframe(void);

void pickimage(Image image, int x, int y);
void drawimage(Image image, int x, int y, int mask);
void imagepixel(Image image, int x, int y, int colour);
int getimagepixel(Image image, int x, int y);
void imageonimage(Image canvas, Image brush, int x, int y, int mask);
