#!/usr/bin/env python

# Clone of the FreeNet game at http://www.jurjans.lv/stuff/net/FreeNet.htm

from gtk import *
import GDK
import random
import md5
import math
import string

U, L, D, R, PWR, ACTIVE, LOCK = 1, 2, 4, 8, 16, 32, 64

SQUARESIZE = 31
NSQUARES_X = 5
NSQUARES_Y = 5
WRAPPING = FALSE
BARRIER_RATE = 0

darea = pix = None

def setup_globals():
    global PWRSIZ, BORDER, ORIGIN, TOTALSIZE_X, TOTALSIZE_Y, pix
    PWRSIZ = SQUARESIZE / 4
    BORDER = SQUARESIZE / 2
    ORIGIN = BORDER
    TOTALSIZE_X = 2*BORDER + NSQUARES_X * SQUARESIZE
    TOTALSIZE_Y = 2*BORDER + NSQUARES_Y * SQUARESIZE
    if darea:
        darea.set_usize(TOTALSIZE_X, TOTALSIZE_Y)
    if pix:
        pix = create_pixmap(win, TOTALSIZE_X, TOTALSIZE_Y)
setup_globals()

def rotate(flags, n):
    # Rotate `flags' n places clockwise.
    fd = flags & 0xF
    ft = flags ^ fd
    fd = (fd + (fd << 4)) >> (n & 3)
    return ft | (fd & 0xF)

def validate(game):
    # Any integer is a valid game seed.
    return 1

class PredictableRNG:
    # I'm going to use MD5, which is way overkill but easy to remember!
    def __init__(self, seed):
        self.seedstr = str(seed)
        if self.seedstr[-1:] == "L":
            self.seedstr = self.seedstr[:-1]
        self.databuf = ""
        self.sequence = 0L

    def getbyte(self):
        if self.databuf == "":
            seqstr = str(self.sequence)
            if seqstr[-1:] == "L":
                seqstr = seqstr[:-1]
            self.databuf = md5.md5(self.seedstr + "/" + seqstr).digest()
            self.sequence = self.sequence + 1
        ret = ord(self.databuf[0])
        self.databuf = self.databuf[1:]
        return ret & 0xFF

    def getint(self, maximum):
        bytes = 1
        limit = 0x100L
        while limit < maximum:
            bytes = bytes + 1
            limit = limit * 0x100L
        exactlimit = limit - (limit % maximum)
        while 1:
            number = 0
            for i in range(bytes):
                number = number * 0x100L + self.getbyte()
            if number >= exactlimit:
                continue
            break
        ret = number % maximum
        if type(maximum) == type(1):
            return int(ret)
        else:
            return ret

