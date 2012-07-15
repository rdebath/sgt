#!/usr/bin/env python

import sys
from reportlab.pdfgen.canvas import Canvas
from reportlab.lib.units import cm, mm, inch
from reportlab.lib.utils import ImageReader
import reportlab.lib.pagesizes

pagesizes = {
    "A0": reportlab.lib.pagesizes.A0,
    "A1": reportlab.lib.pagesizes.A1,
    "A2": reportlab.lib.pagesizes.A2,
    "A3": reportlab.lib.pagesizes.A3,
    "A4": reportlab.lib.pagesizes.A4,
    "A5": reportlab.lib.pagesizes.A5,
    "A6": reportlab.lib.pagesizes.A6,
}
for s in pagesizes.keys():
    pagesizes[s+"L"] = reportlab.lib.pagesizes.landscape(pagesizes[s])
    pagesizes[s+"P"] = reportlab.lib.pagesizes.portrait(pagesizes[s])

def die(*args):
    sys.stderr.write(" ".join(["imagepdf.py:"] + map(str,args)) + "\n")
    sys.exit(1)

class Dimension(object):
    def __init__(self, s, rel=True):
        if isinstance(s, Dimension):
            self.abs = s.abs
            if not self.abs:
                self.imgsize = s.imgsize
            self.val = s.val
        else:
            if s[-2:] == "cm":
                self.abs = True
                unit = cm
                s = s[:-2]
            elif s[-2:] == "mm":
                self.abs = True
                unit = mm
                s = s[:-2]
            elif s[-2:] == "in":
                self.abs = True
                unit = inch
                s = s[:-2]
            elif s[-2:] == "pt":
                self.abs = True
                unit = inch/72.0
                s = s[:-2]
            elif s[-2:] == "px" and rel:
                self.abs = False
                self.imgsize = False
                unit = 1
                s = s[:-2]
            elif s[-1:] == "x" and rel:
                self.abs = False
                self.imgsize = True
                unit = 1
                s = s[:-1]
            else:
                raise ValueError, "dimensions must end in cm, mm, in, pt, px or x"
            self.val = float(s) * unit
    def normalise(self, pixels, size):
        ret = Dimension(self)
        if not ret.abs:
            ret.val = ret.val * size
            if not ret.imgsize:
                ret.val = ret.val / pixels
            ret.abs = True
        return ret
    def pixelise(self, pixels):
        ret = Dimension(self)
        if not ret.abs and ret.imgsize:
            ret.val = ret.val * pixels
            ret.imgsize = False
        return ret

# top-level defaults
outfile = "image.pdf"
pagesize = pagesizes["A4P"]
margin = Dimension("15pt")
croplen = Dimension("10pt")
cropdist = Dimension("5pt")
verbose = False
pages = [[]]
currentimage = {}

def expectedarg():
    die("expected an argument after", opt)
def expectednoarg():
    die("expected no argument after", opt)
def longoptarg():
    global args, optval
    if optval == None:
        try:
            optval = args.pop(0)
        except IndexError:
            expectedarg()
def longoptnoarg():
    global args, optval
    if optval != None:
        expectednoarg()

args = sys.argv[1:]
doingopts = True

