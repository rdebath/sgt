/*
 * Perfectly analyse two-player Elefantenparade by depth-first
 * search. 
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define min(x,y) ( (x)<(y)?(x):(y) )
#define max(x,y) ( (x)>(y)?(x):(y) )

/* ----------------------------------------------------------------------
 * General utility functions.
 */

/*
 * Find the numeric index of a combination.
 */
int comb_to_index(char *array, int setmask, int ignoremask, int n, int k)
{
    int ret = 0;
    int i, nck;

    /*
     * Start by computing n choose k. For the numbers we're working
     * with here, this shouldn't overflow.
     */
    nck = 1;
    for (i = 0; i < k; i++)
	nck = nck * (n-i);
    for (i = 2; i <= k; i++)
	nck = nck / i;

    while (1) {
	while (ignoremask & (1 << (*array)))
	    array++;

	if (k == 0)
	    return ret;		       /* only one position now */

	if (setmask & (1 << (*array))) {
	    /*
	     * First element found.
	     */
	    nck = nck * k / n;
	    k--;
	} else {
	    /*
	     * First element not found, meaning we have just
	     * skipped over (n-1 choose k-1) positions.
	     */
	    ret += nck * k / n;
	    nck = nck * (n-k) / n;
	}
	array++;
	n--;
    }
}

/*
 * Restore a combination from its numeric index.
 */
void comb_from_index(char *array, int val, int ignoremask,
		     int n, int k, int index)
{
    int i, nck;

    /*
     * Start by computing n choose k. For the numbers we're working
     * with here, this shouldn't overflow.
     */
    nck = 1;
    for (i = 0; i < k; i++)
	nck = nck * (n-i);
    for (i = 2; i <= k; i++)
	nck = nck / i;

    while (k > 0) {
	while (ignoremask & (1 << (*array)))
	    array++;

	if (index < nck * k / n) {
	    /*
	     * First element is here.
	     */
	    *array = val;
	    nck = nck * k / n;
	    k--;
	} else {
	    /*
	     * It isn't.
	     */
	    index -= nck * k / n;
	    nck = nck * (n-k) / n;
	}
	array++;
	n--;
    }
}

/* ----------------------------------------------------------------------
 * Game-specific utility functions.
 */

/*
 * Board layout:
 * 
 *  - Board squares start at 0 (so with two players, the starting
 *    positions are 0,1,2,3)
 *  - Water at 15
 *  - 2 logs at 17
 *  - 2 logs at 26
 *  - Water at 31
 *  - 2 logs at 35
 *  - Water at 41
 *  - 2 logs at 44
 *  - 2 logs at 52
 *  - Last real square is 58
 *  - Finish square (3,2,1 logs) is 59.
 */
#define LOGS(x) (x)
#define BLANK   5
#define WATER   6
const int board[59] = {
    BLANK, BLANK, BLANK, BLANK, BLANK, BLANK, BLANK, BLANK, BLANK, BLANK,
    BLANK, BLANK, BLANK, BLANK, BLANK, WATER, BLANK, LOGS(0), BLANK, BLANK,
    BLANK, BLANK, BLANK, BLANK, BLANK, BLANK, LOGS(1), BLANK, BLANK, BLANK,
    BLANK, WATER, BLANK, BLANK, BLANK, LOGS(2), BLANK, BLANK, BLANK, BLANK,
    BLANK, WATER, BLANK, BLANK, LOGS(3), BLANK, BLANK, BLANK, BLANK, BLANK,
    BLANK, BLANK, LOGS(4), BLANK, BLANK, BLANK, BLANK, BLANK, BLANK,
};

/*
 * Exhaustive enumeration of possible positions. We see every
 * position from the perspective of the player whose turn it
 * currently is.
 * 
 * There are a number of basic cases:
 * 
 *  (a) Start of game: 1 position.
 *  (b) One elephant placed: 4 positions.
 *  (c) Two elephants placed: 4*3 positions (they are distinct).
 *  (d) Three elephants placed: 4*3 positions (two of them are
 * 	indistinguishable, so this is equivalent to choosing which
 * 	of the four starting squares is empty and which is the
 * 	second player's first elephant).
 *  (e) Four elephants all in play on the main board: 59*58/2
 * 	(active player's elephants) times 57*56/2 (inactive
 * 	player's elephants) times 2^5 (remaining logs) = 87384192
 * 	positions.
 *  (f) Active player has one elephant home: 59*58/2 (the
 * 	two-elephant team remaining in play) times 57 (the solo
 * 	elephant) times 2^5 (logs) = 3120864 positions.
 *  (g) Inactive player has one elephant home: 3120864 positions
 * 	also.
 *  (h) Active player has both elephants home: 59*58/2 (inactive
 * 	player's elephants) times 2^5 (logs) = 54752 positions.
 *  (i) Inactive player has both elephants home: 54752 positions
 * 	also.
 *  (j) Both players have one elephant home: 59*58 (distinct
 * 	elephants) times 2^5 (logs) = 109504 positions.
 *  (k) Active player has only remaining elephant: 59 times 2^5 =
 * 	1888 positions.
 *  (l) Inactive player has only remaining elephant: 59 times 2^5 =
 * 	1888 positions.
 *  (m) End of game (all elephants home): 2^5 positions (logs may
 * 	remain).
 * 
 * Total number of game positions is 93848734.
 */

/*
 * Structure describing the current game state in a fairly
 * processing-friendly fashion.
 */
struct game_state {
    /*
     * Board layout. 0 means blank square; 1 means active player's
     * elephant; 2 means inactive player's elephant.
     */
    char board[59];

    /*
     * Remaining logs.
     */
    char logs[5];

    /*
     * Elephants left to place, and home, for each player.
     */
    int aplace, ahome, iplace, ihome;
};

/*
 * Marshal a game_state into a numeric position index.
 */