class NetGame:
    def __init__(self, game):
        if game == None:
            game = random.randint(0, 0xFFFF) * 0x10000L
            game = game + random.randint(0, 0xFFFF)
        self.currgame = game

        # Initialise game from seed.
        rng = PredictableRNG(self.currgame)
        self.arena = []
        for x in range(NSQUARES_X):
            self.arena.append([0] * NSQUARES_Y)
        self.arena[NSQUARES_X/2][NSQUARES_Y/2] = PWR
        # Repeatedly go through the grid, find all the
        # possibilities for extending into a new empty square, pick
        # one randomly, and do so.
        if WRAPPING:
            wrapoffset = 0
        else:
            wrapoffset = 1
        while 1:
            possibilities = []
            for x in range(NSQUARES_X):
                for y in range(NSQUARES_Y-wrapoffset):
                    y1 = (y+1) % NSQUARES_Y
                    if self.arena[x][y] == 0 and self.arena[x][y1] != 0 \
                    and (self.arena[x][y1] & (L|R|D)) != (L|R|D):
                        possibilities.append((x, y1, U, x, y, D))
                    if self.arena[x][y] != 0 and self.arena[x][y1] == 0 \
                    and (self.arena[x][y] & (L|R|U)) != (L|R|U):
                        possibilities.append((x, y, D, x, y1, U))
            for x in range(NSQUARES_X-wrapoffset):
                for y in range(NSQUARES_Y):
                    x1 = (x+1) % NSQUARES_X
                    if self.arena[x][y] == 0 and self.arena[x1][y] != 0 \
                    and (self.arena[x1][y] & (U|R|D)) != (U|R|D):
                        possibilities.append((x1, y, L, x, y, R))
                    if self.arena[x][y] != 0 and self.arena[x1][y] == 0 \
                    and (self.arena[x][y] & (U|L|D)) != (U|L|D):
                        possibilities.append((x, y, R, x1, y, L))
            if len(possibilities) == 0:
                break
            choice = rng.getint(len(possibilities))
            sx, sy, sd, dx, dy, dd = possibilities[choice]
            self.arena[sx][sy] = self.arena[sx][sy] | sd
            self.arena[dx][dy] = self.arena[dx][dy] | dd

        # Compute the list of possible locations for barriers
        # between squares, as clues.
        barrierlist = []
        for x in range(NSQUARES_X):
            for y in range(NSQUARES_Y-wrapoffset):
                y1 = (y+1) % NSQUARES_Y
                if not (self.arena[x][y] & D):
                    barrierlist.append((x,y,x,y1))
        for x in range(NSQUARES_X-wrapoffset):
            for y in range(NSQUARES_Y):
                x1 = (x+1) % NSQUARES_X
                if not (self.arena[x][y] & R):
                    barrierlist.append((x,y,x1,y))

        # Now we've got the network, scramble it.
        for x in range(NSQUARES_X):
            for y in range(NSQUARES_Y):
                self.arena[x][y] = rotate(self.arena[x][y], rng.getint(4))

        # Select actual barriers to go in the squares. Doing this
        # _after_ scrambling (though we had to work out the list of
        # potential barrier locations before) ensures that puzzles
        # of the same seed and parameters but with different
        # barrier rates are otherwise identical. Also, the method
        # of selection here ensures that raising the barrier rate
        # creates barrier sets which are supersets of those at
        # lower rates - so you can't, for example, try rates 2,3,4
        # and expect to get _different_ non-overlapping sets of
        # barriers.
        self.barriers = {}
        nbarriers = len(barrierlist) * BARRIER_RATE / 100
        for i in range(nbarriers):
            index = rng.getint(len(barrierlist))
            self.barriers[barrierlist[index]] = 1
            del barrierlist[index]

        # Compute the ACTIVE bits.
        self.compute_active()

        self.moves = []
        self.moveinprogress = FALSE
        self.fullredraw = TRUE
        self.completed = FALSE
        self.completion_animation = 0

    def barrier(self, x1, y1, x2, y2):
        return self.barriers.get((x1,y1,x2,y2))

    def compute_active(self):
        list = []
        def visit(x, y, self=self, list=list):
            self.arena[x][y] = self.arena[x][y] | ACTIVE

            xm1 = (x + NSQUARES_X-1) % NSQUARES_X
            if x == 0 and not WRAPPING: xm1 = None
            ym1 = (y + NSQUARES_Y-1) % NSQUARES_Y
            if y == 0 and not WRAPPING: ym1 = None
            xp1 = (x + 1) % NSQUARES_X
            if xp1 == 0 and not WRAPPING: xp1 = None
            yp1 = (y + 1) % NSQUARES_Y
            if yp1 == 0 and not WRAPPING: yp1 = None

            if xm1 != None and (self.arena[x][y] & L) and \
            (self.arena[xm1][y] & R) and not self.barrier(xm1,y,x,y) and \
            not (self.arena[xm1][y] & ACTIVE):
                list.append((xm1, y))
            if ym1 != None and (self.arena[x][y] & U) and \
            (self.arena[x][ym1] & D) and not self.barrier(x,ym1,x,y) and \
            not (self.arena[x][ym1] & ACTIVE):
                list.append((x, ym1))
            if xp1 != None and (self.arena[x][y] & R) and \
            (self.arena[xp1][y] & L) and not self.barrier(x,y,xp1,y) and \
            not (self.arena[xp1][y] & ACTIVE):
                list.append((xp1, y))
            if yp1 != None and (self.arena[x][y] & D) and \
            (self.arena[x][yp1] & U) and not self.barrier(x,y,x,yp1) and \
            not (self.arena[x][yp1] & ACTIVE):
                list.append((x, yp1))

        for x in range(NSQUARES_X):
            for y in range(NSQUARES_Y):
                self.arena[x][y] = self.arena[x][y] & ~ACTIVE

        for x in range(NSQUARES_X):
            for y in range(NSQUARES_Y):
                if self.arena[x][y] & PWR:
                    visit(x, y)
        while len(list) > 0:
            x, y = list.pop()
            visit(x, y)

        completed = TRUE
        self.activesquares = self.totalsquares = NSQUARES_X * NSQUARES_Y
        for x in range(NSQUARES_X):
            for y in range(NSQUARES_Y):
                if not (self.arena[x][y] & ACTIVE):
                    completed = FALSE
                    self.activesquares = self.activesquares - 1
        if completed:
            self.completed = TRUE
            self.completion_point = len(self.moves)

    def draw(self, pixmap):
	class container:
	    pass
	cont = container()
        def draw_thick_line(pix, gc, x1, y1, x2, y2, active, cont=cont):
	    if cont.phase <= 0:
		draw_line(pix, gc, x1-1, y1, x2-1, y2)
		draw_line(pix, gc, x1+1, y1, x2+1, y2)
		draw_line(pix, gc, x1, y1-1, x2, y2-1)
		draw_line(pix, gc, x1, y1+1, x2, y2+1)
	    elif cont.phase < 0 or cont.phase == 1:
		tmp = gc.foreground
		if active:
		    gc.foreground = powered
		draw_line(pix, gc, x1, y1, x2, y2)
		gc.foreground = tmp

        self.bbox = [None, None, None, None]

        gc = pix.new_gc()
        gc.foreground = grey
        draw_rectangle(pix, gc, TRUE, 0, 0, TOTALSIZE_X, TOTALSIZE_Y)

	for x in range(NSQUARES_X):
	    for y in range(NSQUARES_Y):
		if self.arena[x][y] & LOCK:
		    gc.foreground = lockcolour
		else:
		    gc.foreground = grey
		draw_rectangle(pix, gc, TRUE, ORIGIN + SQUARESIZE * x, \
		ORIGIN + SQUARESIZE * y, SQUARESIZE, SQUARESIZE)

        gc.foreground = borders

        for x in range(NSQUARES_X+1):
            draw_line(pix, gc, ORIGIN + SQUARESIZE * x, ORIGIN, \
            ORIGIN + SQUARESIZE * x, ORIGIN + SQUARESIZE * NSQUARES_Y)
        for y in range(NSQUARES_Y+1):
            draw_line(pix, gc, ORIGIN, ORIGIN + SQUARESIZE * y, \
            ORIGIN + SQUARESIZE * NSQUARES_X, ORIGIN + SQUARESIZE * y)

	for cont.phase in range(3):
	    for x in range(NSQUARES_X):
		for y in range(NSQUARES_Y):
		    gc.foreground = lines
		    k = self.arena[x][y]
		    act = k & ACTIVE
		    k = k & ~(ACTIVE | LOCK)

		    r = (SQUARESIZE-1) / 2
		    xmid = ORIGIN + SQUARESIZE * x + r
		    ymid = ORIGIN + SQUARESIZE * y + r

		    matrix = [[1,0],[0,1]]
		    if self.moveinprogress:
			if self.movex == x and self.movey == y:
			    angle = self.moveprogress*self.moveact*math.pi/2
			    matrix = [[math.cos(angle),-math.sin(angle)], \
			    [math.sin(angle),math.cos(angle)]]
			    self.bbox = [xmid-r-1,ymid-r-1,xmid+r+1,ymid+r+1]

		    if k & U:
			ex, ey = 0, -r
			ex, ey = int(matrix[0][0] * ex + matrix[0][1] * ey), \
			int(matrix[1][0] * ex + matrix[1][1] * ey)
			draw_thick_line(pix, gc, xmid, ymid, xmid+ex, ymid+ey, act)
		    if k & D:
			ex, ey = 0, +r
			ex, ey = int(matrix[0][0] * ex + matrix[0][1] * ey), \
			int(matrix[1][0] * ex + matrix[1][1] * ey)
			draw_thick_line(pix, gc, xmid, ymid, xmid+ex, ymid+ey, act)
		    if k & L:
			ex, ey = -r, 0
			ex, ey = int(matrix[0][0] * ex + matrix[0][1] * ey), \
			int(matrix[1][0] * ex + matrix[1][1] * ey)
			draw_thick_line(pix, gc, xmid, ymid, xmid+ex, ymid+ey, act)
		    if k & R:
			ex, ey = +r, 0
			ex, ey = int(matrix[0][0] * ex + matrix[0][1] * ey), \
			int(matrix[1][0] * ex + matrix[1][1] * ey)
			draw_thick_line(pix, gc, xmid, ymid, xmid+ex, ymid+ey, act)
		    if cont.phase == 2 and (k == U or k == D or k == L or k == R):
			if act:
			    gc.foreground = powered
			else:
			    gc.foreground = blue
			ex, ey = PWRSIZ, PWRSIZ
			ex, ey = int(matrix[0][0] * ex + matrix[0][1] * ey), \
			int(matrix[1][0] * ex + matrix[1][1] * ey)
			points = [(xmid+ex, ymid+ey), (xmid+ey, ymid-ex), \
			(xmid-ex, ymid-ey), (xmid-ey, ymid+ex)]
			draw_polygon(pix, gc, TRUE, points)
			gc.foreground = lines
			draw_polygon(pix, gc, FALSE, points)
		    if cont.phase == 2 and (k & PWR):
			gc.foreground = lines # the power source is black
			ex, ey = PWRSIZ, PWRSIZ
			ex, ey = int(matrix[0][0] * ex + matrix[0][1] * ey), \
			int(matrix[1][0] * ex + matrix[1][1] * ey)
			points = [(xmid+ex, ymid+ey), (xmid+ey, ymid-ex), \
			(xmid-ex, ymid-ey), (xmid-ey, ymid+ex)]
			draw_polygon(pix, gc, TRUE, points)
			gc.foreground = lines
			draw_polygon(pix, gc, FALSE, points)

        for cont.phase in range(2):
            gc.foreground = [lines, barrier][cont.phase]
            for x in range(NSQUARES_X+1):
                for y in range(NSQUARES_Y):
                    x0 = (x+NSQUARES_X-1) % NSQUARES_X
                    x1 = x % NSQUARES_X
                    if self.barrier(x0,y,x1,y):
                        xmin = ORIGIN + SQUARESIZE * x
                        ymin = ORIGIN + SQUARESIZE * y
                        ymax = ymin + SQUARESIZE
                        draw_thick_line(pix, gc, xmin, ymin, xmin, ymax, 0)
            for x in range(NSQUARES_X):
                for y in range(NSQUARES_Y+1):
                    y0 = (y+NSQUARES_Y-1) % NSQUARES_Y
                    y1 = y % NSQUARES_Y
                    if self.barrier(x,y0,x,y1):
                        xmin = ORIGIN + SQUARESIZE * x
                        ymin = ORIGIN + SQUARESIZE * y
                        xmax = xmin + SQUARESIZE
                        draw_thick_line(pix, gc, xmin, ymin, xmax, ymin, 0)

    def move(self, x, y, action, undo=FALSE):
        assert(not self.moveinprogress)

        if undo:
            # In undo mode, action 0 is an actual NOP so isn't
            # treated specially; and a move touching a locked
            # square automatically unlocks it.
            self.arena[x][y] = self.arena[x][y] &~ LOCK

        else:
            if action == 0:
                # Just toggle the lock on the square.
                self.arena[x][y] = self.arena[x][y] ^ LOCK
                self.fullredraw = TRUE
                small_redraw()
                return

            # Locked squares can't be accidentally moved.
            if self.arena[x][y] & LOCK:
                return

        self.movex = x
        self.movey = y
        self.moveact = action

        self.moveinprogress = TRUE
        self.moveprogress = 0.0
        if not undo:
            # Successive moves involving the same square should be
            # merged, or deleted if they turn out to be the
            # identity move.
            if (not self.completed or len(self.moves)!=self.completion_point) \
            and len(self.moves) > 0 and self.moves[-1][0] == x and \
            self.moves[-1][1] == y:
                combine = self.moves[-1][2] + action
                combine = (combine + 1) % 4 - 1
                if combine == 0:
                    self.moves = self.moves[:-1]
                else:
                    self.moves[-1] = (x, y, combine)
            else:
                self.moves.append((x, y, action))

    def finish_move(self):

        # Finish up the move in progress.
        self.arena[self.movex][self.movey] = \
        rotate(self.arena[self.movex][self.movey], self.moveact)
        self.fullredraw = TRUE

        self.moveinprogress = FALSE

        oldcomplete = self.completed

        self.compute_active()

        if self.completed and not oldcomplete:
            self.completion_animation = 1
            timer_inst = timeout_add(50, timer, win)

    def undo(self):
        if len(self.moves) == 0:
            return # nothing to undo
        dx, dy, action = self.moves.pop()
        self.move(dx, dy, -action, undo=TRUE)
        if self.completed and len(self.moves) < self.completion_point:
            self.completed = FALSE

    def status(self, statusbar):
        message = "Active: %d/%d" % (self.activesquares, self.totalsquares)
        if self.completed:
            message = "COMPLETED! " + message
        statusbar.pop(statusctx)
        statusbar.push(statusctx, message)

