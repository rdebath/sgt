/*
 * Enigma settings.h.
 * 
 * Copyright 2000 Simon Tatham. All rights reserved.
 * 
 * Enigma is licensed under the MIT licence. See the file LICENCE for
 * details.
 * 
 * - we are all amf -
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

/*
 * Characters not permitted in level set names. (To prevent people
 * using ../<stuff> as a level set name to try to make Enigma
 * subvert other programs.) Just to be safe, we'll also put '.' in
 * here so that there's absolutely no chance of things in SAVEDIR
 * overwriting each other, by accident or design.
 */
#define SETNAME_INVALID "/."