int marshal(struct game_state state)
{
    int base, ret, i;
    
    base = 0;

    /* (a) Start of game: 1 position. */
    if (state.aplace == 2 && state.iplace == 2)
	return 0;
    base++;

    /* (b) One elephant placed: 4 positions. */
    if (state.aplace == 2 && state.iplace == 1) {
	ret = comb_to_index(state.board, 4, 0, 4, 1);
	return base + ret;
    }
    base += 4;

    /* (c) Two elephants placed: 4*3 positions. */
    if (state.aplace == 1 && state.iplace == 1) {
	/* Index the combination of all elephants. */
	ret = comb_to_index(state.board, 6, 0, 4, 2);
	/* Now index which elephant is whose. */
	ret = ret * 2 + comb_to_index(state.board, 4, 1, 2, 1);
	return base + ret;
    }
    base += 4*3;

    /* (d) Three elephants placed: 4*3 positions. */
    if (state.aplace == 1 && state.iplace == 0) {
	/* Index the combination of all elephants. */
	ret = comb_to_index(state.board, 6, 0, 4, 3);
	/* Now index which elephant is whose. */
	ret = ret * 3 + comb_to_index(state.board, 4, 1, 3, 2);
	return base + ret;
    }
    base += 4*3;

    assert(state.aplace == 0 && state.iplace == 0);

    /* (e) Four elephants all in play on the main board: 87384192 positions. */
    if (state.ahome == 0 && state.ihome == 0) {
	/* Index the combination of all elephants. */
	ret = comb_to_index(state.board, 6, 0, 59, 4);
	/* Now index which elephants are whose. */
	ret = ret * (4*3/2) + comb_to_index(state.board, 4, 1, 4, 2);
	/* Add on the logs. */
	for (i = 0; i < 5; i++) {
	    ret *= 2;
	    if (state.logs[i])
		ret++;
	}
	return base + ret;
    }
    base += 87384192;

    /* (f) Active player has one elephant home: 3120864 positions. */
    if (state.ahome == 1 && state.ihome == 0) {
	/* Index the combination of all elephants. */
	ret = comb_to_index(state.board, 6, 0, 59, 3);
	/* Now index which elephants are whose. */
	ret = ret * 3 + comb_to_index(state.board, 4, 1, 3, 2);
	/* Add on the logs. */
	for (i = 0; i < 5; i++) {
	    ret *= 2;
	    if (state.logs[i])
		ret++;
	}
	return base + ret;
    }
    base += 3120864;

    /* (g) Inactive player has one elephant home: 3120864 positions. */
    if (state.ahome == 0 && state.ihome == 1) {
	/* Index the combination of all elephants. */
	ret = comb_to_index(state.board, 6, 0, 59, 3);
	/* Now index which elephants are whose. */
	ret = ret * 3 + comb_to_index(state.board, 4, 1, 3, 1);
	/* Add on the logs. */
	for (i = 0; i < 5; i++) {
	    ret *= 2;
	    if (state.logs[i])
		ret++;
	}
	return base + ret;
    }
    base += 3120864;

    /* (h) Active player has both elephants home: 54752 positions. */
    if (state.ahome == 2 && state.ihome == 0) {
	/* Index all elephants. */
	ret = comb_to_index(state.board, 6, 0, 59, 2);
	/* Add on the logs. */
	for (i = 0; i < 5; i++) {
	    ret *= 2;
	    if (state.logs[i])
		ret++;
	}
	return base + ret;
    }
    base += 54752;

    /* (i) Inactive player has both elephants home: 54752 positions. */
    if (state.ahome == 0 && state.ihome == 2) {
	/* Index all elephants. */
	ret = comb_to_index(state.board, 6, 0, 59, 2);
	/* Add on the logs. */
	for (i = 0; i < 5; i++) {
	    ret *= 2;
	    if (state.logs[i])
		ret++;
	}
	return base + ret;
    }
    base += 54752;

    /* (j) Both players have one elephant home: 109504 positions. */
    if (state.ahome == 1 && state.ihome == 1) {
	/* Index the combination of both elephants. */
	ret = comb_to_index(state.board, 6, 0, 59, 2);
	/* Now index which elephant is whose. */
	ret = ret * 2 + comb_to_index(state.board, 4, 1, 2, 1);
	/* Add on the logs. */
	for (i = 0; i < 5; i++) {
	    ret *= 2;
	    if (state.logs[i])
		ret++;
	}
	return base + ret;
    }
    base += 109504;

    /* (k) Active player has only remaining elephant: 1888 positions. */
    if (state.ahome == 1 && state.ihome == 2) {
	/* Index the elephant. */
	ret = comb_to_index(state.board, 6, 0, 59, 1);
	/* Add on the logs. */
	for (i = 0; i < 5; i++) {
	    ret *= 2;
	    if (state.logs[i])
		ret++;
	}
	return base + ret;
    }
    base += 1888;

    /* (l) Inactive player has only remaining elephant: 1888 positions. */
    if (state.ahome == 2 && state.ihome == 1) {
	/* Index the elephant. */
	ret = comb_to_index(state.board, 6, 0, 59, 1);
	/* Add on the logs. */
	for (i = 0; i < 5; i++) {
	    ret *= 2;
	    if (state.logs[i])
		ret++;
	}
	return base + ret;
    }
    base += 1888;

    /* (m) End of game: 2^5 positions. */
    assert(state.ahome == 2 && state.ihome == 2);
    /* Just do the logs. */
    ret = 0;
    for (i = 0; i < 5; i++) {
	ret *= 2;
	if (state.logs[i])
	    ret++;
    }
    return base + ret;
}

/*
 * Restore a game_state from a numeric index.
 */
struct game_state unmarshal(int index)
{
    struct game_state state;
    int i;

