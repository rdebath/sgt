int rocket_make_player_images(void);
void rocket_do_palette(int space);
void rocket_make_background(int space, int planet);

extern Image rocket_player_images[2][32];
extern Image rocket_bkgnd_image;

extern unsigned char rocket_bullet_images[2][8+3*3];
extern unsigned char rocket_ball_image[8+7*7];
extern unsigned char rocket_titletext_image[5228];
extern unsigned char rocket_choosekey_image[8+9*66];
extern unsigned char rocket_playertxt_image[8+41*9];
extern unsigned char rocket_lefttxt_image[8+28*9];
extern unsigned char rocket_righttxt_image[8+31*9];
extern unsigned char rocket_thrusttxt_image[8+43*9];
extern unsigned char rocket_burnertxt_image[8+41*9];
extern unsigned char rocket_firetxt_image[8+23*9];
extern unsigned char rocket_p1_image[8+2*9];
extern unsigned char rocket_p2_image[8+6*9];
extern Image rocket_keystxt_images[5];
extern Image rocket_playerstxt_images[2];

/*
 * Starting x and y positions.
 */
/* In space */
#define SSX1 (SCR_WIDTH/4)
#define SSX2 (SCR_WIDTH-1-SSX1)
#define SSY ((SCR_HEIGHT-10)/2)
#define BALLSTART 20             /* initial displacement of ball from ship */
/* And on land */
#define LSX1 (SCR_WIDTH/4+5)
#define LSX2 (SCR_WIDTH-1-LSX1)
#define LSY (SCR_HEIGHT-20)

/*
 * Coords for afterburner bars
 */
#define AFTERYTOP (SCR_HEIGHT-10)
#define AFTERYBOT (SCR_HEIGHT-2)
#define AFTERX1    37
#define AFTERX2   (SCR_WIDTH-1-AFTERX1)

/*
 * Coords for planet, and also ground
 */
#define PLANETX   (SCR_WIDTH/2)
#define PLANETY   ((SCR_HEIGHT-10)/2)
#define GROUNDHEIGHT (SCR_HEIGHT-20)
