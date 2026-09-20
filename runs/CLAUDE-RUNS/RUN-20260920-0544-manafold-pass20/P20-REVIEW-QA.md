# Pass 20 — independent REVIEWER + QA report

**Reviewer:** independent review/QA agent, 2026-09-20
**Tree reviewed:** zhaozhou `manafold-pass20` @ `74f1d7dd` (+ this review's repairs)
**Binaries:** built directly from source with the committed recipe
(`tools/reel/build-direct.sh --output .tmp/p20-review <target>`, one target per
invocation), g++ 16.1.0 MinGW-W64 from the `zhao-env.ps1` toolchain. No CMake
result is claimed.

---

## VERDICT: **BLOCKED**

* **Claim 1 — the rear rip is repaired at the root: CONFIRMED.** Sound in the
  maths, sound under the controls, and — the part that decides — sound to the
  eye at the frame where pass 19 tore. This half of the pass is good work and
  should stand.
* **Claim 2 — one walk, pinned by G11: CONFIRMED,** and better than declared
  (the pin residual is 7 mm, not the 21/38 mm still in the reports).
* **Claim 3 — "the knead dip ships; B strictly lowest on 19 of 21;
  particles react": FALSE AS SHIPPED, on both halves.** The 19/21 was measured
  at a dip gain of 1000; the tree ships 550, where it is **4 of 21**, and 1000
  **fails mspan's G9 by 6.4×**. The particle reaction changes **12 frames of
  600** on Hover and **72 pixels** at its strongest. Direction 21 items 2 and 3
  are outstanding.
* **Claim 4 — the gate changes: five of six sound, one receipt false, one
  specified gate never built.**

Nothing here asks for a bound to move. The block is on the *reporting*: the
matrix was green while one leg described a creature nobody renders.

---

## 1. The finding that decides it — the dip's two claims are mutually exclusive

`kKneadDipGainPm = 550` ships (`manafold_art.h:4880`). `mrear --gate --dip`
set `g_u02_knead_dip_gain_pm = 1000` (`manafold_rear_audit.cpp:976`), so the
matrix leg `n-mrear-dip` — and packet 7 §3's table — measured **1000**.
`P20-GATE-CHANGES.md` packet 7 says the leg "now judges R5 on the shipping
configuration rather than on a knob turned on for the leg". It did not.

My ladder, both instruments, same binaries, one sitting:

| dip gain | mspan **G9** worst angular step (ceiling **8.0°**) | **R5** clips where B never reaches lowest, of 21 |
|---|---|---|
| **550 — SHIPS** | **7.772  PASS** | **17** (B lowest on 4) |
| 650 | 9.212  **FAIL** | 12 |
| 750 | 11.283 **FAIL** | 7 |
| 850 | 24.237 **FAIL** | 6 |
| 1000 — the quoted figure | 50.963 **FAIL** | **2** (the claimed "19 of 21") |

It is not an artefact of which knob was turned. `s` is a product, and gain and
depth are interchangeable in it: `gain 1000 / depth 1210` measures 7.740° and
18 never-lowest (i.e. the shipping point), and `gain 550 / depth 4000` measures
17.924° and 2 never-lowest (i.e. the claimed point). **The ranking the owner
asked for and the continuity ceiling trade monotonically, and the dent cannot
satisfy both.**

At gain 1000 the G9 *position* step is 72.359 mm against the shipping 72.303 —
unchanged — while the angular step goes 7.8° → 51°. That is the packet-6 **roll
flip**, back. The roll-stable aim did not remove it; it pushed it past the
shipping amplitude. Packet 7's retraction of packet 6's verdict is right about
the cause and wrong about the cure being unconditional.

**Which clips actually get the beat at shipping:** slots 0, 1, 2 and 7 only —
idle (Hover/Inspect), Drift, Channel, and Still, which is a two-frame
diagnostic. The other 17, including damage, hit, taunt, taunt2, fall, hasty,
blown and both deaths, never rank B lowest. The owner asked for it **on every
animation**.

## 2. The particle reaction is mathematically present and visually absent

`ZHAO_U02_FOLD_DIP_PM=0` against shipping, Hover, all 600 frames, production
ink:

* **588 of 600 frames are byte-identical.** Twelve frames differ at all.
* Strongest frame (0257): **323 px** of 92,160 changed; **72 px** by more than
  24/255; max delta 132.
* The dip's own geometry moves **6,145 px** (>24) on frame 0285. The reaction is
  ~1% of the gesture's visual weight.
* At **10×** the whole difference is one lightning bolt a few pixels longer and
  one small spur. At 384×240 it cannot be seen.

Cause: `manafold_fx.h` needs `sag_mm > kFoldDipOnsetMm` (90) and is only fully
roused at `kFoldDipRefMm` (420). At gain 550 B's sag clears 90 mm on about 2% of
frames. The identity leg — "`FOLD_DIP_PM=0` changes the bytes" — is true and
says nothing about visibility. This is CLAUDE.md's crayon-grain failure in a new
costume: *it measured fine and looked like flat plastic.*

## 3. Claim 1 — the arc/bow repair

**Maths.** `rear_bow_alpha16` bisects `L·sin(a) = c·a` in cross-multiplied
integers; sinc is strictly decreasing on (0, π) so the bisection is exact and
deterministic. Overflow headroom is comfortable (worst LHS ≈ 4.3e12, worst RHS
≈ 1.3e13, both int64). Degenerates handled: `c ≥ L` and `c ≤ 0` return 0 → the
old linear law, which is correct on the taut side; below `kRearBowMinAlpha16`
(48 a16 ≈ 1.2 mm of sagitta) the arc is the straight line and the switch is a
~1 mm step, not a cliff.

**Closure really is by construction.** At `s = 0`, `φ = 0`, `θ = −α`, so
`dx = R(cos(−α) − cos α) = 0` and `dy = R(sin(−α) + sin α) = 0` exactly. At
`s = L`, `φ = 2α`, `θ = +α`, so `dx = 0` and
`dy = 2R sin α − L = c − L` exactly — the arc's far end lands on the chord.
Both hold on the symmetry of `fx_sin`, in integers, with no accumulation.

**But "by construction" is true of the parameterisation and not of the rig.**
The helpers stop at ~752 mm of a 1010 mm band; the last 258 mm is `kBRearSocket`,
body-pinned. That is where the 113° centreline turn and the 361 mm two-bone
disagreement live. The maths closes; the skeleton hands off. Say it that way.

**The cap branch is dead at ship.** `rear_bow_alpha16`'s bisection is bounded by
`hi = 32000` and `kRearBowMaxAlpha16 = 32000`, so `alpha16 > max` is never true
and the `bow_chord_mm` recovery never runs. Laddered off deliberately; noted so
nobody reads it as live code.

**Determinism:** integer bisection, fixed iteration counts, no floating point in
the solve path. Confirmed.

## 4. Claim 2 — the unified walk, and the residual

`loop_walk` pairs segment *i*'s arc with the span delta on `span_child[i]`, the
bone that **ends** it — the closure's own pairing, factored out verbatim. G11
probes it with 125/375/750 mm deltas (multiples of 125, so the fx16 round trip
is exact) and its control fires (§6).

**The declared residual is stale, and the truth is better.** The "21 mm / 38 mm"
figures were measured through `nodule_aim` (z-then-x) at depth 2000 in packet 6.
Packet 7 replaced the aim and never re-measured. I rebuilt the **committed**
probe — `g++ -DZHAO_P20_PINPROBE … manafold_spangate.cpp` — and ran it:

| configuration | worst `|C_after − C_before|` (L1), 3424 dent samples |
|---|---|
| **shipping** (dent, swing 1000, depth 2200, gain 550) | **7 mm** |
| swing 0 (the press) | 8 mm |
| gain 1000 | 35 mm |

The roll-stable aim cut the pin residual about five-fold. That is a real,
undeclared improvement — and the 35 mm at gain 1000 is one more way the same
over-deep configuration shows its strain.

**The pin holds on the spans too.** C–E rear span excursion, dent-on versus
`KNEAD_DIP_PM=0`, all 23 slots: every difference is a *reduction* or within
2 mm. F–A and C–E genuinely have no term in the dent at shipping amplitude.

## 5. Claim 3's mechanism — the roll-stable aim

`shortest_arc_from_y` is the correct half-way form: for `a = (0,1,0)`,
`q ∝ (|v| + v_y, v_z, 0, −v_x)`, whose vector part is exactly `a × v` and whose
scalar part is `|v|(1 + a·b)`. Axis perpendicular to both ⇒ no twist about the
segment; a function of the two directions alone ⇒ nothing accumulates. Integer,
two `isqrt64`, deterministic.

**The antiparallel case is declared and handled** (`x == 0 && z == 0 && y < 0` →
half turn about `kNoduleAimFlipAxis`, +Z). **It is also genuinely reachable in
its neighbourhood**, and that is where the value degrades: near `b = −a` the
norm `n` collapses and every component is divided by it, so the quaternion's
direction is right and its quantisation is coarse. The 51° step at gain 1000 is
that neighbourhood. The exact pole is guarded; the approach to it is not, and
the shipping amplitude is what keeps the design out of it.

**Exact-off is genuine:** `nodule_aim_rollstable` is a separate function, the
production nodule solve still calls `nodule_aim` verbatim, and the matrix's
`e-identity-carried` leg holds the carried solver to the packet-5 bytes.

---

## 6. Gate-change audit

| # | change | still catches the old fault? | encodes an art value? | control real? | verdict |
|---|---|---|---|---|---|
| 1 | **R1 centreline 60 → 140°** | Yes — v18 reads 171 and `--fail-rear-frame` fires (mask 0xB). The hairpin's *real* guard, arm↔End rotation at 40°, is untouched and reads 16.5. | No. 113° is admitted, and I looked at it (§7a): it is a rounded shoulder, not a crease. | Yes, fired. | **SOUND** |
| 2 | **R4 regression floor 0.12 → 0.40** | Yes — and for the first time it *can* fail for the right reason. The old floor sat *below* the defect. | No. | Yes: `--fail-rear-strain` = `REAR_BOW=legacy` → rail **0.129**, mask **0x8 alone**. Fired by me. | **SOUND** |
| 3 | **R4 hand-off 320 mm hard → reported** | **Partly.** The quantity that *found* the fault now has **no regression guard at all**, and shipping reads **361 mm**, past the bound that was removed. | No — but the stated reason ("it measures how curved the band is") is not quite right: it measures how far two influencing bones disagree about the same vertex, which is a skinning-smear risk, not curvature. | Deferred to the rail floor. | **ACCEPT WITH A GAP** — see follow-ups |
| 4 | **R4 stretch 1.80 → 2.10, step 0.12 → 0.18** | Yes, as forward regression guards. | No. | Row 2's. | **SOUND, mis-attributed.** The doc credits the rise to "the dip ladder (1.919)". I measured it: with `KNEAD_DIP_PM=0` the bank still reads **2.052** and step **0.1630**. **The bow raised these, not the dip.** The dip costs them nothing. |
| 5 | **mspan G5 → the production writer** | Yes for drift/corruption; the ±1 mm chord reconstruction is the exact ambiguity width, not slack. Controls `--fail-e-start/mid/presocket` fire. | No. | Yes, fired. | **SOUND, with a named limit:** G5 now compares the shipped helpers against `write_rear_bow`, the function that wrote them. It is a **consistency** check, not a correctness one — a wrong law passes. Correctness is carried by R4 and by the eye. Say so where it is quoted. |
| 6 | **mspan G6 → local turn + pinch** | Yes, and better: the old projection test could not tell "bowed" from "folded". `--fail-posed-order` fires through the **new** arm. | No. | Yes, fired. | **SOUND.** The leg printed only "0 reversed" with no margin, so I added the margin (§8): shipping **66.43°** against 140°, mutant **160.20°**. Comfortable, and now checkable. |
| 7 | **G11 WALK PAIRING (new)** | New property, nothing loosened. | No. | Yes: `--fail-walk-pairing` → **4 of 5 segments mispaired, +750 mm**, attributed `0x8000`. Fired by me. | **SOUND** |
| 8 | **`kAntennaMaxAngularStepDeg` NOT moved; depth fitted to it** | — | — | — | **The direction of the trade is right and the claim around it is wrong.** 2200 fits the 8° ceiling *at gain 550*. The 19/21 was then quoted from 1.8× deeper, which the same ceiling rejects. Art fitted to gate: yes, for the depth. Claim fitted to neither: yes, for the ranking. |

**Not moved, verified:** `kSpanStretchMaxPm`, `kSpanCompactionMinPm`,
`kSpanMinRunMm`, and the ±2 pm bound tolerance are byte-identical to the branch
point (`git diff 4b3d4576 HEAD -- manafold_spangate.cpp` shows no change on any
of them). No bound was relaxed anywhere in this pass. That part of the ledger is
honest.

### Instruments that were not what they claimed

1. **`--dip` overrode the shipping gain** (§1). Repaired (§8).
2. **R5's hard leg has one arm with no control.** R5 fails on
   `(dip_clips == 0 && forced) || dip_stuck != 0`. `--fail-no-dip` sets the gain
   to 0 and so only ever exercises the *first* arm. The `dip_stuck` arm — B sags
   and never returns, the interesting failure — **has never been seen to fire.**
3. **mspan G10 DENT PIN was specified and never built.**
   `P20-SOLVER-ARCHITECTURE.md` revision 1 specifies G10 DENT PIN with
   `--fail-dent-pin`, and an R4 front-window floor with `--fail-dent-overfold`.
   Neither exists: `grep -n "G10\|fail-dent"` over `manafold_spangate.cpp` and
   `manafold_rear_audit.cpp` returns nothing. **The dent's central contract is
   ungated** — the +19 mm leak packet 5 measured and packet 6 repaired could
   return silently. (The brief's "new G9/G10/G11" is also wrong about G9: G9 is
   pass 19, `075d88af`, and pass 20 did not touch it.)

---

## 7. Visual QA — production ink, `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`

Nine subjects rendered at native (Inspect, Hover, Rest, Drift, Channel,
Taunt III, Trick, Blown, Death-drop), plus three A/B banks: pass-19
(`SOLVER=carried REAR_BOW=legacy`), dip-off (`KNEAD_DIP_PM=0`) and
particle-off (`FOLD_DIP_PM=0`). Frames were sampled by **badness** — ranked by
per-frame difference against the A/B bank — not by index, plus full contact
sheets of Inspect (600) and Channel (420).

**(a) Is the rear connection whole through the whole orbit? — YES.**
At Inspect **f380**, which is pass 19's *own* worst rip (legacy rear rail
0.147), the P19 tube ends in a pinched stub at the body edge; P20 continues it
as a rounded tube that wraps into the body. At **Channel f080** — the worst
sample in the bank, 113° centreline turn and 361 mm hand-off — the join reads as
one continuous trunk fusing into the body, with no crease, no gap and no pop
across f078/f080/f082. Across the full orbit (f060…f599) the junction is clean
at every angle sampled. **The R1 re-expression and the hand-off demotion are
supported by the eye**, which is the only support that counts for them.

