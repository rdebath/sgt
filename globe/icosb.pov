#include "colors.inc"
#include "stones.inc"

background { color Gray }

camera {
  location <+10, 0, +3>
  up <0,0.45,0>
  right <0,0,-0.45>
  sky <0,0,1>
  look_at <0, 0, 0>
}

#include "icos-bw-vert-frag.pov"

plane {
  z, -3
  pigment { color White }
}

light_source {
  <+10, +10, +20> color <1,1,1>
  area_light
  <1,0,0>, <0,1,0>, 50, 50
}

light_source {
  <+20, +20, -2.5> color <1,1,1>
}
