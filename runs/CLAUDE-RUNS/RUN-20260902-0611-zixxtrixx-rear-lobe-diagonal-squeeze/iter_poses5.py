#!/usr/bin/env python3
"""Iteration 5: the four-layer press.
Nose hangs low-front; crest ~570; STEEP dive at (not past) the cap values,
with the neck OPENING vs idle; turn forward; doubled bottom-run (head-back);
sharp fold; return run crawls UNDER the dive at ground level; tail tapers
back to the biting tip = the rear base."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coil_designer import plot

SHIPPED = [-8.2, 2.2, 4.9, 6.0, 7.7, 93.4, 151.1, 170.3, 90.7, 24.7,
           4.9, 4.9, 5.5, 6.6, 9.3, -15.4, -34.1, -26.4, -8.8]

C19 = [-22, -10, 6, 22, 45, 78, 95, 116, 150, 172,
       178, 95, 14, 2, 2, 4, 6, 10, 18]

# C20: doubled run one seg longer (head further back), dive one seg shorter.
C20 = [-25, -12, 4, 20, 45, 78, 96, 118, 155, 172,
       178, 179, 95, 12, 2, 4, 6, 10, 18]

# C21: like C19 but tail arcs with more visible curl to the tip.
C21 = [-22, -10, 6, 22, 45, 78, 95, 116, 150, 172,
       178, 95, 14, 2, 2, 3, 8, 16, 30]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "evidence", "coil-iter5.png")
plot([
    ("shipped collapsed (fault)", SHIPPED, 84),
    ("C19 four-layer press", C19, 60),
    ("C20 longer doubleback", C20, 60),
    ("C21 tail arc", C21, 60),
], out, title="Direction 22 collapsed coil - iteration 5 (sketch side)")
