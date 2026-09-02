#!/usr/bin/env python3
"""Iteration 6: the FOOT-LOOP coil.
Front hangs + descends at (never past) idle neck curvature; the dive sweeps
forward-down; the FOOT CURLS UP-AND-OVER-BACKWARD into a wound rear loop
(uncapped mid-body); the return runs backward above the dive's diagonal;
the rear descends to the planted tail tube tip. Head-back from dive sweep +
span compaction; rear owns the winding; tip is the base."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

C24 = [-18, -6, 8, 26, 45, 78, 112, 133, 168, 205,
       268, 330, 355, 368, 385, 400, 412, 420, 428]

# C25: bigger loop (radius up), return higher, gentler rear descent.
C25 = [-18, -6, 8, 26, 45, 78, 112, 133, 168, 200,
       255, 310, 345, 362, 376, 392, 406, 416, 424]

# C26: loop tighter, rear descent longer and shallower.
C26 = [-16, -4, 10, 28, 48, 78, 112, 133, 172, 215,
       280, 338, 358, 370, 382, 394, 404, 412, 420]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter6.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C24 foot-loop", C24, 330),
    ("C25 wide loop", C25, 300),
    ("C26 tight loop", C26, 340),
], out, title="Direction 22 collapsed coil - iteration 6 (sketch side)")
