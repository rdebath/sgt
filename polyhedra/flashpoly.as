/* -*- java -*- */

/*
 * Flash application source, suitable for building with MTASC,
 * which dynamically displays a convex polyhedron as a wireframe
 * perspective drawing and permits the user to rotate it
 * arbitrarily and/or set it constantly spinning.
 */

/*
 * To compile: mtasc -swf <outfile>.swf -main -header 300:300:50 <infile>.as
 *
 * The above frame rate of 50 can be changed arbitrarily without
 * the program breaking (I hope). The dimensions
 */

class Application {

   /*
    * The current transformation matrix between the input
    * polyhedron space and the output space.
    *
    * 3x3 matrices in this program are represented as objects with
    * fields "xx", "xy" etc. Those indices are rows-first, so the
    * matrix when drawn in normal maths notation looks like
    * 
    *  ( xx xy xz )
    *  ( yx yy yz )
    *  ( zx zy zz )
    */
   static var currmatrix;

   /*
    * The current axis of ongoing rotation, as an object with
    * fields "x", "y", "z" representing a unit vector.
    */
   static var curraxis;

   /*
    * The current angular speed, in radians per second. Always
    * positive; the sense of the rotation is given by the
    * direction of the curraxis vector.
    */
   static var curraspeed;

   /*
    * The absolute time returned from getTimer() at the start of
    * the most recent frame; used to compute inter-frame intervals
    * so as to retain a constant rotation speed whatever frame
    * rate the application is compiled for.
    */
   static var lastframetime;

   /*
    * The polyhedron itself. This is an object containing fields:
    *  * `points' is an array of objects with x,y,z fields giving
    * 	 the point's coordinates in polyhedron space
    *  * `faces' is an array of objects with x,y,z fields giving
    * 	 the face's normal vector, plus a points field giving an
    * 	 array of references to the above points.
    */
   static var poly;

   /*
    * The mouse state: whether the button is down, whether it was
    * down in the previous frame, and if so, where the pointer
    * was.
    */
   static var mousestate, lastmousestate, lastmousex, lastmousey;

   /*
    * Global constant: the virtual distance from origin to viewer
    * used in the perspective calculations.
    */
   static var pdistance = 50;

   /*
    * Global constant: the virtual distance from origin to screen
    * plane used in mouse drag processing.
    */
   static var mdistance = 1;

   /*
    * Multiply a vector by a matrix, returning a new vector.
    */
   static function mattrans(m, v) {
       return {x: m.xx*v.x + m.xy*v.y + m.xz*v.z,
	       y: m.yx*v.x + m.yy*v.y + m.yz*v.z,
	       z: m.zx*v.x + m.zy*v.y + m.zz*v.z};
   }

