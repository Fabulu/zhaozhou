# P22 findings 02 -- the dot ladder, looked at (item 1)

Plates: P22-LOOKS/01..04. All read at native 384x240, magnified NEAREST.

## 01 -- Drift f289, legacy vs ink@1000, 3x full frame
- LEGACY is the owner's complaint, plainly: a fused cloud of pale balls, each
  one wider than the antenna loop it sits on. The green pocket is swamped and
  the white lightning strand is buried under it.
- ink@1000 (g=274 at Drift's 127.7 px) **OVERSHOOTS**: the dots go to dust, the
  green/aqua read disappears, only the lightning line is left. The owner said
  "too big", not "remove them". So the obvious law does overshoot and the
  strength ladder is needed -- exactly the case the brief anticipated.

## 02 -- Drift f289, six rungs at 5x
legacy / ink s=350 (g=746) / s=500 (637) / s=700 (492) / LINE law (355) /
ink s=1000 (274).
- s=350: barely different from legacy. Not delivered.
- s=500: dots become separate DOTS again; lightning loop reads on top.
- s=700: smaller again; outer wanderers starting to square off.
- LINE law s=1000 (the pass-19 line law, g=355): green nearly gone, wanderers
  are 1-2 px and read as dirt.
- ink s=1000: dust.

## 03/04 -- Drift f289 and Hasty f012 at 7x, s = 500 / 600 / 700 vs legacy
Hasty is the clearer read of the two: at legacy the pale balls fill the antenna
loop and bury the two white bolts; from s=500 up the bolts are crisp again.
- 500: soft round dots, generous.
- **600: chosen.** Radius is 564 pm of legacy at Drift -- 56% of the radius,
  ~32% of the area, so the reduction is unmistakable at native. The dots still
  read as soft ROUND dots with their aqua/green colour intact, the wanderers are
  still particles rather than dirt, and the lightning and the pocket both read.
- 700: acceptable, but the outer wanderers begin to read as hard little squares
  rather than soft motes. 600 keeps more of the mana character for a reduction
  that is already plainly delivered.

## The decision, stated honestly
`ZHAO_U02_MANA_DOT_SCALE=ink` with `kManaDotStrengthPm = 600`.
The LAW is the ink's (the owner's named model: same operand, same 120/200/360
knots, same flat floor and ceiling). The STRENGTH is an art value at 600, not
1000, and the reason is that the ink and a dot have different vanishing points:
a 1 px line is still a line, a 2 px dot on a 3 px neighbour is dirt. Following
the ink's curve at full strength was rendered and looked at, and it removed the
effect. This is the ink's law taken 60% of the way, chosen by eye.

**Not an invisible change:** every one of Drift's 300 frames differs, mean 1887
changed pixels of 92,160 (2.0%) at ink@1000; Drift's unique-colour count falls
13,654 -> 9,554. Compare pass 20's rejected first reaction: 12 of 600 frames and
72 pixels.