def delete_event(window, event=None):
    win.destroy()
def destroy_event(win, event=None):
    mainquit()

def small_redraw():
    bbox0 = game.bbox
    game.draw(pix)
    bbox1 = game.bbox
    if game.fullredraw:
        redraw(darea)
        game.fullredraw = FALSE
    elif bbox0[0] != None or bbox1[0] != None:
        if bbox0[0] == None:
            bbox0 = bbox1
        if bbox1[0] == None:
            bbox1 = bbox0
        redraw(darea, [min(bbox0[0],bbox1[0]), min(bbox0[1],bbox1[1]),
        max(bbox0[2],bbox1[2]), max(bbox0[3],bbox1[3])])

def undo_move(menuitem=None):
    global timer_inst

    if game.moveinprogress:
        game.finish_move()
        small_redraw()

    game.undo()
    game.status(statusbar)

    if timer_inst != None:
        timeout_remove(timer_inst)
    timer_inst = timeout_add(20, timer, win)

def key_event(win, event=None):
    global timer_inst

    if event.keyval == GDK.Escape or event.string in ['\021', 'q', 'Q']:
        win.destroy()
        return

    # Hurry up an existing move if one is still being animated.
    if game.moveinprogress:
        game.finish_move()
        small_redraw()

    if event.string in ['\016', 'n', 'N']:
        new_game()
    elif event.string in ['\022', 'r', 'R']:
        restart_game()
    elif event.string in ['\032', 'u', 'U']:
        game.undo()
    else:
        return

    game.status(statusbar)
    if timer_inst != None:
        timeout_remove(timer_inst)
    timer_inst = timeout_add(20, timer, win)

