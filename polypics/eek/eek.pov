#include "colors.inc"
#include "stones.inc"

background { color Gray }

camera {
  location <-5, +3.5, -10>
  up <0,0.51,0>
  right <0,0,-0.68>
  sky <0,1,0>
  look_at <0, 0.5, 0>
}

// Include the polyhedron description, generated for us by povpoly.py.
#include "eek-frag.pov"

// Now for the tricky bit: getting his wings on. We have to do this
// manually.

#declare M = <0.0, 0.324919696233, 1.7013016167>;

polygon {
  // Begin by importing wingicon.png into a unit square. The attached
  // edge of the wing then runs from (0,4/13) to (0,9/13).
  5, <0,0>, <0,1>, <1,1>, <1,0>, <0,0>
  pigment { image_map { png "wingicon.png" transmit 1, 1.0 } }
  // Scale it by 13 so the attached edge runs from (0,4) to (0,9).
  scale <13,13,13>
  // Translate so that the attached edge runs from (0,0) to (0,5).
  translate <0,-4,0>
  // And scale again so that it runs from (0,0) to (0,1).
  scale <0.2,0.2,0.2>
  // Now we need to transform this so that those two points map to
  // the points F and K in the dodecahedron description.
  #declare F = <1.0, 1.37638192047, 0.324919696233>;
  #declare K = <1.61803398875, 0.324919696233, 0.525731112119>;
  // Our y-coordinate is along the FK axis.
  #declare FKy = F-K;
  // Our x-coordinate points directly away from the far corner of
  // the face EKFNM, to which the wing is parallel centre of the
  // dodecahedron, making it perpendicular to the BI axis. So we
  // construct it by taking the average of B and I, subtracting M,
  // and normalising to the same length as our y-vector.
  #declare FKx = vlength(FKy) * vnormalize((F+K)/2-M);
  matrix <
    vdot(FKx, <1,0,0>), vdot(FKx, <0,1,0>), vdot(FKx, <0,0,1>),
    vdot(FKy, <1,0,0>), vdot(FKy, <0,1,0>), vdot(FKy, <0,0,1>),
    0, 0, 1,
    vdot(K, <1,0,0>), vdot(K, <0,1,0>), vdot(K, <0,0,1>)
  >
}

// Now do exactly the same with the other wing.
polygon {
  // Begin by importing wingicon.png into a unit square. The attached
  // edge of the wing then runs from (0,4/13) to (0,9/13).
  5, <0,0>, <0,1>, <1,1>, <1,0>, <0,0>
  pigment { image_map { png "wingicon.png" transmit 1, 1.0 } }
  // Scale it by 13 so the attached edge runs from (0,4) to (0,9).
  scale <13,13,13>
  // Translate so that the attached edge runs from (0,0) to (0,5).
  translate <0,-4,0>
  // And scale again so that it runs from (0,0) to (0,1).
  scale <0.2,0.2,0.2>
  // Now we need to transform this so that those two points map to
  // the points B and I in the dodecahedron description.
  #declare B = <-1.0, 1.37638192047, 0.324919696233>;
  #declare I = <-1.61803398875, 0.324919696233, 0.525731112119>;
  // Our y-coordinate is along the BI axis.
  #declare BIy = B-I;
  // Our x-coordinate points directly away from the far corner of
  // the face MNBIA, to which the wing is parallel centre of the
  // dodecahedron, making it perpendicular to the BI axis. So we
  // construct it by taking the average of B and I, subtracting M,
  // and normalising to the same length as our y-vector.
  #declare BIx = vlength(BIy) * vnormalize((B+I)/2-M);
  matrix <
    vdot(BIx, <1,0,0>), vdot(BIx, <0,1,0>), vdot(BIx, <0,0,1>),
    vdot(BIy, <1,0,0>), vdot(BIy, <0,1,0>), vdot(BIy, <0,0,1>),
    0, 0, 1,
    vdot(I, <1,0,0>), vdot(I, <0,1,0>), vdot(I, <0,0,1>)
  >
}

plane {
  y, -1.37638192047
  pigment { color White }
}

light_source {
  <-5, +20, -5> color <1,1,1>
  area_light
  <1,0,0>, <0,0,1>, 50, 50
}

light_source {
  <0, -1.37, -15> color <1,1,1>
}
