#!/usr/bin/env python

# Solve the inverted-bottles and inverted-tins problems by
# exhaustive bfs over all possible strategies. The statements of the
# two puzzles are as follows:

# The bottle game
# ---------------
#
# You sit in a room, blindfolded. In front of you is a square table
# with four bottles on it, one at each corner. Each bottle is
# either the right way up or upside down.
#
# You are playing a game in which your aim is to finish a move with
# all four bottles the same way up: either all upside-down, or all
# right-way-up, it doesn't matter.
#
# Your move is to reach out to the table and grip two bottles; you
# may feel them to see which way up they are, and then you may turn
# either, neither or both of them the other way up, as you choose.
#
# After you make each move, a referee will rotate the table by an
# arbitrary multiple of 90 degrees (including, possibly, zero) in
# such a way that you don't know how the table has rotated. So in
# your next move, you have no way of knowing the relative positions
# of the bottles you gripped last time and the ones you grip this
# time.
#
# As soon as you end a move with the bottles in a winning state,
# the referee will tell you so and the game will end.
#
# The referee is also on the lookout for cheating. If you attempt
# to touch more than two bottles, or try to feel which way the
# table is turning, or any other means of cheating, the referee
# will notice and the game will be void.
#
# Your task is to devise a strategy which is _guaranteed_ to
# terminate, within a _fixed maximum_ number of moves, with all
# bottles the same way up, no matter what the referee does and no
# matter what the initial arrangement of the bottles was. What is
# the smallest number of moves in which this can be guaranteeably
# done, and what is the strategy that does it?

# The tin game
# ------------
#
# Instead of bottles, you are now playing exactly the same game
# with four baked-bean tins. This means you cannot tell which way
# up they are by touch (we assume they're the old-fashioned kind
# without ring pulls). Your only source of information is that
# after you make a move the referee either tells you you've won or
# doesn't.
#
# Your aim, as before, is to determine the smallest number of moves
# (and what they are) which can _guarantee_ victory.

# A possible _move_ consists of
#
#  - deciding whether to grip two opposite or two adjacent items
#  - deciding which way up the two end up, depending on which way
#    up they began.
#
# Thus, in the bottle game, there are 512 possible moves (a choice
# of four outcomes in each of the four input cases, times two for
# the initial opposite/adjacent choice). In the tin game we are
# restricted to making choices which do not depend on the input,
# which means we have only eight moves (four choices of inversion
# times the opposite/adjacent choice).

bottlemoves = range(512)
tinmoves = [0x0E4, 0x0B1, 0x04E, 0x01B, 0x1E4, 0x1B1, 0x14E, 0x11B]

def search(moves):
    # At any point in time, the game state consists of a set of
    # possible orientations for the four bottles/tins on the table.
    # There are 16 possibilities, so this is best represented as a
    # bitmask. To begin with, all 16 possibilities are permitted.
    #
    # The seventeenth possibility, represented by bit 16, is that
    # the game is already over. This is a sticky state.

    movelist = {}
    movelist[0xFFFF] = ()
    list = [0xFFFF]
    head = 0

    while head < len(list):
        # Pick a game state off the to-do list and process it.
        state = list[head]
        head = head+1

        # Loop over all possible moves.
        for move in moves:
            # Go through the possible table states in `state', and
            # build up the output state by making this move for
            # each.
            newstate = state & 0x10000 # preserve bit 16
            for i in range(16):
                if not (state & (1 << i)):
                    continue
                t = i
                # The first bottle we grip is bit 0.
                b1 = t & 1
                # The second is either 1 or 2, depending on whether
                # we gripped adjacent or opposite bottles, which is
                # determined by bit 8 of the move.
                if move & 0x100:
                    b2 = (t >> 2) & 1 # opposite
                else:
                    b2 = (t >> 1) & 1 # adjacent
                # Combine these two bottle states into a 2-bit
                # number.
                bb = b1 * 2 + b2
                # Look up the output bottle state in the move number.
                newbb = (move >> (2*bb)) & 3
                # Convert this back into two individual bottle states.
                newb1 = (newbb >> 1) & 1
                newb2 = newbb & 1
                # And put those back into t.
                t = (t &~ 1) | newb1
                if move & 0x100:
                    t = (t &~ 4) | (newb2 << 2) # opposite
                else:
                    t = (t &~ 2) | (newb2 << 1) # adjacent
                # Now we know the output table state given by
                # applying this move to the input state. Next we
                # simulate the referee's actions. Firstly, he
                # checks the win condition.
                if t == 0 or t == 15:
                    outmask = 0x10000 # game over!
                else:
                    # If the game isn't won, the referee rotates
                    # the table into one of the four possible
                    # states. So we must take t and rotate it four
                    # times.
                    outmask = 0
                    for j in range(4):
                        outmask = outmask | (1 << t)
                        t = ((t << 1) | (t >> 3)) & 15
                # Now we have the complete set of output table
                # states which result from applying this move to
                # this input table state.
                newstate = newstate | outmask

            # After looping over all possible table states, we have
            # determined the full game state which results from
            # applying this move to this input state.
            if not movelist.has_key(newstate):
                movelist[newstate] = movelist[state] + (move,)
                list.append(newstate)

    # Our search is done. Print the number of moves it took to get
    # to state 0x10000 (game guaranteeably won).
    print movelist[0x10000]

search(bottlemoves)
search(tinmoves)