    memset(state.board, 0, 59);
    memset(state.logs, 1, 5);
    state.aplace = state.iplace = state.ahome = state.ihome = 0;

    /* (a) Start of game: 1 position. */
    if (index < 1) {
	state.aplace = state.iplace = 2;
	return state;
    }
    index--;

    /* (b) One elephant placed: 4 positions. */
    if (index < 4) {
	state.aplace = 2;
	state.iplace = 1;
	comb_from_index(state.board, 2, 0, 4, 1, index);
	return state;
    }
    index -= 4;

    /* (c) Two elephants placed: 4*3 positions. */
    if (index < 4*3) {
	state.aplace = 1;
	state.iplace = 1;
	comb_from_index(state.board, 1, 0, 4, 2, index / 2);
	comb_from_index(state.board, 2, 1, 2, 1, index % 2);
	return state;
    }
    index -= 4*3;

    /* (d) Three elephants placed: 4*3 positions. */
    if (index < 4*3) {
	state.aplace = 1;
	state.iplace = 0;
	comb_from_index(state.board, 1, 0, 4, 3, index / 3);
	comb_from_index(state.board, 2, 1, 3, 2, index % 3);
	return state;
    }
    index -= 4*3;

    /* (e) Four elephants all in play on the main board: 87384192 positions. */
    if (index < 87384192) {
	for (i = 5; i-- > 0 ;) {
	    if (!(index & 1))
		state.logs[i] = 0;
	    index >>= 1;
	}
	comb_from_index(state.board, 1, 0, 59, 4, index / (4*3/2));
	comb_from_index(state.board, 2, 1, 4, 2, index % (4*3/2));
	return state;
    }
    index -= 87384192;

    /* (f) Active player has one elephant home: 3120864 positions. */
    if (index < 3120864) {
	state.ahome = 1;
	state.ihome = 0;
	for (i = 5; i-- > 0 ;) {
	    if (!(index & 1))
		state.logs[i] = 0;
	    index >>= 1;
	}
	comb_from_index(state.board, 1, 0, 59, 3, index / 3);
	comb_from_index(state.board, 2, 1, 3, 2, index % 3);
	return state;
    }
    index -= 3120864;

    /* (g) Inactive player has one elephant home: 3120864 positions. */
    if (index < 3120864) {
	state.ahome = 0;
	state.ihome = 1;
	for (i = 5; i-- > 0 ;) {
	    if (!(index & 1))
		state.logs[i] = 0;
	    index >>= 1;
	}
	comb_from_index(state.board, 1, 0, 59, 3, index / 3);
	comb_from_index(state.board, 2, 1, 3, 1, index % 3);
	return state;
    }
    index -= 3120864;

    /* (h) Active player has both elephants home: 54752 positions. */
    if (index < 54752) {
	state.ahome = 2;
	state.ihome = 0;
	for (i = 5; i-- > 0 ;) {
	    if (!(index & 1))
		state.logs[i] = 0;
	    index >>= 1;
	}
	comb_from_index(state.board, 2, 0, 59, 2, index);
	return state;
    }
    index -= 54752;

    /* (i) Inactive player has both elephants home: 54752 positions. */
    if (index < 54752) {
	state.ahome = 0;
	state.ihome = 2;
	for (i = 5; i-- > 0 ;) {
	    if (!(index & 1))
		state.logs[i] = 0;
	    index >>= 1;
	}
	comb_from_index(state.board, 1, 0, 59, 2, index);
	return state;
    }
    index -= 54752;

    /* (j) Both players have one elephant home: 109504 positions. */
    if (index < 109504) {
	state.ahome = 1;
	state.ihome = 1;
	for (i = 5; i-- > 0 ;) {
	    if (!(index & 1))
		state.logs[i] = 0;
	    index >>= 1;
	}
	comb_from_index(state.board, 1, 0, 59, 2, index / 2);
	comb_from_index(state.board, 2, 1, 2, 1, index % 2);
	return state;
    }
    index -= 109504;

    /* (k) Active player has only remaining elephant: 1888 positions. */
    if (index < 1888) {
	state.ahome = 1;
	state.ihome = 2;
	for (i = 5; i-- > 0 ;) {
	    if (!(index & 1))
		state.logs[i] = 0;
	    index >>= 1;
	}
	comb_from_index(state.board, 1, 0, 59, 1, index);
	return state;
    }
    index -= 1888;

    /* (l) Inactive player has only remaining elephant: 1888 positions. */
    if (index < 1888) {
	state.ahome = 2;
	state.ihome = 1;
	for (i = 5; i-- > 0 ;) {
	    if (!(index & 1))
		state.logs[i] = 0;
	    index >>= 1;
	}
	comb_from_index(state.board, 2, 0, 59, 1, index);
	return state;
    }
    index -= 1888;

    /* (m) End of game: 2^5 positions. */
    state.ahome = 2;
    state.ihome = 2;
    for (i = 5; i-- > 0 ;) {
	if (!(index & 1))
	    state.logs[i] = 0;
	index >>= 1;
    }
    return state;
}

/*
 * Validate a game state.
 */
void validate(struct game_state state)
{
    int ae, ie, i;

    /*
     * Can't have elephants to place and home at the same time.
     */
    assert(state.ahome + state.ihome == 0 || state.aplace + state.iplace == 0);

    /*
     * Active player must have at least as many elephants to place
     * as inactive player.
     */
    assert(state.aplace >= state.iplace);

    /*
     * Count the elephants on the board, and check they add up.
     */
    ae = state.aplace + state.ahome;
    ie = state.iplace + state.ihome;
    for (i = 0; i < 59; i++) {
	if (state.board[i] == 1)
	    ae++;
	else if (state.board[i] == 2)
	    ie++;	
    }
    assert(ae == 2 && ie == 2);
}

/* ----------------------------------------------------------------------
 * Recursive game state evaluator.
 */

