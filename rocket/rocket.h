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
