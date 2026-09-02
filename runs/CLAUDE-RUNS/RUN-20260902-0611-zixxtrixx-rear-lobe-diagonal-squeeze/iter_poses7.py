#!/usr/bin/env python3
"""Iteration 7: the SELF-CLOSING foot loop.
Front hangs and descends at idle-or-opener neck curvature; dive sweeps
mildly forward; short grounded landing; CCW loop up-over-back; the tail
descends the loop's rear-inside and the thin tip nestles into the crease
behind the foot, biting the ground UNDER the coil. No centreline crossing;
contacts are tangent stacks the flatten deform owns."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

C27 = [-20, -8, 6, 24, 50, 78, 108, 130, 152, 170,
       205, 255, 305, 345, 380, 415, 445, 462, 475]

# C28: tighter loop (bigger deltas through the wind), shorter ground run.
C28 = [-20, -8, 6, 24, 50, 78, 108, 130, 155, 178,
       225, 285, 340, 372, 398, 425, 450, 466, 478]

# C29: like C28, crest pressed harder (steeper entry, less hang).
C29 = [-14, -2, 12, 34, 58, 80, 112, 133, 158, 180,
       228, 288, 342, 374, 400, 426, 452, 468, 480]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter7.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C27 closing loop", C27, 420),
    ("C28 tight closing loop", C28, 370),
    ("C29 pressed crest", C29, 370),
], out, title="Direction 22 collapsed coil - iteration 7 (sketch side)")