/*
 * Evaluate a position by making all possible moves. Returns the
 * maximum _advantage_ the active player can gain over the inactive
 * one. Expects `already_done' to point to an array of 93848734
 * signed chars containing the values of already-evaluated
 * positions, or 100 if that position has not already been
 * evaluated.
 */

int positions_checked;

struct move {
    int pos;			       /* elephant to move */
    int step;			       /* how many steps to move it by */
};

struct moves {
    struct move moves[4];
    int nmoves;
    int parade;
};

struct game_state flip(struct game_state state)
{
    int i, t;

    /*
     * Flip the game state into the other player's perspective.
     */
    for (i = 0; i < 59; i++)
	if (state.board[i])
	    state.board[i] = 3 - state.board[i];
    t = state.aplace; state.aplace = state.iplace; state.iplace = t;
    t = state.ahome; state.ahome = state.ihome; state.ihome = t;

    return state;
}

int evaluate(struct game_state state, signed char *already_done,
	     struct moves *outmoves);

int flipval(struct game_state state, signed char *already_done)
{
    /*
     * Flip the game state into the other player's perspective.
     */
    state = flip(state);

    /*
     * And negate the evaluation, since this will be from the
     * _other_ player's perspective.
     */
    return -evaluate(state, already_done, NULL);
}

/*
 * Given a game state and a move, make that move. If `newstate' is
 * true, simply return the resulting game state and the score
 * increments for each player. If `newstate' is false, instead pass
 * the resulting game state on to flipval to evaluate from the
 * opponent's perspective, and then return the value to the active
 * player of making that move.
 */
int moveval(struct game_state state, struct moves moves,
	    signed char *already_done, struct game_state *newstate,
	    int *a_score, int *i_score)
{
    int i, j, e, as, is;
    int ecount, hindmost, bitmask;
    int value = 0, logval;
    struct move *movep;

    as = is = 0;

    if (state.aplace > 0) {
	/*
	 * Special case: we're _placing_ elephants here, not moving
	 * them.
	 */
	if (moves.nmoves != 1 ||
	    moves.moves[0].pos < 0 || moves.moves[0].pos > 3 ||
	    state.board[moves.moves[0].pos] != 0 ||
	    moves.parade)
	    return -101;	       /* disallow move */
	state.board[moves.moves[0].pos] = 1;
	state.aplace--;
	goto done;
    }

    ecount = 0;
    hindmost = 59;
    for (i = 0; i < 59; i++)
	if (state.board[i]) {
	    hindmost = min(i, hindmost);
	    ecount++;
	}

    if (ecount == 0)
	return -101;		       /* no available move at all! */

    /*
     * Validate basic move: for a parade move there must be exactly
     * `ecount' moves (one per elephant), otherwise there must be
     * exactly min(ecount,3) moves.
     */
    if (moves.parade && moves.nmoves != ecount)
	return -101;
    if (!moves.parade && moves.nmoves != min(ecount, 3))
	return -101;

    /*
     * The moves must target distinct elephants, and elephants
     * already on the board, and in reverse order in parade mode.
     */
    for (i = 0; i < moves.nmoves; i++)
	if (!state.board[moves.moves[i].pos])
	    return -101;	       /* no elephant here */
    for (i = 0; i < moves.nmoves-1; i++)
	for (j = i+1; j < moves.nmoves; j++) {
	    if (moves.moves[i].pos == moves.moves[j].pos)
		return -101;	       /* moving same elephant twice */
	    if (moves.parade && moves.moves[i].pos < moves.moves[j].pos)
		return -101;	       /* malformed parade */
	}

    bitmask = 14;		       /* we can move by 1, 2 or 3 */
    movep = moves.moves;
    while (moves.nmoves-- > 0) {
	int pos = movep->pos;
	int step = movep->step;
	movep++;

	/*
	 * Disallow a move of a non-hindmost elephant on water,
	 * unless we're in elephant parade mode.
	 */
	if (!moves.parade && pos > hindmost && board[pos] == WATER)
	    return -101; /* extremely low value forces move to be discarded */

	if (moves.parade) {
	    /*
	     * Ensure all move distances are 1.
	     */
	    if (step != 1)
		return -101;
	} else {
	    /*
	     * Ensure each move distance is used at most once.
	     */
	    if (step < 1 || step > 3 || !(bitmask & (1 << step)))
		return -101;
	    bitmask &= ~(1 << step);
	}

	e = state.board[pos];
	assert(e != 0);
	state.board[pos] = 0;
	i = pos;
	while (step > 0) {
	    step--;
	    if (hindmost == i)
		hindmost++;
	    i++;

	    while (i < 59 && state.board[i]) {
		/*
		 * Hop over another elephant. In this case we leave
		 * hindmost where it is, because it now points to
		 * the elephant we jumped.
		 */
		i++;
	    }
	    /*
	     * The elephant's move terminates prematurely if we
	     * encounter water.
	     * 
	     * (This is a potential rule ambiguity: one could rule
	     * that it was illegal to `waste' a long move by having
	     * it cut short by water, unless there was no
	     * alternative. But this is apparently not the case,
	     * happily since it would be annoyingly hard to check
	     * that there was genuinely no alternative!)
	     */
	    if (i < 59 && board[i] == WATER && i != hindmost)
		break;
	}

	/*
	 * If we've reached the end of the board, log that.
	 */
	if (i >= 59) {
	    logval = 3 - (state.ahome + state.ihome);
	    if (e == 1) {
		state.ahome++;
 		value += logval;       /* we got the logs! */
		as += logval;
	    } else {
		state.ihome++;
		value -= logval;       /* _they_ got the logs, boo */
		is += logval;
	    }
	} else {
	    /*
	     * We haven't reached the end of the board, so put our
	     * elephant down again.
	     */
	    state.board[i] = e;
	    if (board[i] != WATER && board[i] != BLANK &&
		state.logs[board[i]]) {
		/*
		 * There are logs here.
		 */
		if (e == 1) {
		    value += 2;	       /* we got them */
		    as += 2;
		} else {
		    value -= 2;	       /* they got them */
		    is += 2;
		}
		state.logs[board[i]] = 0;
	    }
	}
    }

    done:
    if (newstate) {
	*newstate = flip(state);
	*a_score += as;
	*i_score += is;
	return 0;
    } else
	return value + flipval(state, already_done);
}

