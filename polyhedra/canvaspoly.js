/* -*- java -*-
 *
 * JavaScript code to render a rotatable wire-frame polyhedron in an
 * HTML 5 canvas.
 */

/*
 * To do:
 *
 *  - set up a mechanism for getting the polyhedron description out
 *    of the HTML, to make it easy to put many of these canvases on
 *    the same page.
 *
 *  - work out how to handle drags off the edge of the canvas.
 *    Currently, if we drag off the edge and then let go, the
 *    polyhedron starts moving again when we bring the mouse back
 *    on without the button pressed.
 *
 *  - implement spinning.
 */

/* ----------------------------------------------------------------------
 * Basic functions for manipulating 3-d matrices and vectors.
 *
 * 3x3 matrices in this program are represented as objects with
 * fields "xx", "xy" etc. Those indices are rows-first, so the
 * matrix when drawn in normal maths notation looks like
 * 
 *  ( xx xy xz )
 *  ( yx yy yz )
 *  ( zx zy zz )
 *
 * Vectors are represented as objects with fields "x", "y", "z".
 */

/*
 * Multiply a vector by a matrix, returning a new vector.
 */
function vtrans(m, v) {
    return {x: m.xx*v.x + m.xy*v.y + m.xz*v.z,
	    y: m.yx*v.x + m.yy*v.y + m.yz*v.z,
	    z: m.zx*v.x + m.zy*v.y + m.zz*v.z};
}

/*
 * Multiply two matrices, returning a new matrix.
 */
function matmul(m, n) {
    return {xx: m.xx*n.xx + m.xy*n.yx + m.xz*n.zx,
	    xy: m.xx*n.xy + m.xy*n.yy + m.xz*n.zy,
	    xz: m.xx*n.xz + m.xy*n.yz + m.xz*n.zz,
	    yx: m.yx*n.xx + m.yy*n.yx + m.yz*n.zx,
	    yy: m.yx*n.xy + m.yy*n.yy + m.yz*n.zy,
	    yz: m.yx*n.xz + m.yy*n.yz + m.yz*n.zz,
	    zx: m.zx*n.xx + m.zy*n.yx + m.zz*n.zx,
	    zy: m.zx*n.xy + m.zy*n.yy + m.zz*n.zy,
	    zz: m.zx*n.xz + m.zy*n.yz + m.zz*n.zz};
}

/*
 * Normalise a vector to unit length, unless it's the zero vector,
 * in which case we still return the zero vector.
 */
function vnorm(v) {
    var length = Math.sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (length == 0)
	length = 1;
    return {x: v.x/length, y: v.y/length, z: v.z/length};
}

/*
 * Vector dot product.
 */
function vdot(v, w) {
    return v.x*w.x + v.y*w.y + v.z*w.z;
}

/*
 * Vector cross product.
 */
function vcross(v, w) {
    return {x: v.y*w.z-v.z*w.y,
	    y: v.z*w.x-v.x*w.z,
	    z: v.x*w.y-v.y*w.x};
}

/*
 * Scalar multiple of a vector.
 */
function vscale(v, k) {
    return {x: v.x*k, y: v.y*k, z: v.z*k};
}

/*
 * Sum of two vectors.
 */
function vadd(v, w) {
    return {x: v.x+w.x, y: v.y+w.y, z: v.z+w.z};
}

/*
 * Difference of two vectors.
 */
function vsub(v, w) {
    return {x: v.x-w.x, y: v.y-w.y, z: v.z-w.z};
}

/*
 * Rotate a vector by a given angle about a given unit-vector
 * axis.
 */
function vrotate(v, axis, angle) {
    /*
     * The component of v perpendicular to the rotation plane
     * is invariant.
     */
    var vinv = vscale(axis, vdot(v, axis));

    /*
     * Subtract that off to find the component _in_ the
     * rotation plane.
     */
    var vrot = vsub(v, vinv);

    /*
     * Find the vector perpendicular to both vrot and axis.
     * Since the two input vectors are perpendicular already
     * and axis is a unit vector, this must be the same length
     * as vrot.
     */
    var vperp = vcross(axis, vrot);

    /*
     * Trigonometrically combine vrot and vperp to find the
     * rotated version of vrot.
     */
    vrot = vadd(vscale(vrot, Math.cos(angle)),
		vscale(vperp, Math.sin(angle)));

    /*
     * Add vinv back in, and we're done.
     */
    return vadd(vinv, vrot);
}

/*
 * Return a matrix that rotates by a given angle about a given
 * unit-vector axis.
 */
function mrotate(axis, angle) {
    var vx = vrotate({x:1,y:0,z:0}, axis, angle);
    var vy = vrotate({x:0,y:1,z:0}, axis, angle);
    var vz = vrotate({x:0,y:0,z:1}, axis, angle);
    var newmatrix = {
	xx:vx.x, xy:vy.x, xz:vz.x,
	yx:vx.y, yy:vy.y, yz:vz.y,
	zx:vx.z, zy:vy.z, zz:vz.z
    };
    return newmatrix;
}

