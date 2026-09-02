#!/usr/bin/env python3
"""Iteration 10: the INWARD SPIRAL with a deep nose-hang.
Head hangs forward-down off a narrow crest; the dive (cap-honest) winds on
through the foot, climbs the front, arches over and spirals IN, the thin
tail curling to the biting tip beneath the coil. Two nested bows: the S
wound tighter, planar, no crossings."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot, chain, R

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

CF1 = [-52, -26, -6, 12, 36, 78, 110, 131, 165, 210,
       258, 285, 235, 185, 142, 105, 62, 22, 8]

# CF2: arch a touch lower, pad longer/flatter, foot shallower.
CF2 = [-52, -26, -6, 12, 36, 78, 110, 131, 168, 215,
       262, 288, 240, 190, 148, 112, 70, 28, 10]

# CF3: hang shallower (nose higher but face aim easier), rest as CF2.
CF3 = [-40, -18, -2, 14, 38, 78, 110, 131, 168, 215,
       262, 288, 240, 190, 148, 112, 70, 28, 10]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter10.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("CF1 inward spiral", CF1, 260),
    ("CF2 lower arch", CF2, 260),
    ("CF3 shallow hang", CF3, 260),
], out, title="Direction 22 collapsed coil - iteration 10 (sketch side)")

for nm, t, pin in [("CF2", CF2, 260)]:
    pts = chain(t, pin)
    print(f"{nm} chain (pin y14={pin}):")
    for b, (x, y) in enumerate(pts):
        print(f"  {b:2d}  {x:7.0f}  {y:7.0f}  r={R[b]}")