void printpos(int index, struct game_state state, int two_d,
	      int aletter, int bletter, int ascore, int bscore)
{
    int i, j, score = 1;
    char cboard[59];

    if (!aletter) {
	aletter = 'A';
	bletter = 'I';
	score = 0;
    }

    for (i = 0; i < 59; i++) {
	if (state.board[i] == 0 && board[i] == BLANK)
	    cboard[i] = '-';
	else if (state.board[i] == 0 && board[i] == WATER)
	    cboard[i] = '~';
	else if (state.board[i] == 0 && state.logs[board[i]])
	    cboard[i] = '=';
	else if (state.board[i] == 0)
	    cboard[i] = '-';
	else if (state.board[i] == 1)
	    cboard[i] = board[i] == WATER ? aletter | 0x20 : aletter;
	else
	    cboard[i] = board[i] == WATER ? bletter | 0x20 : bletter;
    }

    if (two_d) {
	static const int two_d[9][12] = {
	    {30,29,28,27,26,25,24,23,22,21,20,19},
	    {31,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,18},
	    {32,-1,56,55,54,53,52,51,50,49,-1,17},
	    {33,-1,57,-1,-1,-1,-1,-1,-1,48,-1,16},
	    {34,-1,58,-1,-1,-1,-1,-1,-1,47,-1,15},
	    {35,-1,-1,-1,-1,-1,-1,-1,-1,46,-1,14},
	    {36,37,38,39,40,41,42,43,44,45,-1,13},
	    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12},
	    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11},
	};

	for (j = 0; j < 9; j++) {
	    for (i = 0; i < 12; i++) {
		if (two_d[j][i] >= 0)
		    putchar(cboard[two_d[j][i]]);
		else
		    putchar(' ');
	    }
	    putchar('\n');
	}
	printf("%8d", index);
	if (score)
	    printf(" %c %2d-%-2d %c", aletter, ascore, bscore, bletter);

    } else {
	printf("%8d", index);
	if (score)
	    printf(" %c %2d-%-2d %c", aletter, ascore, bscore, bletter);
	printf(": ");
	for (i = 0; i < 59; i++)
	    putchar(cboard[i]);
    }
}

/*
 * Actual evaluation function. Given a game state, returns its
 * value, and optionally (if `outmoves' is non-null) also returns
 * one of the optimal moves.
 * 
 * If `outmoves' is null, this function will return the appropriate
 * byte from the `already_done' array instead of recursing, if that
 * byte is valid. If `outmoves' is not null, the function is forced
 * to do at least one level of proper running in order to test all
 * possible moves and see which is the best; but below _that_ it
 * will consult the `already_done' array.
 */
int evaluate(struct game_state state, signed char *already_done,
	     struct moves *outmoves)
{
    struct moves moves;
    int index, i, j;
    int val, bestval;

    /*
     * First see if this position already exists. If `outmoves' is
     * true, we're being asked for our move in this situation, so
     * we can't take this shortcut - but we will take it at the
     * next layer down.
     */
    index = marshal(state);
    if (!outmoves && already_done[index] != 100) {
#ifdef EVAL_DIAGNOSTICS_EXTRA
	printpos(index, state, 0, 0, 0, 0, 0);
	printf(" = %d\n", already_done[index]);
#endif
	return already_done[index];
    }