**(b) Does the knead read as a knead? — PARTLY.**
It reads as the loop *flattening and compressing* through the beat, which is a
knead and not a twist, a slab or a broken antenna. **Packet 6's "hard
rectangular slab" is genuinely gone** — I compared the shipping dip against the
dip-off bank at the same frames and the shipping loop stays a tube. Packet 7's
retraction is correct on that point.
**But B visibly becoming the lowest ball is not legible.** On Hover through the
deepest part of the beat (f270→f310 at 6×) the loop compresses; B does not read
as *the bottom ball*. That matches the measurement — 4 of 21 clips, and Hover
clears the 20 mm margin only barely. The owner's sentence is not yet on screen.

**(c) Do the particles react deliberately? — NO.** §2. Twelve frames of 600,
72 pixels at the strongest, invisible at native.

**(d) New faults anywhere? — NONE FOUND.** Contact sheets of every frame of
Inspect (600) and Channel (420) show no broken, exploded or missing frame.
Rest, Drift, Taunt III, Trick, Blown and Death-drop all read correctly at 3×:
continuous enclosed ink outline, eyes intact and correctly placed, ground
contact sensible (Rest sits on the dirt, Trick plants, Death lies on it), no
seam, no stray geometry, mana character unchanged, distance-scaled lines
thinning correctly on Drift and Blown.

