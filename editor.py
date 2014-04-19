#!/usr/bin/env python

# This is a really simple GUI bitmap font editor program handling both
# .fd and .bdf files, which I use to add characters to the bitmap
# fonts in this source repository.
#
# I previously used gbdfed, but have recently found that it no longer
# compiles easily on modern Linux systems; additionally, it always had
# trouble with at least one detail of the formatting in my BDFs, and
# doesn't support the .fd format I use for Windows font construction.
# Bitmap font editors aren't hard to whip up in Python/Tk, so here's
# one that serves my needs.
#
# I didn't put much effort into a discoverable or conventional UI
# either, so here are some brief instructions, mostly as a reminder
# for myself:
#  - run './editor.py fontfile', where the font file has a .fd or .bdf
#    extension (we don't do proper format detection from content).
#  - use PgUp/PgDn to page around the font encoding panel.
#  - click on a character to open an editor panel showing it.
#  - set and unset pixels in that panel using the left mouse button.
#    (Clicking a pixel toggles it; clicking and dragging sets more
#    pixels to the same state to which the initial click toggled the
#    first pixel.)
#  - press A to abandon the editor panel, or S to close it and save
#    the character back to the encoding panel.
#  - you can have multiple editor panels active at once; mouse clicks
#    change which is highlighted, and hence which one takes an A or S
#    keystroke. PgUp/PgDn keystrokes still go to the encoding panel
#    throughout.
#  - press ^S to save the font file back to disk.
#  - press Q to quit the program.

# TODO:
#
#  - Support editing chars which aren't indexed by a numeric encoding,
#    because they have 'ENCODING -1' in the input bdf. Currently we
#    make an effort to preserve those characters in the load/save
#    cycle, but they're not reachable by the editor.
#
#  - Support chars which extend outside the font bounding box. BDF
#    permits this (you just specify an appropriate BBX, e.g. with
#    negative left x coordinate), but .fd does not. Since I'm mostly
#    interested in nicely behaved fixed-pitch fonts for terminal
#    emulators, it's not a feature that immediately bothers me, but it
#    would be necessary to do something about it if anyone wanted to
#    use this editor more widely.

import sys
import string

from Tkinter import *

import subprocess

gutter = 20
pixel = 32

class CharEditor(object):
    def __init__(self, root, selector, ch, width, bitmap):
        self.root = root
        self.selector = selector
        self.ch = ch
        self.width = width
        self.ysize = self.selector.ysize

        self.canvas = Canvas(self.root.tkroot,
                             width=width*pixel + 2*gutter,
                             height=self.ysize*pixel + 2*gutter,
                             bg='#d0d0d0')

        self.setup()

        self.canvas.bind("<Button-1>", self.click)
        self.canvas.bind("<B1-Motion>", self.drag)

        self.canvas.grid(row=0,rowspan=3,column=self.root.getcol())

        self.set_bitmap(bitmap)

        self.dragstate = None

        self.root.register(self, "CharEditor")

    def setup(self):
        for i in self.canvas.find_all():
            self.canvas.delete(i)

        self.canvas.configure(width=self.width*pixel + 2*gutter)

        self.bitmap = [0] * self.ysize
        self.pixels = [[None]*self.width for y in range(self.ysize)]

        for x in range(self.width+1):
            self.canvas.create_line(gutter + x*pixel, gutter,
                                    gutter + x*pixel, gutter + self.ysize*pixel)
        for y in range(self.ysize+1):
            self.canvas.create_line(gutter, gutter + y*pixel,
                                    gutter + self.width*pixel, gutter + y*pixel)

        self.canvas.create_line(gutter,
                                gutter + font.baseline*pixel,
                                gutter + self.width*pixel,
                                gutter + font.baseline*pixel,
                                width=3) # baseline

    def set_bitmap(self, bitmap):
        for y in range(self.ysize):
            for x in range(self.width):
                self.setpixel(x, y, 1 & (bitmap[y] >> (self.width-1-x)))

    def getpixel(self, x, y):
        assert x >= 0 and x < self.width and y >= 0 and y < self.ysize
        bit = 1 << (self.width-1 - x)
        return self.bitmap[y] & bit

    def setpixel(self, x, y, state):
        assert x >= 0 and x < self.width and y >= 0 and y < self.ysize
        bit = 1 << (self.width-1 - x)
        if state and not (self.bitmap[y] & bit):
            self.bitmap[y] |= bit
            self.pixels[y][x] = self.canvas.create_rectangle(
                gutter + x*pixel, gutter + y*pixel,
                gutter + (x+1)*pixel, gutter + (y+1)*pixel,
                fill='black')
        elif not state and (self.bitmap[y] & bit):
            self.bitmap[y] &= ~bit
            self.canvas.delete(self.pixels[y][x])
            self.pixels[y][x] = None

    def gained_focus(self):
        self.canvas['bg'] = "white"

    def lost_focus(self):
        self.canvas['bg'] = "#d0d0d0"

    def click(self, event):
        self.root.focus(self, "CharEditor")
        self.dragstate = None
        x = (event.x - gutter) / pixel
        y = (event.y - gutter) / pixel
        if x >= 0 and x < self.width and y >= 0 and y < self.ysize:
            self.dragstate = not self.getpixel(x,y)
            self.setpixel(x, y, self.dragstate)

    def drag(self, event):
        if self.dragstate is None:
            return
        x = (event.x - gutter) / pixel
        y = (event.y - gutter) / pixel
        if x >= 0 and x < self.width and y >= 0 and y < self.ysize:
            self.setpixel(x, y, self.dragstate)
            return

    def close(self):
        self.canvas.grid_forget()
        self.root.unregister(self, "CharEditor")
        self.selector.done_editing(self.ch)

    def key(self, event):
        if event.char in ('c','C'):
            for y in range(self.ysize):
                for x in range(self.width):
                    self.setpixel(x, y, 0)
        elif event.char in ('s','S'):
            self.selector.font.putchar(self.ch, self.width, self.bitmap)
            self.selector.redraw()
            self.close()
        elif event.char in ('>',):
            tmpbitmap = [x << 1 for x in self.bitmap]
            self.width += 1
            self.setup()
            self.set_bitmap(tmpbitmap)
        elif event.char in ('<',) and self.width > 0:
            tmpbitmap = [x >> 1 for x in self.bitmap]
            self.width -= 1
            self.setup()
            self.set_bitmap(tmpbitmap)
        elif event.char in ('a','A'):
            self.close()