    /*
     * Failing that, we have to try every possible move from this
     * position and evaluate the result. To begin with, divide up
     * opening from endgame.
     */
    if (state.aplace > 0) {
	/*
	 * If so, we can place it on any of the first four
	 * positions which isn't already an elephant.
	 */
	bestval = -100;
	for (i = 0; i < 4; i++)
	    if (!state.board[i]) {
		moves.moves[0].pos = i;
		moves.moves[0].step = 0;     /* indicates placing an elephant */
		moves.parade = 0;
		moves.nmoves = 1;
		val = moveval(state, moves, already_done, NULL, NULL, NULL);
		bestval = max(val, bestval);
		if (outmoves && val == bestval)
		    *outmoves = moves;
	    }
	assert(bestval != -100);
    } else if (state.ahome == 2 && state.ihome == 2) {
	/*
	 * We have no remaining elephants on the board, and neither
	 * does the opponent. Recursion bottoms out here. Game
	 * over, value zero.
	 */
	bestval = 0;
#if 0
    } else if (state.ahome == 2) {
	/*
	 * One might rule that once both a player's elephants are
	 * home that player can no longer direct play. I don't
	 * think that's right, though; since players can move one
	 * another's elephants anyway, I see no reason the player
	 * with no elephants shouldn't continue to make moves in
	 * the hope of jumping the other player over some logs!
	 */
	bestval = flipval(state, already_done);
#endif
    } else {
	/*
	 * Main play. There are elephants on the board.
	 */
	int elephants[4];
	int ne;
	int permindex;

	/*
	 * One option is always to call Elephant Parade. In this we
	 * move every elephant along by one, including those in
	 * water, starting from the foremost (so that nothing jumps
	 * over anything else).
	 */
	j = 0;
	for (i = 59; i-- > 0 ;) {
	    if (state.board[i]) {
		moves.moves[j].pos = i;
		moves.moves[j].step = 1;
		j++;
	    }
	}
	assert(j <= 4);
	moves.nmoves = j;
	moves.parade = 1;
	bestval = moveval(state, moves, already_done, NULL, NULL, NULL);
	if (outmoves)
	    *outmoves = moves;

	/*
	 * Now we have a load of other moves to make. First, find
	 * the positions of all elephants on the board.
	 */
	for (i = ne = 0; i < 59; i++) {
	    if (state.board[i])
		elephants[ne++] = i;
	}

	/*
	 * We can make our 3,2,1 moves in any order, and pick the
	 * elephants to move in any order too.
	 */
	moves.parade = 0;
	for (permindex = 0; permindex < 6; permindex++) {
	    int m1, m2, m3;

	    switch (permindex) {
	      case 0: moves.moves[0].step=1; moves.moves[1].step=2; moves.moves[2].step=3; break;
	      case 1: moves.moves[0].step=1; moves.moves[1].step=3; moves.moves[2].step=2; break;
	      case 2: moves.moves[0].step=2; moves.moves[1].step=1; moves.moves[2].step=3; break;
	      case 3: moves.moves[0].step=2; moves.moves[1].step=3; moves.moves[2].step=1; break;
	      case 4: moves.moves[0].step=3; moves.moves[1].step=1; moves.moves[2].step=2; break;
	      case 5: moves.moves[0].step=3; moves.moves[1].step=2; moves.moves[2].step=1; break;
	    }

	    for (m1 = 0; m1 < ne; m1++) {
		moves.moves[0].pos = elephants[m1];
		if (ne == 1) {
		    /*
		     * This is the only elephant we can move.
		     */
		    moves.nmoves = 1;
		    val = moveval(state, moves, already_done, NULL, NULL, NULL);
		    bestval = max(val, bestval);
		    if (outmoves && val == bestval)
			*outmoves = moves;
		} else for (m2 = 0; m2 < ne; m2++) if (m2 != m1) {
		    moves.moves[1].pos = elephants[m2];
		    if (ne == 2) {
			/*
			 * These are the only two elephants we can
			 * move.
			 */
			moves.nmoves = 2;
			val = moveval(state, moves, already_done, NULL, NULL, NULL);
			bestval = max(val, bestval);
			if (outmoves && val == bestval)
			    *outmoves = moves;
		    } else for (m3 = 0; m3 < ne; m3++) {
			if (m3 != m2 && m3 != m1) {
			    moves.moves[2].pos = elephants[m3];
			    moves.nmoves = 3;
			    val = moveval(state, moves, already_done, NULL, NULL, NULL);
			    bestval = max(val, bestval);
			    if (outmoves && val == bestval)
				*outmoves = moves;
			}
		    }
		}
	    }
	}
    }

    /*
     * bestval is the value of this position if we make our best
     * move, and hence the value of this position overall. Store it
     * in already_done.
     */
#ifdef EVAL_DIAGNOSTICS
    printpos(index, state, 0, 0, 0, 0, 0);
    printf(" : %d\n", bestval);
#endif
    if (!outmoves) {
	already_done[index] = bestval;
	positions_checked++;
	if (positions_checked % 10000 == 0)
	    printf("done %d\n", positions_checked);
    }
    return bestval;
}

#if defined TEST_COMB

#define N 12

int main(void)
{
    int k, nck, i, j, p, fails = 0;
    char array[N], array2[N];

    /*
     * Test that every combination index does the right thing when
     * translated from index to combination back to index.
     */
    nck = 1;
    for (k = 0; k <= N; k++) {
	printf("n=%d k=%d, %d positions:\n", N, k, nck);

	for (i = 0; i < nck; i++) {
	    memset(array, 0, N);
	    comb_from_index(array, 1, 0, N, k, i);
	    j = comb_to_index(array, 2, 0, N, k);
	    printf("%4d -> ", i);
	    for (p = 0; p < N; p++)
		putchar(array[p] ? '#' : '-');
	    printf(" -> %4d", j);
	    if (j != i) {
		printf(" (FAIL!)");
		fails++;
	    }
	    putchar('\n');
	}

	nck = nck * (N-k) / (k+1);
    }

    /*
     * Now test that every bit pattern within the array does the
     * right thing when translated from combination to index back
     * to combination.
     */
    for (i = 0; i < 1<<N; i++) {
	k = 0;
	for (j = 0; j < N; j++) {
	    array[j] = (i & ((1<<(N-1)) >> j)) ? 1 : 0;
	    k += array[j];
	}
	for (p = 0; p < N; p++)
	    putchar(array[p] ? '#' : '-');

	j = comb_to_index(array, 2, 0, N, k);
	printf(" (k = %2d) -> %4d -> ", k, j);

	memset(array2, 0, N);
	comb_from_index(array2, 1, 0, N, k, j);

	for (p = 0; p < N; p++)
	    putchar(array2[p] ? '#' : '-');

	if (memcmp(array, array2, N)) {
	    printf(" (FAIL!)");
	    fails++;
	}

	putchar('\n');
    }

    printf("%d failures\n", fails);

    return 0;
}

#elif defined TEST_MARSHAL

int main(void)
{
    int i, j, fails = 0;

    for (i = 0; i < 93848734; i++) {
	struct game_state gs = unmarshal(i);
	validate(gs);
	j = marshal(gs);
	if (i != j) {
	    printf("marshal test failure: %d became %d\n", i, j);
	    fails++;
	}

	if (i % 10000 == 0)
	    printf("done %d\n", i);
    }

    printf("%d failures\n", fails);

    return 0;
}

#elif defined POSVIEWER

int main(int argc, char **argv)
{
    struct game_state state;
    int i, index;

    for (i = 1; i < argc; i++) {
	index = atoi(argv[i]);
	state = unmarshal(index);
	printpos(index, state, 0, 0, 0, 0, 0);
	putchar('\n');
    }

    return 0;
}

#else /* play engine is the default */

