#!/usr/bin/env python

# Posted on rec.puzzles early in the morning of 2006-10-08:
#
# 1000 cards numbered 1 to 1000 are shuffled.
# One card at a time is dealt.
# You can take or leave each card as it is dealt.
# After you have taken your first card, you can only take a card if it is
# higher than the last one you took.
# The aim is to take as many as you can.
# 
# You are free to note dealt cards, use computers, charts, tables or
# chicken entrails to decide on action for each card as it comes.
# 
# How many could you expect to take?

from rational import Rational

take = {}
fn = {0:0}

fact = 1
for n in range(1,1001):
    # We now model the situation in which there are n cards
    # remaining in the deck _that we can take_. The expected number
    # of cards we can take with optimal play from this position is
    # fn[n].

    # We completely ignore all the cards we _can't_ take; they are
    # discarded from the deck as soon as they are dealt and we
    # don't get to make a choice about them. So there are n
    # possible _interesting_ cards which could come up next. We
    # iterate over each of them.

    expsum = 0

    mult = 1

    smallesttakek = None

    for k in range(n-1,-1,-1):
        # k is the index of the card turned up, counting from 0 at
        # the top. In other words, if we take this card, there will
        # be k remaining cards we can take. Therefore, our expected
        # gain by taking this card will be 1 + fn[k].
        takegain = fact + fn[k] * mult
        mult = mult * k

        # If we _don't_ take this card, then there will be n-1
        # cards we can take, for which our expected gain is fn[n-1].
        dropgain = fn[n-1]

        #print "!", n, k, takegain, dropgain

        # Whichever of those gives us the bigger expected gain, we
        # do.
        if takegain > dropgain:
            expgain = takegain
            take[(n,k)] = 1
            smallesttakek = k
        else:
            expgain = dropgain
            take[(n,k)] = 0

        expsum = expsum + expgain

    fact = fact * n

    # Having summed the expected gains from all possible deals, we
    # now divide by n to find the _average_ expected gain.
    exp = expsum

    # And store it in the fn array.
    fn[n] = exp

    print n, smallesttakek, 1 + n - smallesttakek, Rational(exp,fact).decrep(20)
    #print n, fn[n]
    

#tk = take.keys()
#tk.sort()
#for t in tk:
#    print t, take[t]
