import sys
import os
import string
import termios
import TERMIOS

try:
    origattr = termios.tcgetattr(sys.stdin.fileno())
except termios.error:
    origattr = [0,0,0,0,0,0,[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]
charattr = origattr[:]  # this copies, where `charattr = origattr' would link
charattr[3] = charattr[3] & ~TERMIOS.ECHO & ~TERMIOS.ICANON
charattr[6][TERMIOS.VSTOP] = 0

def charmodeon(file=sys.stdin):
    try:
	termios.tcsetattr(file.fileno(), TERMIOS.TCSANOW, charattr)
    except termios.error:
	nop = 0

def charmodeoff(file=sys.stdin):
    try:
	termios.tcsetattr(file.fileno(), TERMIOS.TCSANOW, origattr)
    except termios.error:
	nop = 0

def cls(file=sys.stdout):
    file.write("\033[2J\033[H")

def beep(file=sys.stdout):
    file.write("\007")
    file.flush()

def home(file=sys.stdout):
    file.write("\033[H")

def goto(x, y, file=sys.stdout):
    file.write("\033[" + "%d" % (y+1) + ";" + "%d" % (x+1) + "H")

def insline(file=sys.stdout):
    file.write("\033[L")

def prt(file=sys.stdout):
    file.write("\033[2J\033[H")

def getsize():
    # This is a gross hack. ioctl would work better. A signal handler would
    # be better still!
    r = os.popen("stty size")
    s = r.readline()
    r.close()
    w = string.split(s)
    return string.atoi(w[1]), string.atoi(w[0])
