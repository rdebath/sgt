/*
 * Enigma config.h.
 */

/*
 * Where to put Enigma's static files: the levels and level sets.
 */
#ifdef TESTING
#define LEVELDIR "./levels/"
#endif

/*
 * Where to put Enigma's variable files: the users' saved positions
 * and progress details, and the Hall of Fame.
 */
#ifdef TESTING
#define SAVEDIR "./saves/"
#endif

/*
 * The name of the level set Enigma should attempt to load on
 * startup if none is provided on the command line.
 */
#define DEFAULTSET "original"
