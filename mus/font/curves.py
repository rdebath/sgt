import sys
import types
from math import *
from crosspoint import crosspoint

def transform(matrix, x, y, affine=1):
    # 6-element matrix like PostScript's: applying [a,b,c,d,e,f] to
    # transform the point (x,y) returns (ax+cy+e, bx+dy+f).
    a,b,c,d,e,f = matrix
    x, y = a*x+c*y, b*x+d*y
    if affine:
        x, y = x+e, y+f # can omit this for transforming vectors
    return x, y

def translate(x,y):
    return [1, 0, 0, 1, x, y]

def scale(x,y):
    return [x, 0, 0, y, 0, 0]

class Curve:
    def __init__(self, cont = None):
        self.tkitems = []
        self.welds = [None, None]
        self.weldpri = 1
        self.nib = None

    def postinit(self, cont):
        self.tk_addto(cont.canvas)
        cont.curves[cont.curveid] = self
        cont.curveid = cont.curveid + 1
        self.cid = cont.curveid
        self.cont = cont

    def weld_to(self, end, other, oend, half=0, special=None):
        assert(self.welds[end] == None)
        self.welds[end] = (other, oend, half, special)
        if special != None:
            x, y, par, perp = special
            special = -x, -y, par, perp
        other.welds[oend] = (self, end, half, special)
        self.weld_update(end)

    def unweld(self, end):
        if self.welds[end] != None:
            other, oend, half, special = self.welds[end]
            other.welds[oend] = None
            self.welds[end] = None

    def cleanup(self):
        for x in self.tkitems:
            self.canvas.delete(x)
        for end in (0,1):
            if self.welds[end] != None:
                other, oend, half, special = self.welds[end]
                other.welds[oend] = None

    def weld_update(self, end, thispri = None):
        if not self.welds[end]:
            return
        other, oend, half, special = self.welds[end]

        ourepri = max(1, thispri)
        ourdpri = max(self.weldpri, thispri)
        otherepri = 1
        otherdpri = other.weldpri

        sx, sy, sdx, sdy = self.enddata(end)
        ox, oy, odx, ody = other.enddata(oend)

        slen = sqrt(sdx*sdx+sdy*sdy)

        if special:
            ox = ox - special[0] - special[2]*sdx/slen - special[3]*sdy/slen
            oy = oy - special[1] - special[2]*sdy/slen + special[3]*sdx/slen

        maxpri = max(ourepri, otherepri)
        ourmult = ourepri==maxpri
        othermult = otherepri==maxpri
        multsum = ourmult + othermult
        sx = ox = (sx * ourmult + ox * othermult) / multsum
        sy = oy = (sy * ourmult + oy * othermult) / multsum

        if special:
            ox = ox + special[0] + special[2]*sdx/slen + special[3]*sdy/slen
            oy = oy + special[1] + special[2]*sdy/slen - special[3]*sdx/slen

        if not half:
            maxpri = max(ourdpri, otherdpri)
            ourmult = ourdpri==maxpri
            othermult = otherdpri==maxpri
            multsum = ourmult + othermult
            dx = (sdx * ourmult - odx * othermult) / multsum
            dy = (sdy * ourmult - ody * othermult) / multsum
            if dx == 0 and dy == 0:
                dx = 1 # arbitrary
            sdx, sdy = dx, dy
            odx, ody = -dx, -dy

        self.setenddata(end, sx, sy, sdx, sdy)
        other.setenddata(oend, ox, oy, odx, ody)

    def compute_direction(self, t):
        x0, y0 = self.compute_point(t-0.0001)
        x2, y2 = self.compute_point(t+0.0001)
        return (x2-x0)/0.0002, (y2-y0)/0.0002

    def compute_theta(self, t):
        dx, dy = self.compute_direction(t)
        return atan2(-dy, dx)

    def compute_nib(self, t):
        x, y = self.compute_point(t)
        theta = self.compute_theta(t)
        nibfn = self.nib
        if nibfn == None:
            nibfn = self.cont.default_nib
        if type(nibfn) == types.FunctionType:
            return nibfn(self, x, y, t, theta)
        else:
            return nibfn

    def compute_x(self, t):
        return self.compute_point(t)[0]

    def compute_y(self, t):
        return self.compute_point(t)[1]

