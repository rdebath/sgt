#!/usr/bin/env python

# Exhaustively analyse variants of the word game `Ghost', given a
# dictionary on standard input.

import sys
import string

def ghost_moves(s):
    ret = []
    for c in string.lowercase:
        ret.append(s+c)
    return ret

def ghost_subwords(s):
    ret = []
    for i in xrange(len(s)):
            ret.append(s[:i])
    return ret

def superghost_moves(s):
    ret = []
    for c in string.lowercase:
        ret.append(s+c)
        ret.append(c+s)
    return ret

def superghost_subwords(s):
    ret = []
    for i in xrange(len(s)):
        for j in xrange(i, len(s)+1):
            ret.append(s[i:j])
    return ret

ghost = [ghost_moves, ghost_subwords]
superghost = [superghost_moves, superghost_subwords]

def analyse(variant, getword, nplayers):
    moves, subwords = variant

    dicts = []
    
    # Read the dictionary and build up a list of all the words and
    # subwords which are relevant to the analysis, sorted by length.
    while 1:
        s = getword()
        if s == None: break
        if len(s) < 4: continue # Ghost sets a hard lower limit of 4 letters

        while len(dicts) < len(s)+1:
            dicts = dicts + [{}]

        for ss in subwords(s):
            if not dicts[len(ss)].has_key(ss):
                dicts[len(ss)][ss] = 0
        dicts[len(s)][s] = 1

    # Now go through that list in descending order of length,
    # determining the value of each position.
    #
    # For two players, the value of a position is very simply
    # determined. A completed word (dicts[len(w)][w] == 1) is a win
    # for one player or the other depending on the parity of its
    # length; an incomplete word is a win for the player whose turn
    # it is if any move leads to a position already labelled as a
    # win for that player, otherwise it's a win for the other
    # player.
    #
    # With three or more players it becomes harder. An exact
    # analysis of the game would be possible if we were given
    # models of the players' strategic goals (does player 2 prefer
    # to kill player 3 or player 1?); in the absence of that
    # information, we can only draw approximate conclusions most of
    # the time because one cannot _rely_ on what another player
    # will do when presented with more than one equally
    # self-interested choice. Nonetheless there are models under
    # which we can sensibly analyse a multi-player game:
    #
    #  - One-vs-all. In this model we assume that one player is
    #    playing against a fully cooperating team containing all
    #    the others. This reduces the game to a two-player one in
    #    which one `player' gets more moves than another. This
    #    analysis must be performed separately for each player
    #    position.
    #
    #    Any position tagged by this analysis as a win for the
    #    single player is one in which that player can ensure they
    #    do not lose the round no matter _what_ all the other
    #    players do.
    #
    #  - Self-interested. In this model we assume that each player
    #    is primarily out to avoid being the loser. Thus we tag
    #    each position with the _set_ of players who might lose if
    #    that position is reached. For a completed word this is a
    #    one-element set; otherwise, you look at the sets for the
    #    positions reached by each possible move. The set of
    #    possible losers from this position is the union of all
    #    sets for subsequent positions which _don't_ include the
    #    current player, if there is at least one such set.
    #    Otherwise it's the union of absolutely all the
    #    subsequent-position sets. This models the idea that
    #    players will always choose a move which prevents them
    #    losing over a move which does not, but that no player may
    #    rely on what any other player will do _apart_ from this.
    #    This analysis need be performed only once.
    #
    #    Any position tagged by this analysis as a non-loss for a
    #    given player is one in which that player can avoid losing
    #    provided other players play with rational self-interest.
    #    If the position is not also tagged as a win for that
    #    player in the one-vs-all model, then the player is
    #    _dependent_ on the other players playing with rational
    #    self-interest, and might therefore do well to point their
    #    self-interest out to them at any point when it looks as if
    #    they might not have spotted it!

    # dicts[len(w)][w] starts off as either 0 or 1 depending on
    # whether the position is a partial or completed word. As we go
    # through, we alter it to be a compendium of analysis results
    # for that position. Specifically, it's an array containing:
    #
    #  - `nplayers' values for one-vs-all analysis, each of which
    #    is either 0 (this position is a guaranteed non-loss for
    #    that player) or 1 (this position is a loss for that player
    #    if the other players collude)
    #
    #  - one more value for self-interested analysis, which is a
    #    bitmask in which bit (1<<i) is set iff player i can lose
    #    from this position.

    for length in xrange(len(dicts)-1, -1, -1):
        for word in dicts[length].keys():
            if dicts[length][word] == 1:
                # Completed word. Set up the position value based
                # on the word length mod nplayers.
                #
                # Note that player zero is the first person to
                # play; thus they lose on a length-1 word, not a
                # length-0 word.
                loser = (length+nplayers-1) % nplayers
                pval = [0] * nplayers
                pval[loser] = 1
                pval.append(1L << loser)
            else:
                # Incomplete word. Try all possible moves and
                # amalgamate.
                thisplayer = length % nplayers
                movelist = []
                for m in moves(word):
                    if dicts[length+1].has_key(m):
                        movelist.append(dicts[length+1][m])
                # The Ghost move-validity criterion is that there
                # must be a valid following move from any
                # incomplete word.
                if len(movelist) == 0:
                    print word
                    raise "argh!"
                pval = []
                for player in xrange(nplayers):
                    # Determine the one-vs-all value for this
                    # player.
                    if player == thisplayer:
                        # The solo player is the current one. So
                        # iff any subsequent position is tagged 0,
                        # then so is this position.
                        val = reduce(lambda x,y:x&y, [m[player] for m in movelist])
                    else:
                        # The solo player is not the current one.
                        # So iff any subsequent position is tagged
                        # 1, then so is this position.
                        val = reduce(lambda x,y:x|y, [m[player] for m in movelist])
                    pval.append(val)
                # Now determine the self-interested value.
                badset = goodset = 0
                thisplayerbit = 1L << thisplayer
                for m in movelist:
                    set = m[nplayers]
                    if set & thisplayerbit:
                        badset |= set
                    else:
                        goodset |= set
                if goodset:
                    val = goodset
                else:
                    val = badset
                pval.append(val)
            dicts[length][word] = pval


    # We're done. Print out the values for the first few positions,
    # out of general interest.
    for length in range(5):
        wordlist = dicts[length].keys()
        wordlist.sort()
        for word in wordlist:
            print word, dicts[length][word]

def stdin_getword():
    while 1:
        s = sys.stdin.readline()
        if s == "": return None
        while s[-1:] in string.whitespace: s = s[:-1]
        while s[:1] in string.whitespace: s = s[1:]
        if s == "": continue
        if ord(s[0]) < 32: continue
        return string.lower(s)

analyse(superghost, stdin_getword, 4)
