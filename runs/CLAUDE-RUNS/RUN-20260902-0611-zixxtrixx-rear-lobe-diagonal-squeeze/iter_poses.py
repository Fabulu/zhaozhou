#!/usr/bin/env python3
"""Iteration sketches for the Direction-22 collapsed coil. Run, look, adjust."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot

IDLE = [5, 6.9, 13, 23.2, 37.4, 80.2, 117.6, 138.5, 109.9, 63.7,
        0, 1.2, 1.0, 1.7, 6.4, 9.3, 8.2, -30.8, -62.6]
SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

# C1: coil kept in the idle family -- low head reaching forward under the
# crest, big signature fold behind/above it, middle run, sharp second fold,
# bottom run, rear arc down to the planted tail tube tip.
C1 = [-18, -32, -25, -8, 30, 78, 114, 137, 158, 170,
      188, 100, 30, 10, 5, 18, 38, 58, 72]

# C2: same family, deeper doubling of the middle run (head further back),
# rear lobe rises slightly before the strut.
C2 = [-15, -35, -30, -5, 32, 78, 114, 137, 162, 176,
      192, 196, 95, 20, 5, -15, 35, 60, 75]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter1.png")
plot([
    ("idle (reference)", IDLE, 144),
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C1 coil-on-tail", C1, 120),
    ("C2 deeper doubleback", C2, 130),
], out, title="Direction 22 collapsed coil - iteration 1 (sketch side)")
