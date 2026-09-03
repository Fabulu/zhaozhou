# v3: keep the signature S front (0-7); the former grounded run + taper
# (8-14) rolls into ONE readable loop; short steep tail column (15-18) into
# the planted tip. Collapsed: column frozen, loop pressed down, head driven
# back-and-down into the fold. Degrees, head->tail.
STAND_V3 = [4.9, 7.0, 13.1, 23.2, 37.4,            # stance crown: S identity
            80.0, 118.0, 138.0,                    # stance dive
            178.0, 218.0, 258.0, 298.0, 338.0, 18.0, 58.0,  # the loop (40/seg)
            50.0, 58.0, 66.0, 72.0]                # tail column into the tip
COLLAPSED_V3 = [-12.0, 0.0, 22.0, 55.0, 90.0,      # crown folds down-back
                125.0, 155.0, 175.0,               # dive pressed past vertical
                205.0, 240.0, 272.0, 305.0, 340.0, 15.0, 55.0,  # loop tightened low
                50.0, 58.0, 66.0, 72.0]            # column FROZEN
CANDIDATES = [("tail-stand v3", STAND_V3), ("collapsed v3", COLLAPSED_V3)]