class CharSelector(object):
    def __init__(self, root, font):
        self.root = root
        self.font = font

        self.xsize = font.maxwidth()
        self.ysize = font.height

        self.cpl = 16 # chars per line
        self.page = 256 # chars per page
        self.rows = self.page / self.cpl
        self.maxpage = font.maxchar() / self.page
        self.gutter = 8
        selwidth = self.gutter + self.cpl*(self.xsize+self.gutter)
        selheight = self.gutter + self.rows*(self.ysize+self.gutter)
        self.selector = Canvas(self.root.tkroot,
                               width=selwidth,
                               height=selheight,
                               bg="white")
        for x in range(self.cpl+1):
            self.selector.create_line(self.gutter/2 + x*(self.xsize+self.gutter),
                                      self.gutter/2,
                                      self.gutter/2 + x*(self.xsize+self.gutter),
                                      self.gutter/2 + self.rows*(self.ysize+self.gutter))
        for y in range(self.rows+1):
            self.selector.create_line(self.gutter/2,
                                      self.gutter/2 + y*(self.ysize+self.gutter),
                                      self.gutter/2 + self.cpl*(self.xsize+self.gutter),
                                      self.gutter/2 + y*(self.ysize+self.gutter))
        self.rects = []

        self.selector.bind("<Motion>", self.hover)
        self.selector.bind("<Button-1>", self.click)

        self.pagelabel = Label(self.root.tkroot)
        self.charlabel = Label(self.root.tkroot)

        col = self.root.getcol()
        self.pagelabel.grid(row=0,column=col)
        self.selector.grid(row=1,column=col)
        self.charlabel.grid(row=2,column=col)

        self.editors = {}

        self.goto_page(0)

        self.root.register(self, "CharSelector")

    def gained_focus(self):
        pass

    def lost_focus(self):
        pass

    def xy_to_char(self, x, y):
        x = (x - self.gutter/2) / (self.gutter + self.xsize)
        y = (y - self.gutter/2) / (self.gutter + self.ysize)
        if x >= 0 and x < self.cpl and y >= 0 and y < self.rows:
            return self.start + y*self.cpl + x
        else:
            return None

    def hover(self, event):
        ch = self.xy_to_char(event.x, event.y)
        if ch is None:
            label = ""
        else:
            label = "%x" % ch
        self.charlabel['text'] = label

    def click(self, event):
        ch = self.xy_to_char( event.x, event.y)
        if ch is None or ch in self.editors:
            return
        width, bitmap = self.font.getchar(ch)
        if width is None:
            width = self.xsize
            bitmap = [0] * self.ysize
        self.editors[ch] = CharEditor(self.root, self, ch, width, bitmap)
        self.goto_page(self.currpage)

    def done_editing(self, ch):
        del self.editors[ch]
        self.redraw()

    def key(self, event):
        if event.char == '\x13':
            print "saving"
            self.font.write()
        elif event.keysym in ('Next', 'KP_Next'):
            self.goto_page(self.currpage + 1)
        elif event.keysym in ('Prior', 'KP_Prior'):
            self.goto_page(self.currpage - 1)
        else:
            return False
        return True

    def redraw(self):
        self.goto_page(self.currpage)

    def goto_page(self, page):
        if page < 0 or page > self.maxpage:
            return
        self.currpage = page

        for r in self.rects:
            self.selector.delete(r)
        self.rects = []

        self.start = self.currpage * self.page
        self.pagelabel['text'] = "%x - %x" % (
            self.start, self.start + self.page - 1)

        for cpos in range(self.page):
            x, y = cpos % self.rows, cpos / self.rows
            x = self.gutter + x * (self.gutter + self.xsize)
            y = self.gutter + y * (self.gutter + self.ysize)
            ch = cpos + self.start
            if ch in self.editors:
                r = self.selector.create_rectangle(x-1, y-1, x+self.xsize+1, y+self.ysize+1,
                                                   outline="", fill="#ff0000")
                self.rects.append(r)
            width, bitmap = self.font.getchar(ch)
            if width is None:
                r = self.selector.create_rectangle(x, y, x+self.xsize, y+self.ysize,
                                                   outline="", fill="#808080")
                self.rects.append(r)
            else:
                for iy in range(self.ysize):
                    for ix in range(width):
                        bit = 1 << (width-1 - ix)
                        if bitmap[iy] & bit:
                            r = self.selector.create_rectangle(x+ix, y+iy,
                                                               x+ix+1, y+iy+1,
                                                               outline="",
                                                               fill="black")
                            self.rects.append(r)

