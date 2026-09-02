#!/usr/bin/env python3
"""Iteration 11: CF2 x-topology with the y-budget fixed.
Hang, crest, cap-value dive, biting foot, loop, arch exit, forward descent,
curl-back, grounded pad running under the loop, tip biting under the
loop's rear. Tuning targets: tip under ~-25, pad under ~-15, foot bite
~-30, loop bottom resting on the pad, top <= ~700, no non-tangent overlap."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot, chain, R

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

# CF4: descent shallower than CF2 so the pad lands at ground, not below.
CF4 = [-45, -22, -4, 14, 30, 78, 110, 131, 174, 215,
       262, 288, 240, 190, 148, 128, 55, 20, 6]

# CF5: cleaner curl-back: descent 2 segs, curl 1, pad 2.
CF5 = [-40, -18, -2, 16, 32, 78, 110, 131, 174, 215,
       262, 290, 242, 192, 150, 126, 50, 18, 5]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter11.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("CF4", CF4, 360),
    ("CF5", CF5, 360),
], out, title="Direction 22 collapsed coil - iteration 11 (sketch side)")

for nm, t, pin in [("CF4", CF4, 360), ("CF5", CF5, 360)]:
    pts = chain(t, pin)
    print(f"{nm} chain (pin y14={pin}):")
    for b, (x, y) in enumerate(pts):
        print(f"  {b:2d}  {x:7.0f}  {y:7.0f}  r={R[b]}")