class CircleInvolute(Curve):
    def __init__(self, cont, x1, y1, dx1, dy1, x2, y2, dx2, dy2):
        Curve.__init__(self)
        self.inparams = (x1, y1, dx1, dy1, x2, y2, dx2, dy2)
        self.set_params()
        Curve.postinit(self, cont)

    def set_params(self):
        x1, y1, dx1, dy1, x2, y2, dx2, dy2 = self.inparams
        try:
            # Normalise the direction vectors.
            dlen1 = sqrt(dx1**2 + dy1**2); dx1, dy1 = dx1/dlen1, dy1/dlen1
            dlen2 = sqrt(dx2**2 + dy2**2); dx2, dy2 = dx2/dlen2, dy2/dlen2

            self.inparams = (x1, y1, dx1, dy1, x2, y2, dx2, dy2)

            # Find the normal vectors at each end by rotating the
            # direction vectors.
            nx1, ny1 = dy1, -dx1
            nx2, ny2 = dy2, -dx2

            # Find the crossing point of the normals.
            cx, cy = crosspoint(x1, y1, x1+nx1, y1+ny1, x2, y2, x2+nx2, y2+ny2)

            # Measure the distance from that crossing point to each
            # endpoint, and find the difference.
            #
            # The distance is obtained by taking the dot product with
            # the line's defining vector, so that it's signed.
            d1 = (cx-x1) * nx1 + (cy-y1) * ny1
            d2 = (cx-x2) * nx2 + (cy-y2) * ny2

            dd = d2 - d1

            # Find the angle between the two direction vectors. Since
            # they're already normalised to unit length, the magnitude
            # of this is just the inverse cosine of their dot product.
            # The sign must be chosen to reflect which way round they
            # are.
            theta = -acos(dx1*dx2 + dy1*dy2)
            if dx1*dy2 - dx2*dy1 > 0:
                theta = -theta

            # So we need a circular arc rotating through angle theta,
            # such that taking the involute of that arc with the right
            # length does the right thing.
            #
            # Suppose the circle has radius r. Then, when the circle
            # touches the line going to point 1, we need our string to
            # have length equal to d1 - r tan(theta/2). When it touches
            # the line going to point 2, the string length needs to be
            # d2 + r tan(theta/2). The difference between these numbers
            # must be equal to the arc length of the portion of the
            # circle in between, which is r*theta. Setting these equal
            # gives d2-d1 = r theta - 2 r tan(theta/2), which we solve
            # to get r = (d2-d1) / (theta - 2 tan (theta/2)).
            #
            # (In fact, we then flip the sign to take account of the way
            # we subsequently use r.)
            r = dd / (-theta + 2*tan(theta/2))

            # So how do we find the centre of a circle of radius r
            # tangent to both those lines? We shift the start point of
            # each line by r in the appropriate direction, and find
            # their crossing point again.
            cx2, cy2 = crosspoint(x1-r*dx1, y1-r*dy1, x1-r*dx1+nx1, y1-r*dy1+ny1, \
            x2-r*dx2, y2-r*dy2, x2-r*dx2+nx2, y2-r*dy2+ny2)

            # Now find the distance along each line to the centre of the
            # circle, which will be the string lengths at the endpoints.
            s1 = (cx2-x1) * nx1 + (cy2-y1) * ny1
            s2 = (cx2-x2) * nx2 + (cy2-y2) * ny2

            # Determine the starting angle.
            phi = atan2(dy1, dx1)

            # And that's it. We're involving a circle of radius r
            # centred at cx2,cy2; the centre of curvature proceeds from
            # angle phi to phi+theta, and the actual point on the curve
            # is displaced from that centre by an amount which changes
            # linearly with angle from s1 to s2. Store all that.
            self.params = (r, cx2, cy2, phi, theta, s1, s2-s1)
        except ZeroDivisionError, e:
            self.params = None # it went pear-shaped
        except TypeError, e:
            self.params = None # it went pear-shaped

    def transform(self, matrix):
        x1, y1, dx1, dy1, x2, y2, dx2, dy2 = self.inparams
        x1, y1 = transform(matrix, x1, y1)
        x2, y2 = transform(matrix, x2, y2)
        dx1, dy1 = transform(matrix, dx1, dy1, 0)
        dx2, dy2 = transform(matrix, dx2, dy2, 0)
        self.inparams = (x1, y1, dx1, dy1, x2, y2, dx2, dy2)
        self.set_params()

    def compute_point(self, t): # t in [0,1]
        assert self.params != None
        (r, cx, cy, phi, theta, s1, ds) = self.params

        angle = phi + theta * t
        s = s1 + ds * t
        dx = cos(angle)
        dy = sin(angle)
        nx, ny = -dy, dx
        x = cx + dx * r + nx * s
        y = cy + dy * r + ny * s
        return x, y

    def compute_direction(self, t): # t in [0,1]
        assert self.params != None
        (r, cx, cy, phi, theta, s1, ds) = self.params

        angle = phi + theta * t
        s = s1 + ds * t
        dx = cos(angle)
        dy = sin(angle)
        nx, ny = -dy, dx
        ddx = -sin(angle) * theta
        ddy = cos(angle) * theta
        dnx, dny = -ddy, ddx
        x = ddx * r + dnx * s + nx * ds
        y = ddy * r + dny * s + ny * ds
        return x, y

    def tk_refresh(self):
        coords = []
        x1, y1, dx1, dy1, x2, y2, dx2, dy2 = self.inparams
        if self.params == None:
            coords.extend([x1,y1])
            coords.extend([(2*x1+3*x2)/5 - (y1-y2)/14, (2*y1+3*y2)/5 + (x1-x2)/14])
            coords.extend([(3*x1+2*x2)/5 + (y1-y2)/14, (3*y1+2*y2)/5 - (x1-x2)/14])
            coords.extend([x2,y2])
        else:
            (r, cx, cy, phi, theta, s1, ds) = self.params
            dt = min(2 / abs(s1 * theta), 2 / abs((s1+ds) * theta))
            itmax = min(10000, int(1 / dt + 1))
            for it in range(itmax+1):
                t = it / float(itmax)
                x, y = self.compute_point(t)
                coords.extend([x,y])
        for x in self.tkitems:
            self.canvas.delete(x)
        if self.params == None:
            fill = "red"
        else:
            dx1a, dy1a = (lambda t: (cos(t),-sin(t)))(self.compute_theta(0))
            dx2a, dy2a = (lambda t: (cos(t),-sin(t)))(self.compute_theta(1))
            if dx1a * dx1 + dy1a * dy1 < 0 or dx2a * dx2 + dy2a * dy2 < 0:
                fill = "#ff0000"
            else:
                fill = "#00c000"
        self.tkitems.append(self.canvas.create_line(x1, y1, x1+25*dx1, y1+25*dy1, fill=fill))
        self.tkitems.append(self.canvas.create_line(x2, y2, x2-25*dx2, y2-25*dy2, fill=fill))
        self.tkitems.append(self.canvas.create_line(coords))

    def tk_addto(self, canvas):
        self.canvas = canvas
        self.tk_refresh()

    def tk_drag(self, x, y, etype):
        x1, y1, dx1, dy1, x2, y2, dx2, dy2 = self.inparams
        if etype == 1:
            xc1, yc1 = x1+25*dx1, y1+25*dy1
            xc2, yc2 = x2-25*dx2, y2-25*dy2
            if (x-x1)**2 + (y-y1)**2 < 32:
                self.dragpt = 1
            elif (x-x2)**2 + (y-y2)**2 < 32:
                self.dragpt = 4
            elif (x-xc1)**2 + (y-yc1)**2 < 32:
                self.dragpt = 2
            elif (x-xc2)**2 + (y-yc2)**2 < 32:
                self.dragpt = 3
            else:
                self.dragpt = None
                return 0
            return 1
        elif self.dragpt == None:
            return 0
        else:
            if self.dragpt == 1:
                x1 = x
                y1 = y
            elif self.dragpt == 4:
                x2 = x
                y2 = y
            elif self.dragpt == 2 and (x != x1 or y != y1):
                dx1 = x - x1
                dy1 = y - y1
            elif self.dragpt == 3 and (x != x1 or y != y1):
                dx2 = x2 - x
                dy2 = y2 - y
            self.inparams = (x1, y1, dx1, dy1, x2, y2, dx2, dy2)
            self.set_params()
            self.tk_refresh()
            end = (self.dragpt-1)/2
            self.weld_update(end, 2)
            return 1

    def enddata(self, end):
        x, y, dx, dy = self.inparams[4*end:4*(end+1)]
        if end:
            dx, dy = -dx, -dy
        return x, y, dx, dy

    def setenddata(self, end, x, y, dx, dy):
        ox, oy, odx, ody = self.inparams[4*end:4*(end+1)]
        if end:
            odx, ody = -odx, -ody
        changed = (x != ox or y != oy or dx * ody != dy * odx)
        if changed:
            if end:
                dx, dy = -dx, -dy
            p = list(self.inparams)
            p[4*end] = x
            p[4*end+1] = y
            p[4*end+2] = dx
            p[4*end+3] = dy
            self.inparams = tuple(p)
            self.set_params()
            self.tk_refresh()

    def findend(self, x, y):
        x1, y1, dx1, dy1, x2, y2, dx2, dy2 = self.inparams
        if (x-x1)**2 + (y-y1)**2 < 32:
            return 0
        elif (x-x2)**2 + (y-y2)**2 < 32:
            return 1
        else:
            return None

    def serialise(self):
        s = "CircleInvolute(cont, %g, %g, %g, %g, %g, %g, %g, %g)" % \
        self.inparams
        return s