/*
 * Normalise an orthogonal matrix and ensure it really is
 * orthogonal, by taking it apart into unit vectors, taking their
 * cross products to ensure real orthogonality, and normalising them
 * to ensure real unitness.
 *
 * We do this periodically to any matrix we're subjecting to an
 * ongoing series of transformations. We can tolerate small errors
 * in _what_ orthogonal matrix it represents, but it must carry on
 * _being_ an orthogonal matrix or we're really in trouble.
 */
function morthog(m) {
    var ex = {x:m.xx, y:m.yx, z:m.zx};
    var ey = {x:m.xy, y:m.yy, z:m.zy};
    var ez = {x:m.xz, y:m.yz, z:m.zz};

    ez = vcross(ex, ey);
    ey = vcross(ez, ex);

    ex = vnorm(ex);
    ey = vnorm(ey);
    ez = vnorm(ez);

    return {xx:ex.x, xy:ey.x, xz:ez.x,
	    yx:ex.y, yy:ey.y, yz:ez.y,
	    zx:ex.z, zy:ey.z, zz:ez.z};
}

/* ----------------------------------------------------------------------
 * Polyhedron-handling code.
 *
 * A polyhedron is an object containing fields "points" and "faces".
 * "points" in turn is an array of vector objects, while "faces" is
 * an array of objects each of which is both a vector (the face's
 * outward-pointing normal) and also has a field "points" containing
 * a list, in order round the face, of the vertices on the face,
 * specified as indices into the "points" array.
 */

/*
 * Transform a polyhedron structure by an orthogonal matrix,
 * returning a new polyhedron structure without altering the
 * original one.
 */
function ptrans(m, poly) {
    var newpts = [];
    for (var i = 0; i < poly.points.length; i++) {
	var p = poly.points[i];
	newpts.push(vtrans(m, p));
    }
    var newfaces = [];
    for (var i = 0; i < poly.faces.length; i++) {
	var f = poly.faces[i];
	var newface = vtrans(m, f);
	newface.points = f.points;
	newfaces.push(newface);
    }
    return {points:newpts, faces:newfaces};
}

/* Global constant: distance from origin to camera. */
var pdistance = 50;

/*
 * Work out a coordinate system centred on the canvas and scaled to
 * fit it.
 */
function canvascoords(canvas)
{
    var ox = canvas.width/2, oy = canvas.height/2, r = (ox < oy ? ox : oy);
    var scale = r * 0.9 / Math.asin(1/pdistance);
    /*
     * We scale the polyhedron to fit the canvas follows: if our
     * entire polyhedron is contained within a unit sphere about the
     * origin, and we compute perspective coordinates by adding
     * pdistance to the z-coordinate before dividing by it, then no
     * perspective coordinate can exceed asin(1/pdistance) in any
     * direction. We therefore scale that in turn to most of our
     * available radius.
     */
    return {ox:ox,oy:oy,scale:scale};
}

/*
 * Draw a polyhedron on a 2-d canvas context.
 */
function pdraw(canvas, poly) {
    var ctx = canvas.getContext('2d');

    var coords = canvascoords(canvas);

    /*
     * Work out the 2-d coordinates for each point, by applying
     * perspective and scaling to fit the canvas.
     */
    var ourpts = [];
    for (var i = 0; i < poly.points.length; i++) {
	var p = poly.points[i];
	/*
	 * Apply perspective.
	 */
	var px = p.x / (pdistance + p.z);
	var py = p.y / (pdistance + p.z);

	/*
	 * Scale and translate into canvas coordinates.
	 */
	ourpts.push({x: coords.ox + coords.scale * px,
		     y: coords.oy + coords.scale * py});
    }

    /*
     * Work out which faces are backward-facing and which forward.
     */
    var backfaces = [], frontfaces = [];
    for (var i = 0; i < poly.faces.length; i++) {
	var f = poly.faces[i];
	/*
	 * The face is facing the viewer iff a vector from the
	 * camera point (the origin) to somewhere on the face has
	 * negative dot product with that normal.
	 */
	var facevec = vadd(poly.points[f.points[0]], {x:0,y:0,z:pdistance});
	var dotprod = vdot(f, facevec);

	if (dotprod > 0)
	    backfaces.push(f);
	else
	    frontfaces.push(f);
    }

    /*
     * Now we're ready to actually render the faces.
     */
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    var passes = [{linewid:1, colour:'#808080', faces:backfaces},
		  {linewid:2, colour:'#000000', faces:frontfaces}];
    for (var i = 0; i < passes.length; i++) {
	var pass = passes[i];
	ctx.lineWidth = pass.linewid;
	ctx.strokeStyle = pass.colour;
	ctx.lineJoin = ctx.lineCap = "round";
	ctx.beginPath();
	for (var j = 0; j < pass.faces.length; j++) {
	    var f = pass.faces[j];
	    ctx.moveTo(ourpts[f.points[0]].x, ourpts[f.points[0]].y);
	    for (k = 1; k < f.points.length; k++)
		ctx.lineTo(ourpts[f.points[k]].x, ourpts[f.points[k]].y);
	    ctx.closePath();
	}
	ctx.stroke();
    }
}

