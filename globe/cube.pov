#include "colors.inc"
#include "stones.inc"

background { color Gray }

camera {
  location <0, 0, -10>
  up <0,0.35,0>
  right <0.35,0,0>
  look_at <0, 0, 0>
}

#declare A = <0.0, 0.577350269187, 1.63299316185>;
#declare B = <0.0, 1.73205080757, 0>;
#declare C = <1.41421356237, -0.57735026919, 0.816496580928>;
#declare D = <1.41421356237, 0.57735026919, -0.816496580928>;
#declare E = <-1.41421356237, -0.57735026919, 0.816496580928>;
#declare F = <-1.41421356237, 0.57735026919, -0.816496580928>;
#declare G = <0.0, -1.73205080757, 0>;
#declare H = <0.0, -0.577350269187, -1.63299316185>;
// Optional orientation diagnostics.
//sphere { A, 0.1 texture { pigment { color Black } } }
//sphere { B, 0.1 texture { pigment { color Red } } }
//sphere { C, 0.1 texture { pigment { color Green } } }
//sphere { D, 0.1 texture { pigment { color Yellow } } }
//sphere { E, 0.1 texture { pigment { color Blue } } }
//sphere { F, 0.1 texture { pigment { color Magenta } } }
//sphere { G, 0.1 texture { pigment { color Cyan } } }
//sphere { H, 0.1 texture { pigment { color White } } }

union {

  polygon {
    5, <0,0>, <0,1>, <1,1>, <1,0>, <0,0>
    pigment { image_map { png "cube3.png" } }
    matrix < vdot(D-B, <1,0,0>), vdot(D-B, <0,1,0>), vdot(D-B, <0,0,1>),
             vdot(A-B, <1,0,0>), vdot(A-B, <0,1,0>), vdot(A-B, <0,0,1>),
	     1.2, 1.3, 1.4,
             vdot(B, <1,0,0>), vdot(B, <0,1,0>), vdot(B, <0,0,1>) >
  }

  polygon {
    5, <0,0>, <0,1>, <1,1>, <1,0>, <0,0>
    pigment { image_map { png "cube4.png" } }
    matrix < vdot(H-F, <1,0,0>), vdot(H-F, <0,1,0>), vdot(H-F, <0,0,1>),
             vdot(E-F, <1,0,0>), vdot(E-F, <0,1,0>), vdot(E-F, <0,0,1>),
	     1.2, 1.3, 1.4,
             vdot(F, <1,0,0>), vdot(F, <0,1,0>), vdot(F, <0,0,1>) >
  }

  polygon {
    5, <0,0>, <0,1>, <1,1>, <1,0>, <0,0>
    pigment { image_map { png "cube2.png" } }
    matrix < vdot(C-A, <1,0,0>), vdot(C-A, <0,1,0>), vdot(C-A, <0,0,1>),
             vdot(E-A, <1,0,0>), vdot(E-A, <0,1,0>), vdot(E-A, <0,0,1>),
	     1.2, 1.3, 1.4,
             vdot(A, <1,0,0>), vdot(A, <0,1,0>), vdot(A, <0,0,1>) >
  }

  polygon {
    5, <0,0>, <0,1>, <1,1>, <1,0>, <0,0>
    pigment { image_map { png "cube1.png" } }
    matrix < vdot(D-B, <1,0,0>), vdot(D-B, <0,1,0>), vdot(D-B, <0,0,1>),
             vdot(F-B, <1,0,0>), vdot(F-B, <0,1,0>), vdot(F-B, <0,0,1>),
	     1.2, 1.3, 1.4,
             vdot(B, <1,0,0>), vdot(B, <0,1,0>), vdot(B, <0,0,1>) >
  }

  polygon {
    5, <0,0>, <0,1>, <1,1>, <1,0>, <0,0>
    pigment { image_map { png "cube6.png" } }
    matrix < vdot(B-F, <1,0,0>), vdot(B-F, <0,1,0>), vdot(B-F, <0,0,1>),
             vdot(E-F, <1,0,0>), vdot(E-F, <0,1,0>), vdot(E-F, <0,0,1>),
	     1.2, 1.3, 1.4,
             vdot(F, <1,0,0>), vdot(F, <0,1,0>), vdot(F, <0,0,1>) >
  }

  polygon {
    5, <0,0>, <0,1>, <1,1>, <1,0>, <0,0>
    pigment { image_map { png "cube5.png" } }
    matrix < vdot(D-H, <1,0,0>), vdot(D-H, <0,1,0>), vdot(D-H, <0,0,1>),
             vdot(G-H, <1,0,0>), vdot(G-H, <0,1,0>), vdot(G-H, <0,0,1>),
             1.2, 1.3, 1.4,
             vdot(H, <1,0,0>), vdot(H, <0,1,0>), vdot(H, <0,0,1>) >
  }

  rotate y*(-clock)
}

light_source {
  <2, 4, -3> color <0.7,0.7,0.7>
}

light_source {
  <-4, 1, -10> color <0.5,0.5,0.5>
}

light_source {
  <4, 1, -10> color <0.5,0.5,0.5>
}
