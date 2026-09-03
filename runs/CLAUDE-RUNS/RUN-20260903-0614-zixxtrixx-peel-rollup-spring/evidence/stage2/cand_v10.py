# v10: steeper column + tighter loop so the STAND shows daylight under the
# coil; collapsed drops the whole mass toward the tail base and flattens
# visibly (strong compression). Deg, head->tail.
STAND = [2.0, 6.0, 12.0, 24.0, 40.0,
         92.0, 132.0, 165.0,
         192.0, 240.0, 288.0, 336.0, 20.0, 45.0, 50.0,
         50.0, 62.0, 72.0, 78.0]
COLLAPSED = [-35.0, -12.0, 32.0, 85.0, 135.0,
             168.0, 182.0, 190.0,
             203.0, 245.0, 300.0, 345.0, 8.0, 30.0, 45.0,
             50.0, 62.0, 72.0, 78.0]
PEELMID = [3.0, 6.5, 12.5, 24.0, 39.0,
           82.0, 119.0, 142.0,
           120.0, 100.0,
           -35.0, -30.0, 10.0, 35.0, 6.4,
           9.3, 8.2, -30.8, -62.6]
CANDIDATES = [("stand v10", STAND, (19, -1925, 25)),
              ("collapsed v10", COLLAPSED, (19, -1925, 25)),
              ("stand v9 (was)", [2.0, 6.0, 12.0, 24.0, 40.0, 92.0, 132.0, 162.0,
                190.0, 235.0, 280.0, 322.0, 355.0, 22.0, 38.0,
                36.0, 47.0, 58.0, 68.0], (19, -1925, 25))]