def sign(x):
    if x < 0: return -1
    if x > 0: return +1
    return 0

def button_event(win, event):
    global timer_inst
    if not (event.button in [1,2,3]) or event.type != GDK.BUTTON_PRESS:
        return # we only care about left, mid or right clicks
    x = int((event.x + SQUARESIZE - ORIGIN) / SQUARESIZE) - 1
    y = int((event.y + SQUARESIZE - ORIGIN) / SQUARESIZE) - 1
    if x < 0 or x >= NSQUARES_X or y < 0 or y >= NSQUARES_Y:
        return # out of bounds

    if (event.state & 1) or event.button == 2:
	action = 0 # toggle lock
    elif event.button == 1:
        action = -1 # anticlockwise
    else:
        action = +1 # clockwise

    if game.moveinprogress:
        game.finish_move()
        small_redraw()
    game.move(x, y, action)
    game.status(statusbar)
    if timer_inst != None:
        timeout_remove(timer_inst)
    timer_inst = timeout_add(20, timer, win)

def timer(win):
    if game.completion_animation:
        cx = NSQUARES_X/2
        cy = NSQUARES_Y/2
        n = max(NSQUARES_X, NSQUARES_Y)/2+1
        if game.completion_animation <= n:
            for x in range(NSQUARES_X):
                for y in range(NSQUARES_Y):
                    if abs(x-cx) <= game.completion_animation - 1 and \
                    abs(y-cy) <= game.completion_animation - 1:
                        game.arena[x][y] = game.arena[x][y] | LOCK
            game.fullredraw = TRUE
            game.completion_animation = game.completion_animation + 1
            ret = TRUE
        elif game.completion_animation - n <= n:
            for x in range(NSQUARES_X):
                for y in range(NSQUARES_Y):
                    if abs(x-cx) <= game.completion_animation - n - 1 and \
                    abs(y-cy) <= game.completion_animation - n - 1:
                        game.arena[x][y] = game.arena[x][y] & ~LOCK
            game.fullredraw = TRUE
            game.completion_animation = game.completion_animation + 1
            ret = TRUE
        else:
            game.completion_animation = 0
            ret = FALSE
    else:
        if not game.moveinprogress:
            # move has been finished for us already
            game.status(statusbar)
            return FALSE
        game.moveprogress = game.moveprogress + 1.0/6
        if game.moveprogress >= 1.0:
            game.finish_move()
            game.status(statusbar)
            ret = FALSE
        else:
            ret = TRUE
    small_redraw()
    return ret