/* ----------------------------------------------------------------------
 * Top-level code: handle mouse actions and decide how and when to
 * draw what.
 */

function draw(state) {
    var newpoly = ptrans(state.matrix, state.poly);
    pdraw(state.canvas, newpoly);
}

/*
 * Call this once on a canvas to turn it into a
 * polyhedron-displaying applet. This function will take care of
 * assigning mouse and timeout handlers.
 */
function setupPolyhedron(acanvas, apoly) {
    /*
     * Set up persistent state for this canvas.
     */
    var state = { canvas: acanvas, poly: apoly };
    state.matrix = {xx:1, xy:0, xz:0,
	            yx:0, yy:1, yz:0,
	            zx:0, zy:0, zz:1};
    state.mode = 0; // 0 = stationary; 1 = dragging; 2 = spinning

    /*
     * Set up mouse handlers.
     */
    state.canvas.onmousedown = function(event) {
	var coords = canvascoords(state.canvas);
	var mx = (event.pageX-state.canvas.offsetLeft-coords.ox)/coords.scale * -pdistance;
	var my = (event.pageY-state.canvas.offsetTop-coords.oy)/coords.scale * -pdistance;
	state.dragstart = vnorm({x:mx, y:my, z:1});
	state.mode = 1;
    }

    state.canvas.onmousemove = function(event) {
	var coords = canvascoords(state.canvas);
	var mx = (event.pageX-state.canvas.offsetLeft-coords.ox)/coords.scale * -pdistance;
	var my = (event.pageY-state.canvas.offsetTop-coords.oy)/coords.scale * -pdistance;
	if (state.mode == 1) {
	    var dragstart = state.dragstart;
	    var dragend = vnorm({x:mx, y:my, z:1});
	    var axis = vnorm(vcross(dragstart, dragend));
	    var perpstart = vcross(dragstart, axis);
	    var perpend = vcross(dragend, axis);
	    var rot = matmul({xx:axis.x, xy:dragend.x, xz:perpend.x,
		              yx:axis.y, yy:dragend.y, yz:perpend.y,
		              zx:axis.z, zy:dragend.z, zz:perpend.z},
			     {xx:axis.x, xy:axis.y, xz:axis.z,
		              yx:dragstart.x, yy:dragstart.y, yz:dragstart.z,
		              zx:perpstart.x, zy:perpstart.y, zz:perpstart.z});
	    state.matrix = morthog(matmul(rot, state.matrix));
	    draw(state);
	    state.dragstart = dragend;
	}
    }

    state.canvas.onmouseup = function(event) {
	state.mode = 0;
    }

    /*
     * Draw the polyhedron in its initial orientation.
     */
    draw(state);
}

var testpoly = {
    points: [{ x:-1, y:0, z:0 },
	     { x:0, y:-1, z:0 },
	     { x:1, y:0, z:0 },
	     { x:0, y:0, z:-1 },
	     { x:0, y:1, z:0 },
	     { x:0, y:0, z:1 }],
    faces: [{ points: [5,4,0], x:-0.57735026918962573, y:0.57735026918962573, z:0.57735026918962573 },
	    { points: [3,4,2], x:0.57735026918962573, y:0.57735026918962573, z:-0.57735026918962573 },
	    { points: [2,4,5], x:0.57735026918962573, y:0.57735026918962573, z:0.57735026918962573 },
	    { points: [0,1,5], x:-0.57735026918962573, y:-0.57735026918962573, z:0.57735026918962573 },
	    { points: [3,1,0], x:-0.57735026918962573, y:-0.57735026918962573, z:-0.57735026918962573 },
	    { points: [2,1,3], x:0.57735026918962573, y:-0.57735026918962573, z:-0.57735026918962573 },
	    { points: [5,1,2], x:0.57735026918962573, y:-0.57735026918962573, z:0.57735026918962573 },
	    { points: [0,4,3], x:-0.57735026918962573, y:0.57735026918962573, z:-0.57735026918962573 }]
};

function initCanvasPolyhedra() {
    var canvases = document.getElementsByTagName('canvas');
    for (var i = 0; i < canvases.length; i++) {
	var canvas = canvases[i];
	setupPolyhedron(canvas, testpoly);
    }
}