#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

void printmove(struct game_state state, struct moves moves,
	       int a_letter, int b_letter)
{
    int Af, Ab, If, Ib;
    int i;

    printf("%c plays:", a_letter ? a_letter : 'A');
    /*
     * Dump the move to stdout.
     */
    if (moves.moves[0].step == 0) {
	/* elephant placing */
	printf(" e%d\n", moves.moves[0].pos);
    } else if (moves.parade) {
	/* elephant parade */
	printf(" p\n");
    } else {
	Af = Ab = If = Ib = -1;
	for (i = 0; i < 59; i++) {
	    if (state.board[i] == 1) {
		Af = i;
		if (Ab < 0)
		    Ab = i;
	    } else if (state.board[i] == 2) {
		If = i;
		if (Ib < 0)
		    Ib = i;
	    }
	}
	for (i = 0; i < moves.nmoves; i++) {
	    if (moves.moves[i].pos == Af)
		printf(" %cf", a_letter ? a_letter : 'A');
	    else if (moves.moves[i].pos == Ab)
		printf(" %cb", a_letter ? a_letter : 'A');
	    else if (moves.moves[i].pos == If)
		printf(" %cf", b_letter ? b_letter : 'I');
	    else if (moves.moves[i].pos == Ib)
		printf(" %cb", b_letter ? b_letter : 'I');
	    else
		printf(" <%d?>", moves.moves[i].pos);
	    printf("%d", moves.moves[i].step);
	}
	printf("\n");
    }
}