def new_game(menuitem=None, seed=None):
    global game
    game = NetGame(seed)
    game.draw(pix)
    game.status(statusbar)
    if WRAPPING:
        wrapstr = "wrapping"
    else:
        wrapstr = "non-wrapping"
    win.set_title("Net %dx%d %s (%d)" % \
    (NSQUARES_X, NSQUARES_Y, wrapstr, game.currgame))
    redraw(darea)

def restart_game(menuitem=None):
    new_game(seed=game.currgame)

def input_game(menuitem=None):
    global NSQUARES_X, NSQUARES_Y, WRAPPING, BARRIER_RATE
    class container:
        pass
    cont = container()
    def enddlg(win, value, cont=cont):
        if value == 1:
            try:
                cont.nsquares_x = string.atoi(cont.widthbox.get_text())
                cont.nsquares_y = string.atoi(cont.heightbox.get_text())
                cont.wrapping = cont.wrapping.get_active()
                cont.barrier_rate = string.atoi(cont.barrierbox.get_text())
                if cont.nsquares_x >= 3 and cont.nsquares_y >= 3:
                    if cont.editbox.get_text() == "":
                        cont.game = None
                    else:
                        cont.game = string.atol(cont.editbox.get_text())
                else:
                    cont.game = 0
            except ValueError:
                cont.game = 0
            if cont.game != None and not validate(cont.game):
                gdk_beep()
                return
        cont.value = value
        cont.dlg.destroy()
    def key_event(win, event, cont=cont, enddlg=enddlg):
        if event.keyval == GDK.Escape:
            enddlg(cont.dlg, 0)
            return
        elif event.keyval == GDK.Return:
            enddlg(cont.dlg, 1)
            return
    def destroy(win):
        mainquit()
    cont.dlg = dlg = GtkDialog()
    cont.value = 0
    dlg.set_title("Net")
    hbox = GtkHBox()
    label = GtkLabel("Grid width")
    hbox.pack_start(label)
    label.show()
    cont.widthbox = widthbox = GtkEntry()
    cont.widthbox.set_text(str(NSQUARES_X))
    hbox.pack_start(widthbox)
    widthbox.show()
    dlg.vbox.pack_start(hbox)
    hbox.show()
    hbox = GtkHBox()
    label = GtkLabel("Grid height")
    hbox.pack_start(label)
    label.show()
    cont.heightbox = heightbox = GtkEntry()
    cont.heightbox.set_text(str(NSQUARES_Y))
    hbox.pack_start(heightbox)
    heightbox.show()
    dlg.vbox.pack_start(hbox)
    hbox.show()
    cont.wrapping = wrapping = GtkCheckButton("Walls wrap")
    cont.wrapping.set_active(WRAPPING)
    dlg.vbox.pack_start(wrapping)
    wrapping.show()
    hbox = GtkHBox()
    label = GtkLabel("Barrier rate (percent)")
    hbox.pack_start(label)
    label.show()
    cont.barrierbox = barrierbox = GtkEntry()
    cont.barrierbox.set_text(str(BARRIER_RATE))
    hbox.pack_start(barrierbox)
    barrierbox.show()
    dlg.vbox.pack_start(hbox)
    hbox.show()
    label = GtkLabel("Enter game seed")
    dlg.vbox.pack_start(label)
    label.show()
    cont.editbox = editbox = GtkEntry()
    dlg.vbox.pack_start(editbox)
    editbox.show()
    ok = GtkButton("OK")
    dlg.action_area.pack_start(ok)
    ok.show()
    cancel = GtkButton("Cancel")
    dlg.action_area.pack_start(cancel)
    cancel.show()
    ok.connect("clicked", enddlg, 1)
    cancel.connect("clicked", enddlg, 0)
    dlg.connect("destroy", destroy)
    dlg.connect("key_press_event", key_event)
    dlg.show()
    editbox.grab_focus()
    mainloop()
    if cont.value:
        NSQUARES_X = cont.nsquares_x
        NSQUARES_Y = cont.nsquares_y
        WRAPPING = cont.wrapping
        BARRIER_RATE = cont.barrier_rate
        setup_globals()
        new_game(seed=cont.game)

