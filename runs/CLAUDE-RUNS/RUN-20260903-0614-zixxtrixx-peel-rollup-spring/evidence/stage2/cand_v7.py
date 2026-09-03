# v7: C-loop (~225 deg sweep, gap on the left flank) so the dive enters above
# the gap and the column exits below it -- NO chain crossing at the stand.
# Peel-mid lifted properly. Collapsed keeps the C but pressed low+flat, head
# far back over it (the one declared press region). Deg, head->tail.
STAND = [2.0, 6.0, 12.0, 24.0, 40.0,
         85.0, 125.0, 158.0,
         185.0, 230.0, 275.0, 320.0, 355.0, 25.0, 40.0,
         42.0, 52.0, 62.0, 70.0]
PEELMID = [3.0, 6.5, 12.5, 24.0, 39.0,
           81.0, 118.0, 138.0,
           120.0, 85.0,
           -70.0, -25.0, 20.0, 45.0, 55.0,
           20.0, 8.2, -30.8, -62.6]
COLLAPSED = [-25.0, -5.0, 30.0, 75.0, 120.0,
             152.0, 170.0, 180.0,
             195.0, 235.0, 285.0, 330.0, 355.0, 18.0, 38.0,
             42.0, 52.0, 62.0, 70.0]
CANDIDATES = [("peel-mid v7", PEELMID, (15.5, -1477, 113)),
              ("tail-stand v7", STAND, (19, -1925, 25)),
              ("collapsed v7", COLLAPSED, (19, -1925, 25))]
