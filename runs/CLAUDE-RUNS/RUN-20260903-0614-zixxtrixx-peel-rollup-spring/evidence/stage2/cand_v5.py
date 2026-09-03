# v5 sweep: stand variants fixing the crown/loop-closure crowding; collapsed
# variants aiming the nose forward-LOW out of a pressed coil. Deg, head->tail.
S_A = [4.9, 7.0, 13.1, 23.2, 37.4,      # crown at stance
       95.0, 135.0, 165.0,
       200.0, 240.0, 280.0, 318.0, 352.0, 25.0, 52.0,
       42.0, 52.0, 62.0, 70.0]
# S_B: loop phase rotated later (entry flatter) + crown lifted a touch so the
# closure passes lower under it
S_B = [0.0, 5.0, 12.0, 22.0, 36.0,
       90.0, 130.0, 162.0,
       192.0, 228.0, 266.0, 304.0, 342.0, 20.0, 50.0,
       40.0, 50.0, 60.0, 70.0]
# S_C: smaller loop (48/seg) leaving a longer straighter neck into the crown
S_C = [2.0, 6.0, 12.0, 24.0, 40.0,
       80.0, 115.0, 150.0,
       198.0, 246.0, 294.0, 342.0, 30.0, 45.0, 50.0,
       42.0, 52.0, 62.0, 70.0]
# collapsed variants (column frozen at the S_A/B family 42/52/62/70):
# C_A: ellipse low, dive pressed flat on it, crown folds forward-down
C_A = [-5.0, 10.0, 40.0, 80.0, 115.0,
       140.0, 160.0, 172.0,
       185.0, 205.0, 265.0, 325.0, 350.0, 12.0, 45.0,
       42.0, 52.0, 62.0, 70.0]
# C_B: like C_A but crown wraps harder so the nose drops in FRONT (right)
C_B = [-25.0, -5.0, 30.0, 75.0, 120.0,
       150.0, 168.0, 178.0,
       188.0, 210.0, 270.0, 330.0, 352.0, 15.0, 45.0,
       42.0, 52.0, 62.0, 70.0]
# C_C: whole assembly pressed lower: loop bottom sags, column meets it low
C_C = [-15.0, 0.0, 35.0, 80.0, 125.0,
       155.0, 172.0, 180.0,
       190.0, 215.0, 275.0, 335.0, 355.0, 18.0, 48.0,
       42.0, 52.0, 62.0, 70.0]
CANDIDATES = [("stand A", S_A), ("stand B", S_B), ("stand C", S_C),
              ("coll A", C_A), ("coll B", C_B), ("coll C", C_C)]