class StraightLine(Curve):
    def __init__(self, cont, x1, y1, x2, y2):
        Curve.__init__(self)
        self.inparams = (x1, y1, x2, y2)
        self.weldpri = 3
        Curve.postinit(self, cont)

    def transform(self, matrix):
        x1, y1, x2, y2 = self.inparams
        x1, y1 = transform(matrix, x1, y1)
        x2, y2 = transform(matrix, x2, y2)
        self.inparams = (x1, y1, x2, y2)

    def compute_point(self, t): # t in [0,1]
        assert self.inparams != None
        (x1, y1, x2, y2) = self.inparams
        x = x1 + t * (x2-x1)
        y = y1 + t * (y2-y1)
        return x, y

    def tk_refresh(self):
        x1, y1, x2, y2 = self.inparams
        for x in self.tkitems:
            self.canvas.delete(x)
        self.tkitems.append(self.canvas.create_line(x1, y1, x2, y2))

    def tk_addto(self, canvas):
        self.canvas = canvas
        self.tk_refresh()

    def tk_drag(self, x, y, etype):
        x1, y1, x2, y2 = self.inparams
        if etype == 1:
            if (x-x1)**2 + (y-y1)**2 < 32:
                self.dragpt = 1
            elif (x-x2)**2 + (y-y2)**2 < 32:
                self.dragpt = 2
            else:
                self.dragpt = None
                return 0
            return 1
        elif self.dragpt == None:
            return 0
        else:
            if self.dragpt == 1:
                x1 = x
                y1 = y
            elif self.dragpt == 2:
                x2 = x
                y2 = y
            self.inparams = (x1, y1, x2, y2)
            self.tk_refresh()
            for end in 0,1:
                self.weld_update(end)
            return 1

    def enddata(self, end):
        x1, y1, x2, y2 = self.inparams
        if end:
            x1, y1, x2, y2 = x2, y2, x1, y1
        return x1, y1, x2-x1, y2-y1

    def setenddata(self, end, x, y, dx, dy):
        ox, oy = self.inparams[2*end:2*(end+1)]
        changed = (x != ox or y != oy)
        if changed:
            p = list(self.inparams)
            p[2*end] = x
            p[2*end+1] = y
            self.inparams = tuple(p)
            self.tk_refresh()

    def findend(self, x, y):
        x1, y1, x2, y2 = self.inparams
        if (x-x1)**2 + (y-y1)**2 < 32:
            return 0
        elif (x-x2)**2 + (y-y2)**2 < 32:
            return 1
        else:
            return None

    def serialise(self):
        s = "StraightLine(cont, %g, %g, %g, %g)" % self.inparams
        return s
    
