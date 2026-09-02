#!/usr/bin/env python3
"""Iteration 4: the S migrates REARWARD. Front opens (neck curls LESS than
idle - the head lowers and travels back as the crest moves to the mid-body),
the rear owns the winding (uncapped, thin tube), the tail plants the tip as
the lowest, rearmost point with the rear mass arched above it."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

# C11: nose low-front aiming up-forward; neck rises to a mid crest; open
# descent (5-7 well under the idle cap); belly run; rear arch with the
# support on its flank; tail descends to the biting tip at the very rear.
C11 = [-18, -20, -16, -8, 5, 25, 45, 62, 75, 40,
       12, 8, -15, -32, 12, 30, 52, 70, 40]

# C12: same, arch higher + tail tucks under harder (J).
C12 = [-16, -22, -18, -8, 6, 28, 48, 66, 78, 42,
       10, -5, -25, -40, 8, 26, 55, 85, 115]

# C13: crest further back (head travels more), gentler arch.
C13 = [-14, -18, -20, -14, -4, 18, 40, 60, 76, 55,
       25, 8, -12, -28, 5, 22, 45, 65, 45]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter4.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C11 rear-arch", C11, 205),
    ("C12 high arch + J tuck", C12, 235),
    ("C13 crest-back", C13, 195),
], out, title="Direction 22 collapsed coil - iteration 4 (sketch side)")