---

## 8. What I changed, and why

All changes are to **instruments and comments**. No bound, threshold, art value
or shipping byte was touched.

1. **`manafold_rear_audit.cpp` — `--dip` no longer overrides the gain.**
   It now only sets `g_force_dip_leg`; the printed line names the configured
   gain. `n-mrear-dip` therefore judges the creature that ships. The leg still
   passes (the hard arm is "happens and returns", which holds) and now prints
   the true 4-of-21 instead of a fiction. `--fail-no-dip` still sets the gain to
   0 explicitly, so R5's control is unaffected — verified, still fires 0x10.
2. **`manafold_rear_audit.cpp` — the OPEN R5 text** now states the shipping
   reality and carries the gain↔G9 ladder, so the next reader cannot conclude
   "it is one knob away". It is not.
3. **`manafold_rear_audit.cpp` — three stale comments repaired.**
   `kGateRailTargetFloor` said "Shipping BREACHES it today (worst 0.147)";
   `kGateRailCeiling` quoted the pre-bow parity 0.129; the OPEN BREACH R4 string
   said "Not repaired in pass 20" — it *is* repaired, and if that line ever
   prints again it now says so. A comment that describes a fixed defect as
   current is the reassuring-provenance-line hazard.
4. **`manafold_spangate.cpp` — G6 prints its margin.** The re-expressed leg
   printed "0 reversed" and nothing else. It now reports the worst
   consecutive-step turn: **66.43°** shipping against the 140° ceiling, and
   **160.20°** under `--fail-posed-order`. A detector reading zero is a claim.
