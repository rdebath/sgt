#!/usr/bin/env python

# Clone of the FreeNet game at http://www.jurjans.lv/stuff/net/FreeNet.htm

# TODO:
#
#  - Game parameters. Wrapping borders, controllable grid size.
#    Probably also controllable on-screen size while we're at it.
#  - The original occasionally has barriers between squares within
#    the grid. Makes the game easier, of course.
#  - Alternative forms of control? Left/right clicks are OKish, but
#    I'm not 100% convinced. Middle click to lock is Just Wrong.
#  - Some sort of persistent completion indicator, perhaps, in case
#    you click straight past the finished state and don't realise
#    you actually finished!
#  - Indication of how many lights are lit up.

from gtk import *
import GDK
import random
import md5
import math
import string

SQUARESIZE = 31
NSQUARES_X = 11
NSQUARES_Y = 11

PWRSIZ = SQUARESIZE / 4

BORDER = SQUARESIZE * 3 / 2

ORIGIN = BORDER
TOTALSIZE_X = 2*BORDER + NSQUARES_X * SQUARESIZE
TOTALSIZE_Y = 2*BORDER + NSQUARES_Y * SQUARESIZE

U, L, D, R, PWR, ACTIVE, LOCK = 1, 2, 4, 8, 16, 32, 64

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
            game = random.randint(0, 0xFFFF) * 16L
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
        while 1:
            possibilities = []
            for x in range(NSQUARES_X):
                for y in range(NSQUARES_Y-1):
                    if self.arena[x][y] == 0 and self.arena[x][y+1] != 0 \
                    and (self.arena[x][y+1] & (L|R|D)) != (L|R|D):
                        possibilities.append((x, y+1, U, x, y, D))
                    if self.arena[x][y] != 0 and self.arena[x][y+1] == 0 \
                    and (self.arena[x][y] & (L|R|U)) != (L|R|U):
                        possibilities.append((x, y, D, x, y+1, U))
            for x in range(NSQUARES_X-1):
                for y in range(NSQUARES_Y):
                    if self.arena[x][y] == 0 and self.arena[x+1][y] != 0 \
                    and (self.arena[x+1][y] & (U|R|D)) != (U|R|D):
                        possibilities.append((x+1, y, L, x, y, R))
                    if self.arena[x][y] != 0 and self.arena[x+1][y] == 0 \
                    and (self.arena[x][y] & (U|L|D)) != (U|L|D):
                        possibilities.append((x, y, R, x+1, y, L))
            if len(possibilities) == 0:
                break
            choice = rng.getint(len(possibilities))
            sx, sy, sd, dx, dy, dd = possibilities[choice]
            self.arena[sx][sy] = self.arena[sx][sy] | sd
            self.arena[dx][dy] = self.arena[dx][dy] | dd

        # Now we've got the network, scramble it.
        for x in range(NSQUARES_X):
            for y in range(NSQUARES_Y):
                self.arena[x][y] = rotate(self.arena[x][y], rng.getint(4))

        # Compute the ACTIVE bits.
        self.compute_active()

        self.moves = []
        self.moveinprogress = FALSE
        self.fullredraw = TRUE

    def compute_active(self):
        list = []
        def visit(x, y, self=self, list=list):
            self.arena[x][y] = self.arena[x][y] | ACTIVE
            if x > 0 and (self.arena[x][y] & L) and \
            (self.arena[x-1][y] & R) and not (self.arena[x-1][y] & ACTIVE):
                list.append((x-1, y))
            if y > 0 and (self.arena[x][y] & U) and \
            (self.arena[x][y-1] & D) and not (self.arena[x][y-1] & ACTIVE):
                list.append((x, y-1))
            if x < NSQUARES_X-1 and (self.arena[x][y] & R) and \
            (self.arena[x+1][y] & L) and not (self.arena[x+1][y] & ACTIVE):
                list.append((x+1, y))
            if y < NSQUARES_Y-1 and (self.arena[x][y] & D) and \
            (self.arena[x][y+1] & U) and not (self.arena[x][y+1] & ACTIVE):
                list.append((x, y+1))

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

        self.completed = TRUE
        for x in range(NSQUARES_X):
            for y in range(NSQUARES_Y):
                if not (self.arena[x][y] & ACTIVE):
                    self.completed = FALSE

    def draw(self, pixmap):
	class container:
	    pass
	cont = container()
        def draw_thick_line(pix, gc, x1, y1, x2, y2, active, cont=cont):
	    if cont.phase == 0:
		draw_line(pix, gc, x1-1, y1, x2-1, y2)
		draw_line(pix, gc, x1+1, y1, x2+1, y2)
		draw_line(pix, gc, x1, y1-1, x2, y2-1)
		draw_line(pix, gc, x1, y1+1, x2, y2+1)
	    elif cont.phase == 1:
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
        if not self.completed and not undo:
            # Successive moves involving the same square should be
            # merged. This occasionally causes us to end up with an
            # identity move on the list, which is actually what we
            # want because it's precisely those wasted moves which
            # prevent us having exactly the same move count for any
            # completion of a given game!
            if len(self.moves) > 0 and self.moves[-1][0] == x and \
            self.moves[-1][1] == y:
                combine = self.moves[-1][2] + action
                combine = (combine + 1) % 4 - 1
                self.moves[-1] = (x, y, combine)
            else:
                self.moves.append((x, y, action))

    def finish_move(self):

        # Finish up the move in progress.
        self.arena[self.movex][self.movey] = \
        rotate(self.arena[self.movex][self.movey], self.moveact)
        self.fullredraw = TRUE

        self.moveinprogress = FALSE

        self.compute_active()

    def undo(self):
        if len(self.moves) == 0:
            return # nothing to undo
        dx, dy, action = self.moves.pop()
        self.move(dx, dy, -action, undo=TRUE)
        self.compute_active()

    def status(self, statusbar):
        message = "Moves: %d" % len(self.moves)
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
    win.set_title("Net (%d)" % game.currgame)
    redraw(darea)

def restart_game(menuitem=None):
    new_game(seed=game.currgame)

def input_game(menuitem=None):
    class container:
        pass
    cont = container()
    def enddlg(win, value, cont=cont):
        if value == 1:
            try:
                cont.game = string.atol(cont.editbox.get_text())
            except ValueError:
                cont.game = 0
            if not validate(cont.game):
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
        new_game(seed=cont.game)

def expose(darea, event):
    gc = darea.get_style().white_gc
    x, y, w, h = event.area
    darea.draw_pixmap(gc, pix, x, y, x, y, w, h)

def redraw(darea, rect=[0,0,TOTALSIZE_X,TOTALSIZE_Y]):
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
darea = GtkDrawingArea()
darea.set_usize(TOTALSIZE_X, TOTALSIZE_Y)
pix = create_pixmap(win, TOTALSIZE_X, TOTALSIZE_Y)
grey = win.get_style().bg[STATE_NORMAL]
lines = win.get_window().colormap.alloc(0, 0, 0)
powered = win.get_window().colormap.alloc(0, 65535, 65535)
borders = win.get_window().colormap.alloc(grey.red/2,grey.green/2,grey.blue/2)
lockcolour = win.get_window().colormap.alloc(grey.red*3/4,grey.green*3/4,grey.blue*3/4)
blue = win.get_window().colormap.alloc(0, 0, 65535)
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
darea.connect("expose_event", expose)
darea.add_events(GDK.BUTTON_PRESS_MASK)
darea.connect("button_press_event", button_event)
win.connect("key_press_event", key_event)
win.connect("delete_event", delete_event)
win.connect("destroy", destroy_event)

new_game()

win.show()

mainloop()
