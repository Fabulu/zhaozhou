#!/usr/bin/env python3
"""Iteration 9: separate the tail descent from the dive; level the pad at a
declared bite; print real coordinates for the winner."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot, chain, R

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

# C32: C30 with a steep post-pin drop that passes clear below the dive leg,
# then a level grounded pad backward to the biting tip.
C32 = [-18, -6, 8, 26, 45, 78, 112, 133, 168, 200,
       255, 310, 345, 362, 415, 435, 374, 369, 366]

# C33: pad angled a touch deeper at the very tip (tip is the deepest bite).
C33 = [-18, -6, 8, 26, 45, 78, 112, 133, 168, 200,
       255, 310, 345, 362, 415, 435, 372, 368, 372]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter9.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C32 clear descent + pad", C32, 315),
    ("C33 tip-deep pad", C33, 315),
], out, title="Direction 22 collapsed coil - iteration 9 (sketch side)")

pts = chain(C33, 315)
print("C33 chain (pin y14=315):")
for b, (x, y) in enumerate(pts):
    print(f"  {b:2d}  {x:7.0f}  {y:7.0f}  r={R[b]}")
