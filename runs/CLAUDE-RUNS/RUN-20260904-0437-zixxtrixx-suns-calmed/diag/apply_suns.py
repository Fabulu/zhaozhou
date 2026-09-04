"""Rewrite the kZixxSun* constant table in zhao_reel.cpp from a value table.

Usage: python apply_suns.py v2   (table name below)
Positions/radii are never touched; only the six gain/add values per clip.
"""
import re
import sys

POS = {
 "Idle":       "15556,  kZixxSunHeightMm, 15556",
 "Walk":       "-12619,  kZixxSunHeightMm, 18022",
 "Run":        "12619,  kZixxSunHeightMm, 18022",
 "Look":       "12619,  kZixxSunHeightMm, 18022",
 "Balance":    "14142,  kZixxSunHeightMm, 16853",
 "Taunt":      "21666,  kZixxSunHeightMm, -3820",
 "SlowTaunt":  "11000,  kZixxSunHeightMm, 19053",
 "JumpOne":    "13544,  kZixxSunHeightMm, 17336",
 "JumpMulti":  "-13544,  kZixxSunHeightMm, 17336",
 "Attack":     "14142,  kZixxSunHeightMm, 16853",
 "Hit":        "3820,  kZixxSunHeightMm, 21666",
 "Damage":     "21666,  kZixxSunHeightMm, 3820",
 "Knockdown":  "4574,  kZixxSunHeightMm, 21518",
 "Fall":       "-14142,  kZixxSunHeightMm, 16853",
 "HitFloor":   "21518,  kZixxSunHeightMm, 4574",
 "SaltoDummy": "11000,  kZixxSunHeightMm, 19053",
 "SaltoFly":   "-11000,  kZixxSunHeightMm, 19053",
 "SaltoSix":   "16853,  kZixxSunHeightMm, 14142",
 "SaltoNine":  "-16853,  kZixxSunHeightMm, 14142",
 "Death":      "21916,  kZixxSunHeightMm, 1917",
 "Death2":     "3062,  kZixxSunHeightMm, 21785",
}

MOOD = {
 "Idle": "golden morning", "Walk": "azure day", "Run": "hot orange",
 "Look": "violet", "Balance": "teal", "Taunt": "magenta",
 "SlowTaunt": "rose dusk", "JumpOne": "spring lime",
 "JumpMulti": "sunset red-orange", "Attack": "crimson", "Hit": "steel blue",
 "Damage": "ember amber", "Knockdown": "bruise violet", "Fall": "ice blue",
 "HitFloor": "dust orange", "SaltoDummy": "chartreuse gold",
 "SaltoFly": "deep azure", "SaltoSix": "gold", "SaltoNine": "hot pink",
 "Death": "deep red", "Death2": "moonlight",
}

# v2: Direction 30 second pass. Adds ~60% of v1, mults ~60% with SECONDARY
# channels suppressed harder (the green-rich pigment amplifies any mult G, so
# warm suns keep mult G low or the hue dilutes toward white -- measured on v1:
# idle authored add R:G 2.1 arrived on screen as 1.2).
TABLES = {
 "v2": {
  "Idle":       ((240, 70, 12),  (150, 65, 5)),
  "Walk":       ((18, 80, 280),  (10, 35, 135)),
  "Run":        ((280, 90, 10),  (150, 42, 3)),
  "Look":       ((160, 25, 260), (85, 10, 140)),
  "Balance":    ((18, 250, 240), (8, 115, 140)),  # subtlest clip: trimmed less
  "Taunt":      ((250, 18, 210), (140, 6, 95)),
  "SlowTaunt":  ((230, 60, 105), (110, 27, 50)),
  "JumpOne":    ((105, 260, 18), (48, 125, 9)),
  "JumpMulti":  ((290, 60, 8),   (155, 33, 3)),
  "Attack":     ((300, 18, 12),  (160, 6, 6)),
  "Hit":        ((28, 100, 250), (12, 45, 125)),
  "Damage":     ((260, 115, 12), (130, 55, 3)),
  "Knockdown":  ((130, 20, 240), (63, 9, 125)),
  "Fall":       ((55, 150, 270), (27, 70, 135)),
  "HitFloor":   ((250, 125, 20), (125, 57, 6)),
  "SaltoDummy": ((200, 220, 15), (90, 100, 6)),
  "SaltoFly":   ((12, 63, 290),  (6, 27, 155)),
  "SaltoSix":   ((260, 185, 18), (135, 78, 6)),
  "SaltoNine":  ((270, 27, 140), (145, 11, 72)),
  "Death":      ((280, 12, 8),   (150, 4, 4)),
  "Death2":     ((105, 115, 260),(48, 51, 130)),
 },
}


def main(table_name):
    table = TABLES[table_name]
    p = "tools/reel/zhao_reel.cpp"
    s = open(p, encoding="utf-8").read()
    for name, (m, a) in table.items():
        pat = re.compile(r"constexpr ZixxSunSpec kZixxSun" + name +
                         r"\s+\{[^}]*\};[ \t]*// [^\n]*")
        new = ("constexpr ZixxSunSpec kZixxSun%-10s {%s,  kZixxSunInnerMm, "
               "kZixxSunOuterMm, fxm(%d), fxm(%d), fxm(%d), fxm(%d), fxm(%d), "
               "fxm(%d)};  // %s"
               % (name, POS[name], m[0], m[1], m[2], a[0], a[1], a[2], MOOD[name]))
        s, n = pat.subn(lambda _mo, new=new: new, s)
        assert n == 1, name
    open(p, "w", encoding="utf-8", newline="\n").write(s)
    print("sun table", table_name, "applied")


if __name__ == "__main__":
    main(sys.argv[1])
