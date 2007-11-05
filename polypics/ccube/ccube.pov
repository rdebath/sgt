#include "colors.inc"
#include "stones.inc"

background { color Gray }

camera {
  location <-5, +3, -10>
  up <0,0.37,0>
  right <0,0,-0.37>
  sky <0,1,0>
  look_at <0, 0, 0>
}

#include "ccubefrag.pov"

plane {
  y, -1
  pigment { color White }
}

light_source {
  <-5, +10, -15> color <1,1,1>
  area_light
  <1,0,0>, <0,0,1>, 50, 50
}

light_source {
  <0, -0.5, -15> color <0.3,0.3,0.3>
}
