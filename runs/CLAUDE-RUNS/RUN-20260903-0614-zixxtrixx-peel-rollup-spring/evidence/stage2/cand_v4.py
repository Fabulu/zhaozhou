# v4: dive steepened so it drops into the loop instead of lying on the tail
# column; column leaned forward ("might be at an angle"); collapsed presses
# the loop into a low ellipse with the head jutting forward-low, nose outside
# the mass. Degrees, head->tail.
STAND_V4 = [4.9, 7.0, 13.1, 23.2, 37.4,           # stance crown
            95.0, 135.0, 165.0,                   # dive steepened, drops in
            195.0, 232.0, 270.0, 308.0, 345.0, 22.0, 55.0,  # the loop
            42.0, 52.0, 62.0, 70.0]               # column, leaned forward
COLLAPSED_V4 = [-10.0, 5.0, 30.0, 70.0, 110.0,    # crown folds hard down
                150.0, 172.0, 180.0,              # dive pressed to horizontal
                188.0, 210.0, 275.0, 330.0, 350.0, 15.0, 50.0, # loop low+flat
                42.0, 52.0, 62.0, 70.0]           # column FROZEN
CANDIDATES = [("tail-stand v4", STAND_V4), ("collapsed v4", COLLAPSED_V4)]