def typed_game(size, wrapping):
    global NSQUARES_X, NSQUARES_Y, WRAPPING
    NSQUARES_X = NSQUARES_Y = size
    WRAPPING = wrapping
    setup_globals()
    new_game()

def n5_game(menuitem=None): typed_game(5, FALSE)
def w5_game(menuitem=None): typed_game(5, TRUE)
def n7_game(menuitem=None): typed_game(7, FALSE)
def w7_game(menuitem=None): typed_game(7, TRUE)
def n9_game(menuitem=None): typed_game(9, FALSE)
def w9_game(menuitem=None): typed_game(9, TRUE)
def n11_game(menuitem=None): typed_game(11, FALSE)
def w11_game(menuitem=None): typed_game(11, TRUE)

def expose(darea, event):
    gc = darea.get_style().white_gc
    x, y, w, h = event.area
    darea.draw_pixmap(gc, pix, x, y, x, y, w, h)

def redraw(darea, rect=None):
    if rect == None:
        rect = [0,0,TOTALSIZE_X,TOTALSIZE_Y]
    gc = darea.get_style().white_gc
    x, y, x2, y2 = rect
    w, h = x2-x+1, y2-y+1
    if gc != None:
        darea.draw_pixmap(gc, pix, x, y, x, y, w, h)

