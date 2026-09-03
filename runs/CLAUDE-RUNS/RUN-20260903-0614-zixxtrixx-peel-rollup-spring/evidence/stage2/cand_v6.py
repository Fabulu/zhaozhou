# v6: the chosen family. stand = v5 "stand C"; collapsed = v5 "coll B"
# retuned to the stand-C loop phase; NEW: peel-mid (arm 220) -- the freed
# front of the grounded stretch curling up while the tail still rests.
# Each entry: (name, headings_deg, plant) where plant = (station, x, y).
STAND = [2.0, 6.0, 12.0, 24.0, 40.0,
         80.0, 115.0, 150.0,
         198.0, 246.0, 294.0, 342.0, 30.0, 45.0, 50.0,
         42.0, 52.0, 62.0, 70.0]
COLLAPSED = [-25.0, -5.0, 30.0, 75.0, 120.0,
             150.0, 168.0, 178.0,
             188.0, 210.0, 270.0, 330.0, 352.0, 15.0, 45.0,
             42.0, 52.0, 62.0, 70.0]
PEELMID = [3.0, 6.5, 12.5, 24.0, 39.0,          # front rides
           81.0, 118.0, 142.0,                  # dive barely deepens
           130.0, 110.0,                        # dive bottom starting to roll
           -40.0, -30.0, -5.0, 15.0, 28.0,      # freed stretch arcs UP
           25.0, 8.2, -30.8, -62.6]             # 16-18 still resting (stance)
CANDIDATES = [("peel-mid v6", PEELMID, (15.5, -1477, 113)),
              ("tail-stand v6", STAND, (19, -1925, 25)),
              ("collapsed v6", COLLAPSED, (19, -1925, 25))]