class Bezier(Curve):
    def __init__(self, cont, x1, y1, x2, y2, x3, y3, x4, y4):
        Curve.__init__(self)
        self.inparams = (x1, y1, x2, y2, x3, y3, x4, y4)
        self.weldpri = 1
        Curve.postinit(self, cont)

    def transform(self, matrix):
        (x1, y1, x2, y2, x3, y3, x4, y4) = self.inparams
        x1, y1 = transform(matrix, x1, y1)
        x2, y2 = transform(matrix, x2, y2)
        x3, y3 = transform(matrix, x3, y3)
        x4, y4 = transform(matrix, x4, y4)
        self.inparams = (x1, y1, x2, y2, x3, y3, x4, y4)

    def compute_point(self, t): # t in [0,1]
        assert self.inparams != None
        (x1, y1, x2, y2, x3, y3, x4, y4) = self.inparams
        x = x1 * (1-t)**3 + x2 * 3*(1-t)**2*t + x3 * 3*(1-t)*t**2 + x4 * t**3
        y = y1 * (1-t)**3 + y2 * 3*(1-t)**2*t + y3 * 3*(1-t)*t**2 + y4 * t**3
        return x, y

    def tk_refresh(self):
        coords = []
        (x1, y1, x2, y2, x3, y3, x4, y4) = self.inparams
        maxcdiff = max(abs(x1-x2),abs(x2-x3),abs(x3-x4),abs(x1-x3),abs(x2-x4),abs(x1-x4),abs(y1-y2),abs(y2-y3),abs(y3-y4),abs(y1-y3),abs(y2-y4),abs(y1-y4))
        itmax = min(int(maxcdiff*3+1), 1000)
        for it in range(itmax+1):
            t = it / float(itmax)
            x, y = self.compute_point(t)
            coords.extend([x,y])
        for x in self.tkitems:
            self.canvas.delete(x)
        fill = "#00c000"
        self.tkitems.append(self.canvas.create_line(x1, y1, x2, y2, fill=fill))
        self.tkitems.append(self.canvas.create_line(x3, y3, x4, y4, fill=fill))
        self.tkitems.append(self.canvas.create_line(coords))

    def tk_addto(self, canvas):
        self.canvas = canvas
        self.tk_refresh()

    def tk_drag(self, x, y, etype):
        (x1, y1, x2, y2, x3, y3, x4, y4) = self.inparams
        if etype == 1:
            if (x-x1)**2 + (y-y1)**2 < 32:
                self.dragpt = 1
            elif (x-x2)**2 + (y-y2)**2 < 32:
                self.dragpt = 2
            elif (x-x3)**2 + (y-y3)**2 < 32:
                self.dragpt = 3
            elif (x-x4)**2 + (y-y4)**2 < 32:
                self.dragpt = 4
            else:
                self.dragpt = None
                return 0
            return 1
        elif self.dragpt == None:
            return 0
        else:
            if self.dragpt == 1:
                x2 = x2 - x1 + x
                y2 = y2 - y1 + y
                x1 = x
                y1 = y
            elif self.dragpt == 2:
                x2 = x
                y2 = y
            elif self.dragpt == 3:
                x3 = x
                y3 = y
            elif self.dragpt == 4:
                x3 = x3 - x4 + x
                y3 = y3 - y4 + y
                x4 = x
                y4 = y
            self.inparams = (x1, y1, x2, y2, x3, y3, x4, y4)
            self.tk_refresh()
            for end in 0,1:
                self.weld_update(end)
            return 1

    def enddata(self, end):
        x1, y1, x2, y2, x3, y3, x4, y4 = self.inparams
        if end:
            x1, y1, x2, y2 = x4, y4, x3, y3
        return x1, y1, x2-x1, y2-y1

    def setenddata(self, end, x, y, dx, dy):
        dlen = sqrt(dx*dx + dy*dy)
        dx, dy = dx/dlen, dy/dlen

        ox, oy = self.inparams[6*end:6*end+2]
        oxc, oyc = self.inparams[2*end+2:2*end+4]
        odx, ody = oxc-ox, oyc-oy
        odlen = sqrt(odx*odx + ody*ody)
        odx, ody = odx/odlen, ody/odlen
        changed = (x != ox or y != oy or dx != odx or dy != ody)
        if changed:
            p = list(self.inparams)
            p[6*end] = x
            p[6*end+1] = y
            p[2*end+2] = x + dx * odlen
            p[2*end+3] = y + dy * odlen
            self.inparams = tuple(p)
            self.tk_refresh()

    def findend(self, x, y):
        x1, y1, x2, y2, x3, y3, x4, y4 = self.inparams
        if (x-x1)**2 + (y-y1)**2 < 32:
            return 0
        elif (x-x4)**2 + (y-y4)**2 < 32:
            return 1
        else:
            return None

    def serialise(self):
        s = "Bezier(cont, %g, %g, %g, %g, %g, %g, %g, %g)" % self.inparams
        return s