win = GtkWindow()
win.set_title("Net")
win.set_policy(FALSE, FALSE, TRUE)
vbox = GtkVBox()
statusbar = GtkStatusbar()
vbox.pack_end(statusbar)
statusbar.show()
statusctx = statusbar.get_context_id("playing")
menubar = GtkMenuBar()
vbox.pack_start(menubar)
menubar.show()
gamemenu = GtkMenu()
gameitem = GtkMenuItem("Game")
gameitem.set_submenu(gamemenu)
menubar.append(gameitem)
gameitem.show()
newitem = GtkMenuItem("New")
gamemenu.append(newitem)
newitem.show()
typeitem = GtkMenuItem("Type")
gamemenu.append(typeitem)
typeitem.show()
restitem = GtkMenuItem("Restart")
gamemenu.append(restitem)
restitem.show()
specitem = GtkMenuItem("Specific")
gamemenu.append(specitem)
specitem.show()
separator = GtkMenuItem()
gamemenu.append(separator)
separator.show()
undoitem = GtkMenuItem("Undo Move")
gamemenu.append(undoitem)
undoitem.show()
separator = GtkMenuItem()
gamemenu.append(separator)
separator.show()
quititem = GtkMenuItem("Exit")
gamemenu.append(quititem)
quititem.show()
typemenu = GtkMenu()
typeitem.set_submenu(typemenu)
n5item = GtkMenuItem("5x5")
typemenu.append(n5item)
n5item.show()
w5item = GtkMenuItem("5x5 wrapping")
typemenu.append(w5item)
w5item.show()
n7item = GtkMenuItem("7x7")
typemenu.append(n7item)
n7item.show()
w7item = GtkMenuItem("7x7 wrapping")
typemenu.append(w7item)
w7item.show()
n9item = GtkMenuItem("9x9")
typemenu.append(n9item)
n9item.show()
w9item = GtkMenuItem("9x9 wrapping")
typemenu.append(w9item)
w9item.show()
n11item = GtkMenuItem("11x11")
typemenu.append(n11item)
n11item.show()
w11item = GtkMenuItem("11x11 wrapping")
typemenu.append(w11item)
w11item.show()