while len(args) > 0:
    arg = args.pop(0)
    if arg[0] == "-" and doingopts:
        if arg[1] == "-":
            # Long option.
            try:
                i = arg.index("=")
                opt = arg[:i]
                optval = arg[i+1:]
            except ValueError:
                opt = arg[:]
                optval = None
            if opt == "--":
                # Special case: inhibit further option processing.
                # This actually isn't much use in this particular
                # application, but it's traditional :-)
                doingopts = False
            elif opt == "--output":
                longoptarg()
                outfile = optval
            elif opt == "--verbose":
                longoptnoarg()
                verbose = True
            elif opt == "--pagesize":
                longoptarg()
                try:
                    pagesize = pagesizes[optval.upper()]
                except KeyError:
                    die("unrecognised page size '%s'" % optval)
            elif opt in ["--cropmark", "--crop", "--cropmarks", "--crops"]:
                longoptnoarg()
                currentimage['crops'] = True
            elif opt in ["--xcrop", "--xcrops"]:
                longoptarg()
                try:
                    currentimage['xcrops'] = map(Dimension, optval.split(","))
                except ValueError:
                    die("expected dimensions as crop positions")
            elif opt in ["--ycrop", "--ycrops"]:
                longoptarg()
                try:
                    currentimage['ycrops'] = map(Dimension, optval.split(","))
                except ValueError:
                    die("expected dimensions as crop positions")
            elif opt == "--xsplit":
                longoptarg()
                try:
                    currentimage['xsplit'] = map(float, optval.split(","))
                except ValueError:
                    die("expected list of numbers as crop positions")
            elif opt == "--ysplit":
                longoptarg()
                try:
                    currentimage['ysplit'] = map(float, optval.split(","))
                except ValueError:
                    die("expected list of numbers as crop positions")
            elif opt == "--croplen":
                longoptarg()
                try:
                    croplen = Dimension(optval,rel=False)
                except ValueError:
                    die("expected a dimension as crop mark length")
            elif opt == "--cropdist":
                longoptarg()
                try:
                    cropdist = Dimension(optval,rel=False)
                except ValueError:
                    die("expected a dimension as crop mark distance")
            elif opt in ["--margin", "--gutter"]:
                longoptarg()
                try:
                    margin = Dimension(optval,rel=False)
                except ValueError:
                    die("expected a dimension as margin width")
            elif opt == "--help":
                longoptnoarg()
                print """
usage: imagepdf.py [options] [imageoptions] image [[imageoptions] image...]
options across all images:
      -o, --output FILENAME  specify name of output PDF file
      -p, --pagesize SIZE    specify page size (A4, A4P, A4L, ...)
      --margin DIM           space to avoid around edge of page (default 15pt)
      --croplen DIM          length of crop marks (default 10pt)
      --cropdist DIM         distance from crop marks to image boundary (5pt)
      -v, --verbose          print diagnostics about image placement
options per image:
      --xsplit D[,POS[,N]]   split page into D columns, start at column POS
                                 (default 0), use N columns (default 1)
      --ysplit D[,POS[,N]]   likewise for rows (upwards from bottom)
      --crop                 print crop marks around image
      --xcrop DIM,DIM,DIM    specify x-positions of crop marks
      --ycrop DIM,DIM,DIM    specify y-positions of crop marks (up from bottom)
      -x DIM                 specify x-position of left of image
      -y DIM                 specify y-position of bottom of image
      -w DIM                 specify width of whole image
      -h DIM                 specify height of whole image
      -X DPI                 specify DPI in x direction
      -Y DPI                 specify DPI in y direction
      -D DPI                 specify DPI in both directions at once
      -C ANGLE               rotate clockwise by ANGLE (multiple of 90)
      -A ANGLE               rotate anticlockwise by ANGLE (multiple of 90)
options between images:
      -n                     start a new page
dimension syntax:
      NNNcm, NNNin, NNNpt    absolute dimensions in centimetres, inches, points
      NNNpx                  dimension in image pixels
      NNNx                   dimension as multiple of whole image width/height
examples:
      for an A4 greetings card (use any image with about 1:sqrt(2) ratio):
        imagepdf.py --ysplit 2 [rotate] image.png -o card.pdf
      for an A5 greetings card (same kind of image):
        imagepdf.py --ysplit 2 --xsplit 2,1 [rotate] image.png -o card.pdf
      for two A5 greetings cards (same kind of image again, but two of them):
        imagepdf.py --ysplit 2   --xsplit 2,1 [rotate] image1.png \\
                    --ysplit 2,1 --xsplit 2,1 [rotate] image2.png -o card.pdf
      for a batch of gift tags (use images with aspect ratio of about 2:3):
        imagepdf.py -p A4L -o tags.pdf \\
             --xsplit 5.2,0.1 -w 2in --crop --ycrop 0x,1x,2x picture1.png \\
             --xsplit 5.2,1.1 -w 2in --crop --ycrop 0x,1x,2x picture2.png \\
             --xsplit 5.2,2.1 -w 2in --crop --ycrop 0x,1x,2x picture3.png \\
             --xsplit 5.2,3.1 -w 2in --crop --ycrop 0x,1x,2x picture4.png \\
             --xsplit 5.2,4.1 -w 2in --crop --ycrop 0x,1x,2x picture5.png
      for a DVD case (273mm x 183mm aspect ratio; spine is middle 14mm):
        imagepdf.py -p A4L -o cover.pdf -w 273mm \\
             --crop --xcrop 0x,129.5mm,143.5mm,1x pic.png
"""[1:-1] # get rid of leading/trailing newlines
                sys.exit(0)
            else:
                die("unrecognised option", opt)
        else:
            # Short option(s).
            arg = arg[1:]
            while len(arg) > 0:
                c = arg[0]
                opt = "-"+c
                arg = arg[1:]
                if c in "opxywhXYDCA": # short options requiring arguments
                    if arg != "":
                        optval = arg
                    else:
                        try:
                            optval = args.pop(0)
                        except IndexError:
                            expectedarg()
                    if c == 'o':
                        outfile = optval
                    elif c == 'p':
                        try:
                            pagesize = pagesizes[optval.upper()]
                        except KeyError:
                            die("unrecognised page size '%s'" % optval)
                    elif c == 'x':
                        try:
                            currentimage['x'] = Dimension(optval,rel=False)
                        except ValueError:
                            die("expected a dimension as x coordinate")
                    elif c == 'y':
                        try:
                            currentimage['y'] = Dimension(optval,rel=False)
                        except ValueError:
                            die("expected a dimension as y coordinate")
                    elif c == 'w':
                        try:
                            currentimage['w'] = Dimension(optval,rel=False)
                        except ValueError:
                            die("expected a dimension as width")
                    elif c == 'h':
                        try:
                            currentimage['h'] = Dimension(optval,rel=False)
                        except ValueError:
                            die("expected a dimension as height")
                    elif c == 'X':
                        try:
                            currentimage['xdpi'] = float(optval)
                        except ValueError:
                            die("expected a number as x DPI value")
                    elif c == 'Y':
                        try:
                            currentimage['ydpi'] = float(optval)
                        except ValueError:
                            die("expected a number as y DPI value")
                    elif c == 'D':
                        try:
                            dpi = float(optval)
                            currentimage['xdpi'] = currentimage['ydpi'] = dpi
                        except ValueError:
                            die("expected a number as DPI value")
                    elif c == 'C':
                        try:
                            currentimage['angle'] = -int(optval)
                            if currentimage['angle'] % 90:
                                die("expected a multiple of 90 as rotation")
                        except ValueError:
                            die("expected a number as rotation")
                    elif c == 'A':
                        try:
                            currentimage['angle'] = int(optval)
                            if currentimage['angle'] % 90:
                                die("expected a multiple of 90 as rotation")
                        except ValueError:
                            die("expected a number as rotation")
                else:
                    # Short options requiring no arguments.
                    if c == 'n':
                        pages.append([])
                    elif c == 'v':
                        verbose = True
                    else:
                        die("unrecognised option", opt)
    else:
        # Filename.
        currentimage['filename'] = arg
        pages[-1].append(currentimage)
        currentimage = {}

