#!/usr/bin/env python

from gtk import *
import GDK
import random
import math
import string

SQUARESIZE = 50
NSQUARES = 4
ISOMETRIC = 0.3  # scale z by this before subtracting from x and y

BORDER = SQUARESIZE * ISOMETRIC * 1.5

ORIGIN = BORDER
TOTALSIZE = 2*BORDER + NSQUARES * SQUARESIZE

# Indices into a cube tuple. First two are the top face, i.e.
# facing the player, and the bottom face, i.e. facing the board.
# The remaining four are the side faces in the L, R, U, D
# directions respectively.
T, B, L, R, U, D = range(6)

# Coordinate sets for a clockwise path around each face.
facecoords = [
[(0,0,1),(1,0,1),(1,1,1),(0,1,1)],
[(0,0,0),(0,1,0),(1,1,0),(1,0,0)],
[(0,0,1),(0,1,1),(0,1,0),(0,0,0)],
[(1,0,1),(1,0,0),(1,1,0),(1,1,1)],
[(0,0,1),(0,0,0),(1,0,0),(1,0,1)],
[(0,1,1),(1,1,1),(1,1,0),(0,1,0)]]

def validate(game):
    pos = game >> (NSQUARES * NSQUARES)
    if pos >= (NSQUARES * NSQUARES): return 0 # must be in range
    if game & (1L << pos): return 0 # must start on non-blue square
    n = 0
    for i in range(NSQUARES * NSQUARES):
        if game & (1L << i):
            n = n + 1
    if n != 6: return 0 # must have exactly six blue squares
    return 1

