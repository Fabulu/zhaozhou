#!/usr/bin/env python3
"""Iteration 8: C25 proportions + the FOOT PAD tail.
After the loop, the tail drops steeply and then flattens into a grounded
pad running backward, tip the rearmost biting point: the stand."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

C30 = [-18, -6, 8, 26, 45, 78, 112, 133, 168, 200,
       255, 310, 345, 362, 400, 425, 385, 372, 368]

C31 = [-24, -10, 6, 24, 45, 78, 112, 133, 168, 205,
       262, 318, 350, 366, 402, 428, 388, 374, 368]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter8.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C30 loop + foot pad", C30, 320),
    ("C30 pin 290", C30, 290),
    ("C31 deeper hang", C31, 300),
], out, title="Direction 22 collapsed coil - iteration 8 (sketch side)")
