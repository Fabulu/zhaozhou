#!/usr/bin/env python3
"""Iteration 2: press the crest under ~600, land the tip at ~-20, tilt the
stack down-and-back, tuck the head low-front."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

# C3: head low-front, neck rises gently backward to a crest ~600, signature
# fold (5-7 at idle cap) drops onto a forward middle run that climbs slightly
# (tilted layers = the diagonal), sharp fold B, bottom run descending backward
# into the tail taper that plants the tip.
C3 = [-15, -22, -18, -4, 38, 78, 114, 137, 168, 184,
      188, 110, 32, 8, 12, 10, 14, 22, 34]

# C4: like C3 but the middle run doubles back further (head pulled deeper
# back) and the fold B lands later; tail taper slightly steeper at the end.
C4 = [-12, -25, -22, -6, 36, 78, 114, 137, 166, 182,
      190, 194, 108, 26, 6, 10, 16, 26, 40]

# C5: C3 with a raised rear: bottom run tilted harder so the rear-bottom is
# clearly the lowest, tail plants right under the coil's rear edge.
C5 = [-15, -22, -18, -4, 38, 78, 114, 137, 168, 186,
      190, 112, 34, 12, 16, 18, 24, 32, 42]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter2.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C3", C3, 105),
    ("C4 deeper doubleback", C4, 118),
    ("C5 rear-tilted", C5, 128),
], out, title="Direction 22 collapsed coil - iteration 2 (sketch side)")