class CubeGame:
    def __init__(self, game):
        if game == None:
            game = 0L
            list = range(NSQUARES * NSQUARES)
            # Pick six squares to be blue.
            for i in range(6):
                pos = random.choice(list)
                game = game | (1L << pos)
                list.remove(pos)
            # And a final non-blue square to be the cube's starting point.
            pos = random.choice(list)
            game = game | (long(pos) << (NSQUARES*NSQUARES))
        self.currgame = game

        self.cube = [0, 0, 0, 0, 0, 0]
        self.arena = []
        for i in range(NSQUARES):
            self.arena.append([0] * NSQUARES)
        for pos in range(NSQUARES*NSQUARES):
            if game & (1L << pos):
                x, y = divmod(pos, NSQUARES)
                self.arena[x][y] = 1
        self.cubex, self.cubey = \
        divmod(int(game >> (NSQUARES*NSQUARES)), NSQUARES)

        self.moveinprogress = FALSE
        self.moves = []
        self.completed = FALSE
        self.fullredraw = FALSE

    def draw(self, pixmap):

        self.bbox = [None, None, None, None]

        gc = pix.new_gc()
        gc.foreground = grey
        draw_rectangle(pix, gc, TRUE, 0, 0, TOTALSIZE, TOTALSIZE)

        for x in range(NSQUARES):
            for y in range(NSQUARES):
                if self.arena[x][y]:
                    gc.foreground = blue
                else:
                    gc.foreground = grey
                draw_rectangle(pix, gc, TRUE, ORIGIN+SQUARESIZE*x,
                ORIGIN+SQUARESIZE*y, SQUARESIZE, SQUARESIZE)

        gc.foreground = lines
        for i in range(NSQUARES+1):
            draw_line(pix, gc, ORIGIN, ORIGIN+SQUARESIZE*i,
            ORIGIN+NSQUARES*SQUARESIZE, ORIGIN+SQUARESIZE*i)
            draw_line(pix, gc, ORIGIN+SQUARESIZE*i, ORIGIN,
            ORIGIN+SQUARESIZE*i, ORIGIN+NSQUARES*SQUARESIZE)

        # Now draw the cube. This is complicated by the fact that a
        # move may be in progress, so the cube might be rolling on
        # one edge...
        dx = (1, 0)
        dy = (0, 1)
        dz = (-ISOMETRIC, -ISOMETRIC)

        # If a move is in progress, adjust dx, dy and dz for the
        # rolling cube.
        if self.moveinprogress:
            # For simplicity, we'll always draw the cube as if it
            # were based in the bottom-most or rightmost of the
            # before and after positions (this allows us to keep
            # the origin at 0,0).
            cubex = max(self.cubex, self.newx)
            cubey = max(self.cubey, self.newy)
            if self.newx < self.cubex or self.newy < self.cubey:
                progress = +self.moveprogress
                cube = self.cube
            else:
                progress = 1.0-self.moveprogress
                cube = self.newcube
            # Now work out the coordinates of the rotation matrix.
            theta = math.pi/2 * progress
            rx = math.cos(theta)
            ry = math.sin(theta)
            # So (rx,ry) is (1,0) at the start of the move, (0,1) at the end.
            if self.newx != self.cubex:
                # Horizontal rotation.
                newdx = (dx[0]*rx + dz[0]*ry, dx[1]*rx + dz[1]*ry)
                newdz = (dx[0]*-ry + dz[0]*rx, dx[1]*-ry + dz[1]*rx)
                dx, dz = newdx, newdz
            else:
                # Vertical rotation.
                newdy = (dy[0]*rx + dz[0]*ry, dy[1]*rx + dz[1]*ry)
                newdz = (dy[0]*-ry + dz[0]*rx, dy[1]*-ry + dz[1]*rx)
                dy, dz = newdy, newdz
        else:
            cubex, cubey = self.cubex, self.cubey
            cube = self.cube

        facelist = []
        for face in range(6):
            # Work out the 2-D points we'll use to draw this face.
            points = []
            for pt in facecoords[face]:
                x = dx[0] * pt[0] + dy[0] * pt[1] + dz[0] * pt[2]
                y = dx[1] * pt[0] + dy[1] * pt[1] + dz[1] * pt[2]
                points.append((int(ORIGIN+(cubex+x)*SQUARESIZE),
                int(ORIGIN+(cubey+y)*SQUARESIZE)))
            # Find out whether these points are in a clockwise or
            # anticlockwise arrangement. If the latter, discard the
            # face because it's facing away from the viewer.
            #
            # This would involve fiddly winding-number stuff for a
            # general polygon, but for the simple parallelograms
            # we'll be seeing here, all we have to do is check
            # whether the corners turn right or left. So we'll take
            # the vector from points[0] to points[1], turn it right
            # 90 degrees, and check the sign of the dot product
            # with that and the next vector (points[1] to
            # points[2]).
            v1 = (points[1][0]-points[0][0], points[1][1]-points[0][1])
            v1r = (-v1[1], v1[0])
            v2 = (points[2][0]-points[1][0], points[2][1]-points[1][1])
            dp = v1r[0] * v2[0] + v1r[1] * v2[1]

            if dp > 0:
                # We are going to display this face.
                facelist.append(cube[face], points)
                # Update the bounding box for drawing the cube.
                for p in points:
                    if self.bbox[0] == None or self.bbox[0] > p[0]:
                        self.bbox[0] = p[0]
                    if self.bbox[1] == None or self.bbox[1] > p[1]:
                        self.bbox[1] = p[1]
                    if self.bbox[2] == None or self.bbox[2] < p[0]:
                        self.bbox[2] = p[0]
                    if self.bbox[3] == None or self.bbox[3] < p[1]:
                        self.bbox[3] = p[1]

        # Fill the faces.
        for colour, points in facelist:
            if colour:
                gc.foreground = blue
            else:
                gc.foreground = grey
            draw_polygon(pix, gc, TRUE, points)
        # Now outline the faces.
        gc.foreground = lines
        for colour, points in facelist:
            draw_polygon(pix, gc, FALSE, points)

        assert self.bbox[0] != None
        assert self.bbox[1] != None
        assert self.bbox[2] != None
        assert self.bbox[3] != None

    def move(self, dx, dy, undo=FALSE):
        assert(not self.moveinprogress)

        # If this is an undo move, we do the swap _first_.
        self.moveisundo = undo
        if undo:
            self.arena[self.cubex][self.cubey], self.newcube[B] = \
            self.newcube[B], self.arena[self.cubex][self.cubey]

        # Make a move on the grid.
        if self.cubex + dx < 0 or self.cubex + dx >= NSQUARES:
            return # nothing to do
        if self.cubey + dy < 0 or self.cubey + dy >= NSQUARES:
            return # nothing to do
        oldx, oldy = self.cubex, self.cubey
        newx, newy = oldx + dx, oldy + dy

        assert dx == 0 or dy == 0 # no diagonal moves
        assert dx != 0 or dy != 0 # no zero-effect moves either

        if dx < 0:
            mapping = [R, L, T, B, U, D]
        elif dx > 0:
            mapping = [L, R, B, T, U, D]
        elif dy < 0:
            mapping = [D, U, L, R, T, B]
        else:
            assert dy > 0
            mapping = [U, D, L, R, B, T]

        newcube = []
        for i in mapping:
            newcube.append(self.cube[i])

        self.newcube = newcube
        self.newx, self.newy = newx, newy
        self.moveinprogress = TRUE
        self.moveprogress = 0.0
        if not self.completed and not undo:
            self.moves.append((dx,dy))

    def finish_move(self):
        # Swap bottom face. (Once the game is completed, we allow
        # the user to arbitrarily roll the fully blue cube around
        # the grid just for fun.)
        if not self.completed and not self.moveisundo:
            self.arena[self.newx][self.newy], self.newcube[B] = \
            self.newcube[B], self.arena[self.newx][self.newy]

        self.cubex, self.cubey = self.newx, self.newy
        self.cube = self.newcube

        self.moveinprogress = FALSE

        if not self.completed:
            complete = TRUE
            for i in self.cube:
                if not i:
                    complete = FALSE
            if complete:
                self.completed = TRUE
                self.completex, self.completey = self.cubex, self.cubey

    def undo(self):
        if len(self.moves) == 0:
            return # nothing to undo
        if self.completed:
            self.cubex, self.cubey = self.completex, self.completey
            self.completed = FALSE
            self.fullredraw = TRUE
        dx, dy = self.moves.pop()
        self.move(-dx, -dy, undo=TRUE)

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
    else:
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

    if event.string == '\016':
        new_game()
        return

    # Hurry up an existing move if one is still being animated.
    if game.moveinprogress:
        game.finish_move()
        small_redraw()

    if event.string in ['\032', 'u', 'U']:
        game.undo()
    elif event.keyval == GDK.Up or event.keyval == GDK.KP_Up:
        game.move(0, -1)
    elif event.keyval == GDK.Down or event.keyval == GDK.KP_Down:
        game.move(0, +1)
    elif event.keyval == GDK.Left or event.keyval == GDK.KP_Left:
        game.move(-1, 0)
    elif event.keyval == GDK.Right or event.keyval == GDK.KP_Right:
        game.move(+1, 0)
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
    if event.button != 1 or event.type != GDK.BUTTON_PRESS:
        return # we only care about left clicks
    x = int((event.x + SQUARESIZE - ORIGIN) / SQUARESIZE) - 1
    y = int((event.y + SQUARESIZE - ORIGIN) / SQUARESIZE) - 1
    if x < 0 or x >= NSQUARES or y < 0 or y >= NSQUARES:
        return # out of bounds
    dx = x - game.cubex
    dy = y - game.cubey
    if (dx == 0 or dy == 0) and (dx != 0 or dy != 0):
        if game.moveinprogress:
            game.finish_move()
            small_redraw()
        game.move(sign(dx), sign(dy))
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
    game = CubeGame(seed)
    game.draw(pix)
    game.status(statusbar)
    win.set_title("Cube (%d)" % game.currgame)
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
    dlg.set_title("Cube")
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

def redraw(darea, rect=[0,0,TOTALSIZE,TOTALSIZE]):
    gc = darea.get_style().white_gc
    x, y, x2, y2 = rect
    w, h = x2-x+1, y2-y+1
    if gc != None:
        darea.draw_pixmap(gc, pix, x, y, x, y, w, h)

win = GtkWindow()
win.set_title("Cube")
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
darea.set_usize(TOTALSIZE, TOTALSIZE)
pix = create_pixmap(win, TOTALSIZE, TOTALSIZE)
grey = win.get_style().bg[STATE_NORMAL]
lines = win.get_window().colormap.alloc(0, 0, 0)
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