class Root(object):
    def __init__(self, font):
        self.tkroot = Tk()

        self.font = font

        self.tkroot.bind("<Key>", self.key)

        self.maxcol = 0
        self.usedcols = set()

        self.thinglists = {}
        self.focused = {}

        self.selector = CharSelector(self, font)

    def getcol(self):
        ret = self.maxcol
        self.maxcol += 1
        self.usedcols.add(ret)
        return ret

    def releasecol(self):
        self.usedcols.remove(ret)
        while self.maxcol > 0 and self.maxcol-1 not in self.usedcols:
            self.maxcol -= 1

    def register(self, thing, kind):
        self.thinglists.setdefault(kind, [])
        assert thing not in self.thinglists[kind]
        self.thinglists[kind].append(thing)
        if self.focused.get(kind) is None:
            thing.gained_focus()
            self.focused[kind] = thing

    def unregister(self, thing, kind):
        assert thing in self.thinglists[kind]
        self.thinglists[kind].remove(thing)
        if thing == self.focused[kind]:
            self.focused[kind] = None

    def focus(self, thing, kind):
        assert thing in self.thinglists[kind]
        if thing != self.focused[kind]:
            if self.focused[kind] is not None:
                self.focused[kind].lost_focus()
            thing.gained_focus()
            self.focused[kind] = thing

    def key(self, event):
        for kind, thing in sorted(self.focused.items()):
            if thing is not None and thing.key(event):
                return
        if event.char in ('q','Q','\x11'):
            sys.exit(0)

class Font(object):
    def maxwidth(self):
        width = 0
        for ch in self.charencodings():
            if ch >= 0:
                width = max(width, self.getchar(ch)[0])
        return width

class FD(Font):
    "Class to read and write my .fd file format."
    def __init__(self, filename):
        self.height = self.baseline = None
        self.chars = {}
        curr = -1 # special key indicating file header
        self.chars[-1] = ""
        with open(filename) as f:
            for line in f.readlines():
                words = [w.lower() for w in line.split()]
                if len(words) == 0:
                    pass # rule out this case for all the elifs
                elif words[0] == "height":
                    self.height = int(words[1])
                elif words[0] == "ascent":
                    self.baseline = int(words[1])
                elif words[0] == "char":
                    curr = int(words[1])
                    self.chars[curr] = ""

                self.chars[curr] += line

    def write(self):
        with open(filename, "w") as f:
            for ch in sorted(self.chars):
                text = self.chars[ch]
                while text[-2:] != "\n\n":
                    text += "\n" # ensure each part separated by blank line
                f.write(self.chars[ch])
            f.close()

    def getchar(self, ch):
        try:
            chdata = self.chars[ch]
        except KeyError:
            return None, None
        width = None
        bitmap = []
        for line in chdata.splitlines():
            words = [w.lower() for w in line.split()]
            if len(words) == 0:
                pass # rule out this case for all the elifs
            elif words[0] == "width":
                width = int(words[1])
            elif words[0][0] in "01":
                row = 0
                for bit in words[0]:
                    row = row * 2 + int(bit)
                bitmap.append(row)
        return width, bitmap

    def putchar(self, ch, width, bitmap):
        chdata = "char %d\n" % ch
        chdata += "width %d\n" % width
        for row in bitmap:
            for b in range(width-1, -1, -1):
                chdata += "%d" % (1 & (row >> b))
            chdata += "\n"
        chdata += "\n"
        self.chars[ch] = chdata

    def maxchar(self):
        return 255 # fd fonts only support single-byte charsets AFAIK

    def charencodings(self):
        return sorted(self.chars.keys())

