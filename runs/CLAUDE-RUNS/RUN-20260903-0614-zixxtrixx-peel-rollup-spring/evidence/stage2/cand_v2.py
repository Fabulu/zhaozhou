# v2: the stand is the aerial S carried back onto the tail; compression folds
# the standing S down into itself, head back and down. Degrees, head->tail.
STAND_V2 = [4.9, 7.0, 13.1, 23.2, 37.4,          # head crown ~stance
            80.2, 118.0, 138.0, 105.0, 70.0,     # the dive keeps its hook
            60.0, 52.0, 50.0, 47.0, 45.0,        # former grounded run = rising limb
            45.0, 55.0, 65.0, 72.0]              # tail column into the tip
COLLAPSED_V2 = [-15.0, 0.0, 25.0, 55.0, 85.0,    # head folds down-back into pocket
                110.0, 140.0, 160.0, 120.0, 75.0,# dive fold closes
                55.0, 42.0, 36.0, 40.0, 42.0,    # limb shallows: mass drops
                45.0, 55.0, 65.0, 72.0]          # tail column FROZEN = stand
CANDIDATES = [("tail-stand v2", STAND_V2), ("collapsed v2", COLLAPSED_V2)]
