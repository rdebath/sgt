# Bresenham circle drawing
# 
# You use it to draw 1/8 of the circle - say from the topmost point
# to the 45 degree point to the right - and then you can replicate
# that by reflection and rotation into the other seven octants.
# 
# The way it works is that you're plotting y such that y^2 is close
# to r^2-x^2, and you always keep track of the _difference_ D
# between y^2 and r^2-x^2.
# 
# So to start with, you plot at (0,r), so that y=r, x=0 and D=0.
# 
# Next, you want to work out the correct y for x+1. Now (x+1)^2
# equals x^2+2x+1, so the new value of (r^2-x^2) will be 2x+1 less
# than the old one - so D will go up by 2x+1. Now in this octant,
# you know that you'll always be either decrementing y by one, or
# leaving it alone; (y-1)^2 = y^2-2y+1, so _if_ you decrement y
# then the new value of D will go down by 2y-1.
# 
# So you add 2x+1 to D, and then consider whether leaving it alone
# or subtracting 2y-1 will make it end up closer to zero. If the
# latter, you do so, and decrement y. Then increment x, plot the
# point, and repeat. The termination condition comes when x>y,
# since then you've crossed over into the next octant.
#
# Since the algorithm is effectively rounding to nearest rather
# than up or down, it doesn't suffer from little blobs at the
# compass points or from excessively flat sides.

array = []
xoff = 0
yoff = 0

def pixel(x,y):
    global array, xoff, yoff
    x = x + xoff
    y = y + yoff
    s = array[y]
    s = s[:x] + "*" + s[x+1:]
    array[y] = s

def plot(x,y):
    pixel(x,y)
    pixel(x,-y)
    pixel(-x,y)
    pixel(-x,-y)
    pixel(y,x)
    pixel(y,-x)
    pixel(-y,x)
    pixel(-y,-x)

def bres(r):
    y = r
    x = 0
    D = 0
    while y >= x:
	plot(x,y)
	D = D + (2*x + 1)
	D2 = D - (2*y - 1)
	if abs(D2) < abs(D):
	    D = D2
	    y = y - 1
	x = x + 1

def circle(r):
    global array, xoff, yoff
    array = [" " * (2*r+1)] * (2*r+1)
    xoff = yoff = r
    bres(r)
    for s in array:
	print s

circle(35)