pdf = Canvas(outfile, pagesize=pagesize)
for page in pages:
    for image in page:
        angle = image.get("angle", 0)
        angle = (angle / 90) & 3
        cropdefault = [Dimension("0x"),Dimension("1x")]

        for k in 'w','h':
            if k in image:
                image[k] = image[k].val

        img = ImageReader(image['filename'])
        image['pw'], image['ph'] = img.getSize() # pixel width+height

        if angle & 1:
            image["pw"], image["ph"] = image["ph"], image["pw"]

        # Deal with hsplit/vsplit.
        for plo,phi,pi,split in (("xlo","xhi",0,"xsplit"),
                                 ("ylo","yhi",1,"ysplit")):
            lo = 0
            hi = pagesize[pi]
            split = image.get(split, [1])
            if len(split) < 2: split.append(0)
            if len(split) < 3: split.append(1)
            hi = hi / split[0]
            lo = lo + hi * split[1]
            hi = lo + hi * split[2]
            image[plo] = lo
            image[phi] = hi

        # Work out width and height of image.
        autodimensions = []
        for dim,dpi,pix,crops,plo,phi in (
            ("w","xdpi","pw","xcrops","xlo","xhi"),
            ("h","ydpi","ph","ycrops","ylo","yhi")
        ):
            if dim in image:
                pass # already got it
            elif dpi in image:
                image[dim] = image[pix] * 72.0 / image[dpi]
            else:
                lo = image[plo] + margin.val
                hi = image[phi] - margin.val

                pixmin, pixmax = 0, image[pix]
                if image.get('crops',False):
                    lo = lo + croplen.val + cropdist.val
                    hi = hi - croplen.val + cropdist.val

                    cropposns = image.get(crops, cropdefault)
                    for cpos in cropposns:
                        if not cpos.abs:
                            cpos = cpos.pixelise(image[pix])
                            pixmin = min(cpos.val, pixmin)
                            pixmax = max(cpos.val, pixmax)

                thisdpi = (pixmax-pixmin) * 72.0 / (hi-lo)
                autodimensions.append((dim, pix, thisdpi))
        if len(autodimensions) == 1:
            # If we have one dimension known, work out the other by
            # preserving aspect ratio.
            if "w" in image:
                image["h"] = image["ph"] * image["w"] / float(image["pw"])
            else: # "h" in image
                image["w"] = image["pw"] * image["h"] / float(image["ph"])
        elif len(autodimensions) > 1:
            thisdpi = reduce(max, [thisdpi for (dim, pix, thisdpi)
                                   in autodimensions])
            for (dim, pix, ignore) in autodimensions:
                image[dim] = image[pix] * 72.0 / thisdpi

        # Work out coordinates of image.
        for pos, dim, pi, minval, maxval, crops, plo, phi in (
            ("x","w",0,"x0","x1","xcrops","xlo","xhi"),
            ("y","h",1,"y0","y1","ycrops","ylo","yhi")
        ):
            if pos in image:
                pass # already got it
            else:
                lo = image[plo] + margin.val
                hi = image[phi] - margin.val

                pmin, pmax = 0, image[dim]
                if image.get('crops',False):
                    lo = lo + croplen.val + cropdist.val
                    hi = hi - croplen.val + cropdist.val

                    cropposns = image.get(crops, cropdefault)
                    for cpos in cropposns:
                        if not cpos.abs:
                            cpos = cpos.normalise(image[pix], image[dim])
                        pmin = min(cpos.val, pmin)
                        pmax = max(cpos.val, pmax)

                image[pos] = ((lo+hi) - (pmax+pmin)) / 2.0
                image[minval] = image[pos] + pmin
                image[maxval] = image[pos] + pmax

        # Draw crop marks.
        if image.get('crops',False):
            pdf.setLineWidth(inch/720.0)
            pdf.setLineCap(0)
            for xcrop in image.get("xcrops", cropdefault):
                if not xcrop.abs:
                    xcrop = xcrop.normalise(image["pw"], image["w"])
                pdf.line(xcrop.val + image["x"], image["y0"] - cropdist.val,
                         xcrop.val + image["x"], image["y0"] - cropdist.val - croplen.val)
                pdf.line(xcrop.val + image["x"], image["y1"] + cropdist.val,
                         xcrop.val + image["x"], image["y1"] + cropdist.val + croplen.val)
            for ycrop in image.get("ycrops", cropdefault):
                if not ycrop.abs:
                    ycrop = ycrop.normalise(image["ph"], image["h"])
                pdf.line(image["x0"] - cropdist.val, ycrop.val + image["y"],
                         image["x0"] - cropdist.val - croplen.val, ycrop.val + image["y"])
                pdf.line(image["x1"] + cropdist.val, ycrop.val + image["y"],
                         image["x1"] + cropdist.val + croplen.val, ycrop.val + image["y"])

        if verbose:
            print "%s: bl (%d,%d), tr (%d,%d), size (%d,%d), dpi (%g,%g)" % (
                image["filename"],
                image["x"], image["y"],
                image["x"]+image["w"], image["y"]+image["h"],
                image["w"], image["h"],
                image["pw"] * 72.0 / image["w"],
                image["ph"] * 72.0 / image["h"],
            )

        # Rotate.
        rotates = [
            lambda x,y,w,h: (x,y,w,h), # no rotation
            lambda x,y,w,h: (y, -x-w, h, w), # anticlockwise
            lambda x,y,w,h: (-x-w, -y-h, w, h), # 180
            lambda x,y,w,h: (-y-h, x, h, w), # clockwise
        ]
        image["x"], image["y"], image["w"], image["h"] = (
            rotates[angle](image["x"], image["y"], image["w"], image["h"])
        )

        # And draw the image.
        pdf.saveState()
        pdf.rotate(angle * 90)
        pdf.drawImage(img, image["x"], image["y"], image["w"], image["h"])
        pdf.restoreState()

    pdf.showPage()
pdf.save()
