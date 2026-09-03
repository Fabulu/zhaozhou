# v8: peel-mid as the BREAKING WAVE (ramp off contact, crest curling over,
# long-way roll on stations 10-11); collapsed pressed harder (crown lower,
# nose deeper back). Every per-station spline leg <= 165 deg. Deg, head->tail.
STAND = [2.0, 6.0, 12.0, 24.0, 40.0,
         85.0, 125.0, 158.0,
         185.0, 230.0, 275.0, 320.0, 355.0, 25.0, 40.0,
         42.0, 52.0, 62.0, 70.0]
PEELMID = [3.0, 6.5, 12.5, 24.0, 39.0,
           82.0, 119.0, 142.0,
           160.0, 195.0,
           165.0, 160.0, 105.0, 65.0, 35.0,
           15.0, 8.2, -30.8, -62.6]
COLLAPSED = [-30.0, -10.0, 30.0, 80.0, 128.0,
             158.0, 175.0, 185.0,
             200.0, 245.0, 292.0, 335.0, 358.0, 15.0, 35.0,
             42.0, 52.0, 62.0, 70.0]
CANDIDATES = [("peel-mid v8", PEELMID, (15.0, -1398, 126)),
              ("tail-stand v8", STAND, (19, -1925, 25)),
              ("collapsed v8", COLLAPSED, (19, -1925, 25))]
