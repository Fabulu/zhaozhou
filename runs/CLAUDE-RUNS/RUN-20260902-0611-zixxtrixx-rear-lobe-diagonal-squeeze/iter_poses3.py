#!/usr/bin/env python3
"""Iteration 3: two families.
C8 = pressed double-lobe (open window) with rear lobe + planted tail strut.
C9 = the hook wound SHUT into a pressed ring (the coil at its extreme),
     exiting backward mid-height, rear arc descending to the planted tip.
Neck segments 5-7 never exceed idle (80.2/117.6/138.5)."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

C8 = [-10, -4, 4, 14, 40, 78, 116, 137, 170, 95,
      30, 8, 5, 4, -14, -45, 20, 38, 52]

C9 = [-8, 0, 10, 40, 62, 78, 116, 137, 170, 262,
      335, 358, 368, 372, 382, 395, 405, 408, 412]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter3.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C8 double-lobe + tail strut", C8, 92),
    ("C9 wound-shut ring coil", C9, 92),
], out, title="Direction 22 collapsed coil - iteration 3 (sketch side)")