5. **`manafold_art.h` — three stale comment blocks flagged and corrected**
   (`kKneadDipGainPm`'s "STILL SHIPS OFF" and its 15-of-21 line;
   `kKneadDipFoldPm`'s pre-bow ladder). The shipping 4-of-21 and the gain↔G9
   trade are recorded beside the constant that causes them.

## 9. Receipts

* **Gate matrix: 151 legs, 151 PASS, 0 FAIL** — `P20-RECEIPTS/gate-matrix-review.txt`, run from scratch
  in **one invocation** after every change above.
* **Controls fired by me, by hand:**
  * `--fail-rear-strain` → rc 1, worst rear rail **0.129**, `rear gate mask 0x8`.
  * `--fail-walk-pairing` → rc 1, **4 of 5 segments mispaired, +750 mm**,
    attributed **0x8000**.
  * `--fail-no-dip` → rc 1, `rear gate mask 0x10`.
  * `--fail-posed-order` → rc 1, worst turn **160.20°** through G6's new arm.
  * `--fail-e-start` / `--fail-e-mid` / `--fail-e-presocket` → rc 1 each.
* **Pin probe** (committed, `-DZHAO_P20_PINPROBE`): shipping **7 mm L1**.
* **Renderer md5** (this review's build): `6194148838d40ea96b9ff246a9f08c2e`.
  Note the packet-7 report quotes `d8dcb918…` from the same source — the direct
  build is not byte-reproducible across environments, so the md5 identifies a
  build, not a tree. The CRC identity legs are the tree's receipt.

## 10. Follow-ups, in priority order

1. **Decide item 2 with the owner's eye, not with a knob.** The dent cannot
   reach "B lowest on every animation" inside G9. The path named by the pass's
   own earlier packets — redistribute across A/B/C as Taunt III's crown shuffle
   does — is still the right one, and it is authoring.
2. **Item 3 needs a visible amplitude.** `kFoldDipOnsetMm` = 90 against a sag
   that rarely clears it. Lower the onset, or drive the reaction from the dip's
   *schedule* rather than from the achieved sag, then look at it.
3. **Build G10 DENT PIN with `--fail-dent-pin`.** The probe already exists and
   the shipping number is 7 mm; a leg bounding it would cost little.
4. **Give R5's `dip_stuck` arm a control**, or fold the arm into the one that
   has one.
5. **Put a regression ceiling back on the hand-off** — reported *and* bounded,
   at a number above the shipping 361 with margin. Removing the bound entirely
   left the quantity that found the fault with no guard.
