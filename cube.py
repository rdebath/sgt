#!/usr/bin/env python

from gtk import *
import GDK
import random
import math

SQUARESIZE = 80
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

class CubeGame:
    def __init__(self):
        self.cube = [0, 0, 0, 0, 0, 0]
        self.arena = []
        for i in range(NSQUARES):
            self.arena.append([0] * NSQUARES)
        list = range(NSQUARES * NSQUARES)
        # Pick six squares to be blue.
        for i in range(6):
            pos = random.choice(list)
            x, y = divmod(pos, NSQUARES)
            list.remove(pos)
            self.arena[x][y] = 1
        # And a final non-blue square to be the cube's starting point.
        pos = random.choice(list)
        self.cubex, self.cubey = divmod(pos, NSQUARES)
        self.moveinprogress = FALSE
        self.moves = 0
        self.completed = FALSE

    def draw(self, pixmap):
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
                points.append((ORIGIN+(cubex+x)*SQUARESIZE,
                ORIGIN+(cubey+y)*SQUARESIZE))
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

    def move(self, dx, dy):
        # Hurry up an existing move if one is still being animated.
        if self.moveinprogress:
            self.finish_move()

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
        if not self.completed:
            self.moves = self.moves + 1

    def finish_move(self):
        # Swap bottom face. (Once the game is completed, we allow
        # the user to arbitrarily roll the fully blue cube around
        # the grid just for fun.)
        if not self.completed:
            self.arena[self.newx][self.newy], self.newcube[B] = \
            self.newcube[B], self.arena[self.newx][self.newy]

        self.cubex, self.cubey = self.newx, self.newy
        self.cube = self.newcube

        self.moveinprogress = FALSE

        complete = TRUE
        for i in self.cube:
            if not i:
                complete = FALSE
        self.completed = complete

    def status(self, statusbar):
        message = "Moves: %d" % self.moves
        if self.completed:
            message = "COMPLETED! " + message
        statusbar.pop(statusctx)
        statusbar.push(statusctx, message)

def delete_event(window, event=None):
    win.destroy()
def destroy_event(win, event=None):
    mainquit()

def key_event(win, event=None):
    global timer_inst

    if event.keyval == GDK.Escape or event.string == '\021':
        win.destroy()
        return

    if event.string == '\016':
        new_game(None)
        return

    if event.keyval == GDK.Up or event.keyval == GDK.KP_Up:
        game.move(0, -1)
    elif event.keyval == GDK.Down or event.keyval == GDK.KP_Down:
        game.move(0, +1)
    elif event.keyval == GDK.Left or event.keyval == GDK.KP_Left:
        game.move(-1, 0)
    elif event.keyval == GDK.Right or event.keyval == GDK.KP_Right:
        game.move(+1, 0)
    else:
        return
    timeout_add(20, timer, win)

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
    game.draw(pix)
    redraw(darea)
    return ret

def new_game(menuitem):
    global game
    game = CubeGame()
    game.draw(pix)
    game.status(statusbar)
    redraw(darea)

def expose(darea, event=None):
    gc = darea.get_style().white_gc
    x, y, w, h = event.area
    darea.draw_pixmap(gc, pix, x, y, x, y, w, h)

def redraw(darea):
    gc = darea.get_style().white_gc
    darea.draw_pixmap(gc, pix, 0, 0, 0, 0, TOTALSIZE, TOTALSIZE)

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

newitem.connect("activate", new_game)
quititem.connect("activate", delete_event)
darea.connect("expose_event", expose)
win.connect("key_press_event", key_event)
win.connect("delete_event", delete_event)
win.connect("destroy", destroy_event)

game = CubeGame()
game.draw(pix)
game.status(statusbar)

win.show()

mainloop()
