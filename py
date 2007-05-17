#!/bin/sh 

# Convenient means of invoking Python with several handy modules
# preloaded.
python -i -c 'import mathlib
from mathlib import *
import rational
from rational import *
import math
from math import *' "$@"