   /*
    * Multiply two matrices, returning a new matrix.
    */
   static function matmul(m, n) {
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
    * Normalise a vector to unit length.
    */
   static function vnorm(v) {
       var length = Math.sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
       if (length == 0)
	   length = 1; // shouldn't happen, we hope!
       return {x: v.x/length, y: v.y/length, z: v.z/length};
   }

   /*
    * Vector dot product.
    */
   static function vdot(v, w) {
       return v.x*w.x + v.y*w.y + v.z*w.z;
   }

   /*
    * Vector cross product.
    */
   static function vcross(v, w) {
       return {x: v.y*w.z-v.z*w.y,
	       y: v.z*w.x-v.x*w.z,
	       z: v.x*w.y-v.y*w.x};
   }

   /*
    * Scalar multiple of a vector.
    */
   static function vscale(v, k) {
       return {x: v.x*k, y: v.y*k, z: v.z*k};
   }

   /*
    * Sum of two vectors.
    */
   static function vadd(v, w) {
       return {x: v.x+w.x, y: v.y+w.y, z: v.z+w.z};
   }

   /*
    * Difference of two vectors.
    */
   static function vsub(v, w) {
       return {x: v.x-w.x, y: v.y-w.y, z: v.z-w.z};
   }

   /*
    * Rotate a vector by a given angle about a given unit-vector
    * axis.
    */
   static function vrotate(v, axis, angle) {
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
    * Normalise an orthogonal matrix and ensure it really is
    * orthogonal, by taking it apart into unit vectors, taking
    * their cross products to ensure real orthogonality, and
    * normalising them to ensure real unitness.
    *
    * We do this to avoid any possibility of gradual numerical
    * instability creeping in due to the fact that we're
    * constantly multiplying currmatrix by small incremental
    * rotations. We can tolerate small errors in _what_ orthogonal
    * matrix currmatrix represents, but it must carry on _being_
    * an orthogonal matrix or we're really in trouble.
    */
   static function morthog(m) {
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

   /*
    * Simple mouse-down and mouse-up event handlers. These just
    * track the mouse button state; all the real work happens in
    * newframe().
    */
   static function mousedown() { mousestate = true; }
   static function mouseup() { mousestate = false; }

   static var tmpdone;

   static function newframe() {
       var pt;
       var face;
       var i, j;
       var mousex, mousey;

       /*
	* Find the elapsed interval between the last frame and
	* this one. (We have to divide by 1000, since getTimer()
	* returns in milliseconds and we work internally in
	* seconds.)
	*/
       var newtime = getTimer();
       var interval = (newtime - lastframetime) / 1000;
       if (interval <= 0 || tmpdone)
	   return;		       /* shouldn't happen, but just in case */
       lastframetime = newtime;

       /*
	* FIXME: when debugging finished, remove the text field
	* and move the clear instruction down to the display loop.
	*/
       _root.clear();
       _root.createTextField("hello", 1, 10, 10, 280, 20);
       _root.hello.text = "";

       /*
	* Update the angular velocity of the solid if a drag has
	* taken place. Otherwise, leave the angular velocity the
	* way it was in the previous frame so that the solid keeps
	* rotating.
	*/
       mousex = _root._xmouse;
       mousey = _root._ymouse;
       if (mousestate && lastmousestate) {
	   /*
	    * A drag happened. First, compute the vectors from the
	    * origin to the initial and final mouse positions.
	    */
	   var vbefore = {x: (lastmousex-150)/150, y: (lastmousey-150)/150, z: -mdistance};
	   var vafter = {x: (mousex-150)/150, y: (mousey-150)/150, z: -mdistance};

	   vbefore = vnorm(vbefore);
	   vafter = vnorm(vafter);

	   /*
	    * Now figure out the axis of rotation required to turn
	    * the one vector into the other, by cross product.
	    * 
	    * Since both the input vectors have the same nonzero
	    * z-component, they cannot be additive inverses (which
	    * is one case where the cross product would return
	    * zero). They might, however, be identical (which is
	    * the other such case); but that's OK, because in that
	    * case currangle will be zero (modulo rounding errors)
	    * and so we can set curraxis to anything we like.
	    */
	   curraxis = vnorm(vcross(vbefore, vafter));

	   /*
	    * Determine the angle of rotation by dot product, and
	    * divide by the frame time to get the angular speed.
	    */
	   curraspeed = Math.acos(vdot(vbefore, vafter)) / interval;

	   /*
	    * If either axis or angular speed went to zero,
	    * normalise both.
	    */
	   if (curraspeed == 0 ||
	       (curraxis.x == 0 && curraxis.y == 0 && curraxis.z == 0)) {
	       curraxis = { x:1,y:0,z:0 };
	       curraspeed = 0;
	   }
       }
       lastmousex = mousex;
       lastmousey = mousey;
       lastmousestate = mousestate;

       /*
	* Update the main rotation matrix based on the current
	* angular velocity.
	*/
       var angle = curraspeed * interval;
       var vx = vrotate({x:1,y:0,z:0}, curraxis, angle);
       var vy = vrotate({x:0,y:1,z:0}, curraxis, angle);
       var vz = vrotate({x:0,y:0,z:1}, curraxis, angle);
       var newmatrix = {
	   xx:vx.x, xy:vy.x, xz:vz.x,
	   yx:vx.y, yy:vy.y, yz:vz.y,
	   zx:vx.z, zy:vy.z, zz:vz.z
       };
       currmatrix = matmul(newmatrix, currmatrix);
       var tmpmatrix = currmatrix;
       currmatrix = morthog(currmatrix);
       var tmpdiff =
	   Math.abs(tmpmatrix.xx - currmatrix.xx) +
	   Math.abs(tmpmatrix.xy - currmatrix.xy) +
	   Math.abs(tmpmatrix.xz - currmatrix.xz) +
	   Math.abs(tmpmatrix.yx - currmatrix.yx) +
	   Math.abs(tmpmatrix.yy - currmatrix.yy) +
	   Math.abs(tmpmatrix.yz - currmatrix.yz) +
	   Math.abs(tmpmatrix.zx - currmatrix.zx) +
	   Math.abs(tmpmatrix.zy - currmatrix.zy) +
	   Math.abs(tmpmatrix.zz - currmatrix.zz);
       if (tmpdiff > 0.1 && !tmpdone) {
	   _root.hello.text = tmpdiff + " " +
	       currmatrix.xx.toString() + " " +
	       currmatrix.xy.toString() + " " +
	       currmatrix.xz.toString() + " " +
	       currmatrix.yx.toString() + " " +
	       currmatrix.yy.toString() + " " +
	       currmatrix.yz.toString() + " " +
	       currmatrix.zx.toString() + " " +
	       currmatrix.zy.toString() + " " +
	       currmatrix.zz.toString();
	   tmpdone = true;
       }

       /* ------------------------------------------------------------
	* Draw the polyhedron.
	*/

       /*
	* We begin by transforming every point's coordinates via
	* the current transformation matrix, and then applying
	* perspective and transforming into the Flash movie
	* coordinate space. We assign each point two additional
	* fields dx and dy, giving their display coordinates.
	*/
       for (i = 0; i < poly.points.length; i++) {
	   /*
	    * Transform via currmatrix.
	    */
	   var newpt = mattrans(currmatrix, poly.points[i]);

	   /*
	    * Perspective transform.
	    *
	    * FIXME: we'll need flashpoly.py to scale the whole
	    * polyhedron to bound its radius by 1.
	    */
	   var px = newpt.x / (pdistance + newpt.z);
	   var py = newpt.y / (pdistance + newpt.z);

	   /*
	    * And transform into movie space.
	    * 
	    * We choose our scaling factor as follows: if our
	    * entire polyhedron is contained within a unit sphere
	    * about the origin, and we compute perspective
	    * coordinates as above (by adding pdistance to the
	    * z-coordinate before dividing by it), then no
	    * perspective coordinate can exceed asin(1/pdistance)
	    * in any direction. We therefore scale that in turn to
	    * a respectable 120 out of our available radius of
	    * 150.
	    */
	   poly.points[i].dx = 150 + 120 / Math.asin(1/pdistance) * px;
	   poly.points[i].dy = 150 + 120 / Math.asin(1/pdistance) * py;
       }

       /*
	* Now we go through the faces, transforming their normal
	* vectors and working out which are backward-facing and
	* which forward. We assign each face an additional boolean
	* field `phase', which is 0 for backward faces and 1 for
	* forward faces.
	*/
       for (i = 0; i < poly.faces.length; i++) {
	   /*
	    * Transform normal vector.
	    */
	   var newnorm = mattrans(currmatrix, poly.faces[i]);

	   /*
	    * And set up phase based on whether the z component of
	    * that vector is positive or negative.
	    */
	   poly.faces[i].phase = (newnorm.z > 0 ? 0 : 1);
       }

       /*
	* Now we're ready to actually render the faces.
	*/
       for (var phase = 0; phase < 2; phase++) {
	   if (phase == 0) {
	       _root.lineStyle(1, 0x808080, 100, true, "normal", "round", "round");
	   } else if (phase == 1) {
	       _root.lineStyle(2, 0x000000, 100, true, "normal", "round", "round");
	   }
	   for (i = 0; i < poly.faces.length; i++) {
	       if (poly.faces[i].phase == phase) {
		   var lastpt =
		       poly.faces[i].points[poly.faces[i].points.length-1];
		   _root.moveTo(lastpt.dx, lastpt.dy);
		   for (j = 0; j < poly.faces[i].points.length; j++)
		       _root.lineTo(poly.faces[i].points[j].dx,
				    poly.faces[i].points[j].dy);
	       }
	   }
       }
   }

   static function main(mc) {

       /*
	* The polyhedron description.
	*/
       // --- BEGIN POLYHEDRON ---
       /*
	* This static polyhedron description is here for
	* standalone compilation and testing. In normal use,
	* flashpoly.py will read this template file and replace
	* the polyhedron description here with one derived from
	* the input polyhedron file.
	*/
       var A = { x:-0.904534033733291, y:-0.301511344577764, z:-0.301511344577764 };
       var B = { x:-0.904534033733291, y:0.301511344577764, z:0.301511344577764 };
       var C = { x:-0.301511344577764, y:0.904534033733291, z:0.301511344577764 };
       var D = { x:0.301511344577764, y:0.904534033733291, z:-0.301511344577764 };
       var E = { x:-0.301511344577764, y:0.301511344577764, z:0.904534033733291 };
       var F = { x:0.301511344577764, y:-0.301511344577764, z:0.904534033733291 };
       var G = { x:-0.301511344577764, y:-0.301511344577764, z:-0.904534033733291 };
       var H = { x:0.301511344577764, y:0.301511344577764, z:-0.904534033733291 };
       var I = { x:0.904534033733291, y:0.301511344577764, z:-0.301511344577764 };
       var J = { x:0.904534033733291, y:-0.301511344577764, z:0.301511344577764 };
       var K = { x:-0.301511344577764, y:-0.904534033733291, z:-0.301511344577764 };
       var L = { x:0.301511344577764, y:-0.904534033733291, z:0.301511344577764 };
       var HGABCD = { points: new Array(H,G,A,B,C,D), x:-0.57735026918962573, y:0.57735026918962573, z:-0.57735026918962573 };
       var EFJIDC = { points: new Array(E,F,J,I,D,C), x:0.57735026918962573, y:0.57735026918962573, z:0.57735026918962573 };
       var GHIJLK = { points: new Array(G,H,I,J,L,K), x:0.57735026918962573, y:-0.57735026918962573, z:-0.57735026918962573 };
       var FEBAKL = { points: new Array(F,E,B,A,K,L), x:-0.57735026918962573, y:-0.57735026918962573, z:0.57735026918962573 };
       var AGK = { points: new Array(A,G,K), x:-0.57735026918962584, y:-0.57735026918962584, z:-0.57735026918962584 };
       var BEC = { points: new Array(B,E,C), x:-0.57735026918962584, y:0.57735026918962584, z:0.57735026918962584 };
       var DIH = { points: new Array(D,I,H), x:0.57735026918962584, y:0.57735026918962584, z:-0.57735026918962584 };
       var FLJ = { points: new Array(F,L,J), x:0.57735026918962584, y:-0.57735026918962584, z:0.57735026918962584 };
       poly = { points: new Array(A,B,C,D,E,F,G,H,I,J,K,L), faces: new Array(HGABCD, EFJIDC, GHIJLK, FEBAKL, AGK, BEC, DIH, FLJ) };
       // --- END POLYHEDRON ---

       /*
	* Initialise the dynamic fields.
	*/
       currmatrix = { xx:1,xy:0,xz:0,yx:0,yy:1,yz:0,zx:0,zy:0,zz:1 };
       curraxis = { x:1,y:0,z:0 }; // matters very little since aspeed=0
       curraspeed = 0;
       lastframetime = getTimer();
       mousestate = lastmousestate = false;
       lastmousex = lastmousey = 0;

       tmpdone = false;

       /*
	* Arrange to be called at mouse-down, mouse-up and
	* new-frame time.
	*/
       mc.onEnterFrame = newframe;
       mc.onMouseDown = mousedown;
       mc.onMouseUp = mouseup;
   }
}
