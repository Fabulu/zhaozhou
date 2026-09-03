# v9: peel-mid = v7 wave with the 9/10 hairpin softened (nose stays well
# ahead of the tail and between stance and stand); stand leaned a touch more
# with the dive steepened so it drops into the loop clear of the column;
# collapsed = v8 (strong). Deg, head->tail.
STAND = [2.0, 6.0, 12.0, 24.0, 40.0,
         92.0, 132.0, 162.0,
         190.0, 235.0, 280.0, 322.0, 355.0, 22.0, 38.0,
         36.0, 47.0, 58.0, 68.0]
PEELMID = [3.0, 6.5, 12.5, 24.0, 39.0,
           82.0, 119.0, 142.0,
           120.0, 100.0,
           -35.0, -30.0, 10.0, 35.0, 45.0,
           18.0, 8.2, -30.8, -62.6]
COLLAPSED = [-30.0, -10.0, 30.0, 80.0, 128.0,
             158.0, 175.0, 185.0,
             200.0, 245.0, 292.0, 335.0, 358.0, 15.0, 33.0,
             36.0, 47.0, 58.0, 68.0]
CANDIDATES = [("peel-mid v9", PEELMID, (15.0, -1398, 126)),
              ("tail-stand v9", STAND, (19, -1925, 25)),
              ("collapsed v9", COLLAPSED, (19, -1925, 25))]