darea = GtkDrawingArea()
darea.set_usize(TOTALSIZE_X, TOTALSIZE_Y)
pix = create_pixmap(win, TOTALSIZE_X, TOTALSIZE_Y)
grey = win.get_style().bg[STATE_NORMAL]
lines = win.get_window().colormap.alloc(0, 0, 0)
powered = win.get_window().colormap.alloc(0, 65535, 65535)
borders = win.get_window().colormap.alloc(grey.red/2,grey.green/2,grey.blue/2)
lockcolour = win.get_window().colormap.alloc(grey.red*3/4,grey.green*3/4,grey.blue*3/4)
blue = win.get_window().colormap.alloc(0, 0, 65535)
barrier = win.get_window().colormap.alloc(65535, 0, 0)
vbox.add(darea)
darea.show()
win.add(vbox)
vbox.show()

timer_inst = None

newitem.connect("activate", new_game)
restitem.connect("activate", restart_game)
specitem.connect("activate", input_game)
undoitem.connect("activate", undo_move)
quititem.connect("activate", delete_event)
n5item.connect("activate", n5_game)
w5item.connect("activate", w5_game)
n7item.connect("activate", n7_game)
w7item.connect("activate", w7_game)
n9item.connect("activate", n9_game)
w9item.connect("activate", w9_game)
n11item.connect("activate", n11_game)
w11item.connect("activate", w11_game)
darea.connect("expose_event", expose)
darea.add_events(GDK.BUTTON_PRESS_MASK)
darea.connect("button_press_event", button_event)
win.connect("key_press_event", key_event)
win.connect("delete_event", delete_event)
win.connect("destroy", destroy_event)

new_game()

win.show()

mainloop()