int main(int argc, char **argv)
{
    signed char *data;

    int current_index;
    int a_letter, b_letter, a_score, b_score;
    int two_d = 0;
    char cmd[512];

    struct game_state state;

    /*
     * Command syntax:
     * 
     * 	- ep -e [filename] evaluates the game (~5 CPU hours on a
     * 	  2GHz machine) and writes out a 93Mb data file to
     * 	  `epeval.dat' or the specified filename if any.
     * 
     * 	- ep [filename] mmaps that file and uses it to present a
     * 	  user interface allowing you to inspect positions, enter
     * 	  moves, see evaluations of any position you view, and
     * 	  request an optimal move in a given circumstance.
     */
    if (argc > 1 && !strcmp(argv[1], "-e")) {
	int ret, pos;
	struct game_state state;
	char *fname;
	FILE *fp;

	data = malloc(93848734);

	state = unmarshal(0);

	memset(data, 100, 93848734);

	ret = evaluate(state, data, NULL);

	printf("Game value = %d\n", ret);

	if (argc > 2)
	    fname = argv[2];
	else
	    fname = "epeval.dat";

	fp = fopen(fname, "wb");
	pos = 0;
	while (pos < 93848734) {
	    ret = fwrite(data+pos, 1, 93848734-pos, fp);
	    if (ret <= 0) {
		perror("fwrite");
		return 1;
	    }
	    printf("wrote %d\n", ret);
	    pos += ret;
	}
	fclose(fp);

	return 0;
    }

    {
	int fd;
	char *fname;

	if (argc > 1)
	    fname = argv[1];
	else
	    fname = "epeval.dat";

        fd = open(fname, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "%s: open: %s\n", fname, strerror(errno));
            return 1;
        }

        data = mmap(NULL, 93848734, PROT_READ, MAP_SHARED, fd, 0);
        if (!data) {
            fprintf(stderr, "%s: mmap: %s\n", fname, strerror(errno));
            return 1;
        }
    }

    a_letter = b_letter = 0;
    a_score = b_score = 0;
    current_index = 0;

    printf("Type 'h' for help.\n");

    while (1) {
	state = unmarshal(current_index);
	printpos(current_index, state, two_d,
		 a_letter, b_letter, a_score, b_score);
	putchar('\n');

	/*
	 * Print position value.
	 */
	if (data[current_index] == 100) {
	    printf("Position unreachable by legal play\n");
	} else if (!a_letter) {
	    printf("Value to A: %d\n", data[current_index]);
	} else {
	    int margin = data[current_index] + a_score - b_score;
	    if (margin > 0) {
		printf("Projection: %c wins by %d\n", a_letter, margin);
	    } else if (margin == 0) {
		printf("Projection: a draw\n");
	    } else {
		printf("Projection: %c wins by %d\n", b_letter, -margin);
	    }
	}

	input:
	if (isatty(fileno(stdin))) {
	    printf("\n> ");
	    fflush(stdout);
	}
	if (!fgets(cmd, sizeof(cmd), stdin)) {
	    printf("\nGoodbye.\n");
	    break;
	}

	if (!cmd[0])
	    goto input;

	if (cmd[0] == 'h') {
	    printf("Commands:\n"
		   "  h             show this message\n"
		   "  s G 0 0 Y     set colour letters and current scores\n"
		   "  3974534       jump to a numbered game position\n"
		   "  39 G 0 0 Y    set position _and_ letters/scores\n"
		   "  Af2 Bb1 Ao3   make a specified move of three elephants\n"
		   "  p             make an elephant parade move\n"
		   "  e2            place an initial elephant at a position\n"
		   "  o             find optimal move and print it\n"
		   "  r             re-display current position\n"
		   "  d             toggle between 1-D and 2-D display\n");
	    goto input;
	}

	if (cmd[0] == 'r')
	    continue;		       /* redisplay */

	if (cmd[0] == 'd') {
	    two_d = !two_d;
	    continue;		       /* and redisplay */
	}

	if (isupper((unsigned char)cmd[0])) {
	    char *p = cmd;
	    struct moves moves;
	    struct game_state newstate;
	    int Af, Ab, If, Ib, A, I;
	    int i, t;

	    Af = Ab = If = Ib = -1;
	    for (i = 0; i < 59; i++) {
		if (state.board[i] == 1) {
		    Af = i;
		    if (Ab < 0)
			Ab = i;
		} else if (state.board[i] == 2) {
		    If = i;
		    if (Ib < 0)
			Ib = i;
		}
	    }

	    A = I = 0;

	    for (i = 0; i < 3; i++) {
		int side, which, step;

		while (*p && !isupper((unsigned char)*p)) p++;
		if (!*p) {
		    break;
		}
		side = (*p == a_letter ? 'A' :
			*p == b_letter ? 'I' :
			*p == 'A' ? 'A' : 'I');
		p++;
		while (*p && !islower((unsigned char)*p)) p++;
		if (!*p) {
		    printf("unable to find enough valid moves\n");
		    i = -1;
		    break;
		}
		if (side == 'A' ? A : I)
		    which = (side == 'A' ? A : I);   /* forced choice */
		else
		    which = (*p == 'f' ? 1 : 2);   /* user choice */
		p++;
		while (*p && !isdigit((unsigned char)*p)) p++;
		if (!*p) {
		    printf("unable to find enough valid moves\n");
		    i = -1;
		    break;
		}
		step = *p - '0';
		p++;
		moves.moves[i].pos = (side == 'A' && which == 1 ? Af :
				      side == 'A' && which == 2 ? Ab :
				      side == 'I' && which == 1 ? If : Ib);
		moves.moves[i].step = step;
	    }
	    if (i < 0)
		continue;

	    moves.nmoves = i;
	    moves.parade = 0;

	    i = moveval(state, moves, data, &newstate, &a_score, &b_score);
	    if (i <= -100) {
		printf("Illegal move\n");
	    } else {
		printmove(state, moves, a_letter, b_letter);

		current_index = marshal(newstate);
		/*
		 * And flip the letters and scores.
		 */
		t = a_letter; a_letter = b_letter; b_letter = t;
		t = a_score; a_score = b_score; b_score = t;
	    }

	    continue;
	}

	if (cmd[0] == 'p') {
	    struct moves moves;
	    struct game_state newstate;
	    int t, i, j;

	    j = 0;
	    for (i = 59; i-- > 0 ;) {
		if (state.board[i]) {
		    moves.moves[j].pos = i;
		    moves.moves[j].step = 1;
		    j++;
		}
	    }
	    assert(j <= 4);
	    moves.nmoves = j;
	    moves.parade = 1;

	    i = moveval(state, moves, data, &newstate, &a_score, &b_score);
	    if (i <= -100) {
		printf("Illegal move\n");
	    } else {
		printmove(state, moves, a_letter, b_letter);

		current_index = marshal(newstate);
		/*
		 * And flip the letters and scores.
		 */
		t = a_letter; a_letter = b_letter; b_letter = t;
		t = a_score; a_score = b_score; b_score = t;
	    }

	    continue;
	}

	if (cmd[0] == 'e') {
	    struct moves moves;
	    struct game_state newstate;
	    int i, t;

	    moves.nmoves = 1;
	    moves.parade = 0;
	    moves.moves[0].pos = atoi(cmd+1);
	    moves.moves[0].step = 0;

	    i = moveval(state, moves, data, &newstate, &a_score, &b_score);
	    if (i <= -100) {
		printf("Illegal move\n");
	    } else {
		printmove(state, moves, a_letter, b_letter);

		current_index = marshal(newstate);
		/*
		 * And flip the letters and scores.
		 */
		t = a_letter; a_letter = b_letter; b_letter = t;
		t = a_score; a_score = b_score; b_score = t;
	    }

	    continue;
	}

	if (cmd[0] == 'o') {
	    struct moves moves;
	    struct game_state newstate;
	    moves.nmoves = -1;	       /* placeholder */
	    evaluate(state, data, &moves);
	    if (moves.nmoves == -1)
		printf("No move available from this position (end of game)\n");
	    else {
		int i, t;

		i = moveval(state, moves, data, &newstate, &a_score, &b_score);
		assert(i > -100);

		printmove(state, moves, a_letter, b_letter);

		current_index = marshal(newstate);
		/*
		 * And flip the letters and scores.
		 */
		t = a_letter; a_letter = b_letter; b_letter = t;
		t = a_score; a_score = b_score; b_score = t;
	    }
	    continue;
	}

	if (cmd[0] == 's' || isdigit((unsigned char)cmd[0])) {
	    char *p = cmd;
	    int al, bl, as, bs;

	    if (*p == 's')
		p++;
	    else {
		current_index = atoi(p);
		while (*p && isdigit((unsigned char)*p)) p++;
		while (*p && isspace((unsigned char)*p)) p++;
		if (!*p)
		    continue;	       /* this isn't a syntax error */
	    }

	    while (*p && isspace((unsigned char)*p)) p++;
	    if (!*p || !isupper((unsigned char)*p))
		printf("Unable to find first colour letter after 's'\n");
	    else {
		al = *p++;
		while (*p && isspace((unsigned char)*p)) p++;
		if (!*p || !isdigit((unsigned char)*p))
		    printf("Unable to find first score after 's'\n");
		else {
		    as = atoi(p);
		    while (*p && isdigit((unsigned char)*p)) p++;
		    while (*p && (*p == '-' || isspace((unsigned char)*p)))
			p++;
		    if (!*p || !isdigit((unsigned char)*p))
			printf("Unable to find second score after 's'\n");
		    else {
			bs = atoi(p);
			while (*p && isdigit((unsigned char)*p)) p++;
			while (*p && isspace((unsigned char)*p)) p++;
			if (!*p || !isupper((unsigned char)*p))
			    printf("Unable to find second colour letter after 's'\n");
			else {
			    bl = *p++;
			    a_letter = al;
			    b_letter = bl;
			    a_score = as;
			    b_score = bs;
			}
		    }
		}
	    }
	    continue;
	}
    }
    return 0;
}

#endif
