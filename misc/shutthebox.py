#!/usr/bin/env python

# Program to analyse the dice solitaire game `Shut the Box'.

# In this game you start with nine flaps, all in the Up position,
# labelled 1..9. In each turn you roll two dice. The numbers on the
# dice give rise to a set of possible moves (see below). Each move
# involves flipping one or more Up flaps into the Down position;
# _all_ the flaps involved in the move must be Up for the move to
# be valid. Your aim is to end a move with all flaps Down; if at
# any point you have no valid move, you lose.
#
# I know of two variant rules about what moves are allowable on a
# given roll.
#
#  - Sally's variant: You must use each die as a unit (like
#    backgammon), but may add them together, or may use them both
#    separately. You don't have to use both dice - you can choose
#    to use only one, at your whim. Thus, if you roll 5 and 2, you
#    may flip down 2, or 5, or 2+5=7, or _both_ 2 and 5 - but even
#    if both 2 and 5 are still up, you may still choose to flip
#    only one.
#
#  - Becky's variant: Only the sum of the two dice is significant.
#    You must flip down a set of numbers whose sum is precisely
#    that total. Thus, if you roll 5 and 2, or _anything_ else
#    totalling 7, then you must flip down numbers totalling 7, so
#    your possible moves are 7, 6-1, 5-2, 4-3 or 4-2-1.

import sys

class sally:
    name = "Sally's variant"
    d = 36

    dierolls = []
    for i in range(6,0,-1):
	for j in range(i,0,-1):
	    s = str(i) + "-" + str(j)
	    if i == j:
		n = 1
	    else:
		n = 2
	    dierolls.append((s, (i, j), n))

    def moves(roll):
	i, j = roll
	list = []
	list.append((i,))
	if j != i:
	    list.append((j,))
	    list.append((i, j))
	if i + j <= 9:
	    list.append((i+j,))
	return list

    movelist = {}
    for s, r, n in dierolls:
	movelist[r] = moves(r)

class becky:
    name = "Becky's variant"

    d = 36
    dierolls = [
    ("2", 2, 1),
    ("3", 3, 2),
    ("4", 4, 3),
    ("5", 5, 4),
    ("6", 6, 5),
    ("7", 7, 6),
    ("8", 8, 5),
    ("9", 9, 4),
    ("10", 10, 3),
    ("11", 11, 2),
    ("12", 12, 1)]

    def moves(roll):
	def recurse(n, total, maximum, prefix, list):
	    # Recursively find all sets of n distinct numbers, all
	    # at most `maximum' and summing to `total'. Construct
	    # each set as a tuple, prepend `prefix' and stick it on
	    # the end of `list'.
	    if total <= 0 or maximum <= 0 or n <= 0:
		return # silly special cases
	    if n == 1:
		# Only one possibility.
		if total <= maximum:
		    list.append(prefix + (total,))
	    else:
		# Go through all possibilities for the _largest_
		# number.
		for i in range(maximum, 0, -1):
		    recurse(n-1, total-i, i-1, prefix+(i,), list)

	list = []
	for n in range(1, 12):
	    recurse(n, roll, 9, (), list)
	return list

    movelist = {}
    for s, r, n in dierolls:
	movelist[r] = moves(r)

def movestr(move):
    s = ""
    h = ""
    for m in move:
	s = s + h + str(m)
	h = "-"
    return s

class nullfile:
    def write(*a):
	pass

def analyse(var, how, file):
    # Analyse a given game variant.

    # First prepare a list of all game states, sorted by number of
    # raised flaps (thus any state is listed _after_ all states
    # that can be reached from it).
    states = []
    for g in range(1, 1<<9):
	state = ()
	count = 0
	name = ""
	for i in range(1, 10):
	    if g & (1 << (i-1)):
		state = state + (i,)
		name = name + str(i)
		count = count + 1
	    else:
		name = name + "_"
	states.append((count, state, name))
    states.sort()

    winprob = {}
    winprob[()] = 1.0 # special case: total win

    # Now work through each state in the given order, figuring out
    # the optimal strategy and the winning probability in each one.
    for count, state, name in states:
	# Header.
	file.write("  " + name + ":\n")

	ptotal = 0

	# For each possible die roll...
	for s, r, n in var.dierolls:
	    # ... try every possible move.
	    file.write("    " + s + ":")
	    possibles = []
	    for move in var.movelist[r]:
		# First see if the move is valid, and also find the
		# state it leaves us in if so.
		valid = 1
		newstate = list(state)
		for i in move:
		    if not (i in newstate):
			valid = 0
			break
		    newstate.remove(i)
		if not valid:
		    continue

		# We have a valid move.
		possibles.append((winprob[tuple(newstate)], move, newstate))

	    # Sort the possible moves into descending order of
	    # goodness.
	    possibles.sort()
	    possibles.reverse()

	    # Determine the probability of winning the game from
	    # this state on this die roll.
	    if how == "best":
		# Choose the best possibility.
		if len(possibles) == 0:
		    p = 0 # no valid move, we lose
		else:
		    p = possibles[0][0]
	    elif how == "random":
		# Choose a random possibility. I.e. our probability
		# averages all the possible moves.
		p = 0
		for wp, move, newstate in possibles:
		    p = p + wp
		if len(possibles) > 0:
		    p = p / len(possibles)

	    # Add this to the average win probability for this
	    # state.
	    ptotal = ptotal + p * n

	    # Output the list of possible moves in order.
	    if len(possibles) == 0:
		file.write(" LOSE\n")
	    else:
		for wp, move, newstate in possibles:
		    file.write(" %s(%.3f)" % (movestr(move), wp))
		file.write("\n")

	# Now determine the overall win probability.
	winprob[state] = ptotal / var.d
	file.write("    Win chance: %.3f\n\n" % winprob[state])

    # Return value of the analyse routine is the overall winning
    # probability.
    return winprob[(1,2,3,4,5,6,7,8,9)]

for variant in sally, becky:
    title = "Analysis of " + variant.name
    print title
    print "=" * len(title)
    print
    # Print out a full strategic analysis of the variant.
    bestprob = analyse(variant, "best", sys.stdout)
    # Find the probability of a random player winning.
    randprob = analyse(variant, "random", nullfile())
    print "  Winning probability with perfect play = %.3f" % bestprob
    print "  Winning probability with random play  = %.3f" % randprob
    print "  Skill factor = %.3f" % (bestprob / randprob)
    print