class BDF(Font):
    "Class to read and write BDF."
    def __init__(self, filename):
        ascent = descent = pointsize = resy = None
        self.chars = {}
        curr = (0,) # file header
        self.chars[curr] = ""
        with open(filename) as f:
            for line in f.readlines():
                words = [w for w in line.split()]
                if len(words) == 0:
                    pass # rule out this case for all the elifs
                elif words[0] == "CHARS" or words[0] == "ENDFONT":
                    continue # skip this line completely
                elif words[0] == "FONT_ASCENT":
                    ascent = int(words[1])
                elif words[0] == "FONT_DESCENT":
                    descent = int(words[1])
                elif words[0] == "POINT_SIZE":
                    pointsize = int(words[1])
                elif words[0] == "RESOLUTION_Y":
                    resy = int(words[1])
                elif words[0] == "STARTCHAR":
                    curr = (-1,) # special key indicating unknown encoding yet
                    charname = words[1]
                    self.chars[curr] = ""
                elif words[0] == "ENCODING":
                    assert curr == (-1,)
                    encoding = int(words[1])
                    if encoding >= 0:
                        curr = (1, encoding) # numeric encoding
                    else:
                        curr = (2, charname) # just a name
                    self.chars[curr] = self.chars[(-1,)]
                    del self.chars[(-1,)]

                self.chars[curr] += line
        self.baseline = ascent
        self.height = ascent + descent
        self.swscale = 720000. / (resy * pointsize)

    def write(self):
        with open(filename, "w") as f:
            for ch in sorted(self.chars):
                text = self.chars[ch]
                while text[-2:] != "\n\n":
                    text += "\n" # ensure each part separated by blank line
                while text[-3:] == "\n\n\n":
                    text = text[:-1] # ensure not _too_ many blank lines
                f.write(text)
                if ch == (0,):
                    f.write("CHARS %d\n\n" % (len(self.chars)-1))
            f.write("ENDFONT\n")
            f.close()

    def getchar(self, ch):
        try:
            chdata = self.chars[(1, ch)]
        except KeyError:
            return None, None
        width = None
        bitmap = []
        seen_bm_keyword = False
        bt, bb = 0, 0
        bl, br = 0, 0
        for line in chdata.splitlines():
            words = [w for w in line.split()]
            if len(words) == 0:
                pass # rule out this case for all the elifs
            elif words[0] == "SWIDTH":
                width = int(round(float(words[1]) / self.swscale))
            elif words[0] == "BITMAP":
                seen_bm_keyword = True
            elif words[0] == "BBX":
                bbxw, bbxh, bbxx, bbxy = map(int, words[1:5])
                bl = bbxx
                br = width - (bbxx + bbxw)
                bt = self.baseline - (bbxy + bbxh)
                bb = bbxy + self.height - self.baseline
                bitmap.extend([0] * bt)
            elif seen_bm_keyword and words[0] != "ENDCHAR":
                row = int(words[0], base=16)
                row = row >> (len(words[0]) * 4 - (width-br-bl))
                bitmap.append(row << br)
        bitmap.extend([0] * bb)
        return width, bitmap

    def putchar(self, ch, width, bitmap):
        chdata = "STARTCHAR uni%04x\nENCODING %d\n" % (ch, ch)
        chdata += "SWIDTH %d 0\nDWIDTH %d 0\n" % (self.swscale*width, width)
        chdata += "BBX %d %d 0 %d\n" % (
            width, self.height, self.baseline-self.height)
        chdata += "BITMAP\n"
        paddedbits = (width + 7) & ~7
        for row in bitmap:
            row <<= (paddedbits - width)
            chdata += "%0*X\n" % (paddedbits / 4, row)
        chdata += "ENDCHAR\n"
        self.chars[(1, ch)] = chdata

    def maxchar(self):
        return 0x10FFFF

    def charencodings(self):
        return sorted([x[1] for x in self.chars.keys() if x[0] == 1])

filename = sys.argv[1]
if filename[-3:] == ".fd":
    font = FD(filename)
elif filename[-4:] == ".bdf":
    font = BDF(filename)
else:
    assert False, "unrecognised font type"

root = Root(font)

mainloop()
