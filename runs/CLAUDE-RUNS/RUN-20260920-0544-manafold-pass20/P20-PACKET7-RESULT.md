# Packet 7 — the roll-stable aim, and the dip ships

**Date:** 2026-09-20
**Built:** `shortest_arc_from_y` + `nodule_aim_rollstable`; the dent switched ON
at depth 2200 with the swing at 1000; slot 20's per-clip share raised 750 → 900.
**Gate matrix:** 151 legs, **151 PASS, 0 FAIL**, including two new byte-identity
legs (`P20-RECEIPTS/gate-matrix.txt`, `packet7-crc.txt`).
**Renderer:** md5 `d8dcb918c9f6f7a7560479abaee0799f`, built from the committed
source.

---

## 1. The aim, as built

`nodule_aim` composes `quat_z(aim_z) * quat_x(aim_x)`. That lands the segment on
its target, but the roll it leaves about the segment's own axis falls out of the
decomposition — and the decomposition is degenerate when the target leaves the
parent's XY plane, because `aim_z` is then the angle of a vanishing projection.
The swing sends B exactly there, and packet 6 measured **55–80° of frame
rotation in one 60 Hz sample** on a carrier whose position was moving smoothly.

`shortest_arc_from_y` is the minimal rotation instead — the shortest arc taking
the segment's current direction (+Y in the parent frame) to the target
direction:

    a = (0,1,0),  b = v/|v|
    q ~ (1 + a·b, a × b)  =  (L + vy, vz, 0, −vx) / N

Its axis is perpendicular to both directions, so it introduces **no twist about
the segment**, and it is a function of the two directions alone — nothing
accumulates from frame to frame. No trigonometry: one `isqrt64` for `L`, one for
the norm, integer throughout.

**The antiparallel case has a named axis.** At `b = −a` every perpendicular axis
is a shortest arc, so the choice is declared rather than left to arithmetic —
and declared as a CONSTANT, `kNoduleAimFlipAxis`, not as a literal inside the
function. It is `kFoldZ`: a half turn about +Z, the loop's own fold axis, so a
fully reversed segment folds in the plane the creature already bends in rather
than rolling out of it. `kTiltX` is the other way, kept as a knob because which
way a reversal reads is an art question.

**It is exact-off by construction.** `nodule_aim_rollstable` is a separate
function; the production nodule solve still calls `nodule_aim` verbatim, and only
the dent — which is new — aims this way.

### G9 per sample, measured

| configuration | worst angular step on a visible carrier |
|---|---|
| no dip at all (the bank's own baseline) | 7.591° |
| swing, z-then-x aim (packet 6) | **55–80°** |
| swing, roll-stable aim, depth 2200 | **7.772°** (slot 4, f0110, B) |
| …depth 2425 | 8.695° |
| …depth 2700 | 9.457° |

Ceiling `kAntennaMaxAngularStepDeg` = 8°, **not moved**. Accel 5.040 and jerk
5.000 against a 6° ceiling, both *below* the no-dip bank's own 5.280 / 5.247.

---

## 2. The depth, chosen by eye and bounded by one gate

The ladder {2200, 2425, 2700} (s ≈ 0.91 / 1.00 / 1.11) was rendered on Inspect
and Hover and read at 4–5×. **With the roll-stable aim the slab is gone at every
rung** — which retro-diagnoses packet 6's "the fold slabs": it was never the
fold, it was the roll flip tearing the tube's rings apart. The loop reads as a
continuous tube that sags in the middle through the whole gesture
(`P20-LOOKS/p20_rs_strip.jpg`, `p20_rs_ladder.jpg`).

All three rungs read well, so the choice falls to the one gate that is close:
depth 2200 measures 7.772° against the 8° ceiling, 2250 measures 8.080° and is
over. **Depth 2200 ships.** The ceiling was not moved to fit a deeper number;
the depth was chosen to fit the ceiling, which is the only honest direction for
that trade.

---

## 3. B is the lowest ball on every gameplay clip that hosts the beat

R5, shipping configuration: **21 clips author a dip, 19 reach strictly lowest by
the 20 mm margin, 0 fail to return.**

The two that do not:

| slot | clip | best margin | why |
|---|---|---|---|
| 15 | the **lab diagnostic** (`lab::kLabKeys`) | −191 mm | a diagnostic rig pose, not a gameplay animation; its loop never presents B above the other two in the first place |
| 16 | **nodule-solo** (`manafold-nodule-solo`) | −74 mm | the isolated-carrier diagnostic; its whole purpose is to move one nodule against a still loop |

Both are diagnostics, inside the owner's own carve-out. Trick (slot 13) and
Taunt III (slot 21) author no dip at all (`kKneadDipClipPm` = 0), as before.

**Slot 20 (blown) needed the per-clip lever**, and got it: its share went
750 → 900, which took it from −26 mm to strictly lowest. That is the table's
declared purpose. It changes blown's *carried* rendering too — an authored art
value, stated here rather than buried.

---

## 4. Identity and the particle reaction

| leg | result |
|---|---|
| shipping (dent ON) | hover `0xC8FAD499`, inspect `0xFD8D4D0E`, still `0x138FE8B0`, taunt3 `0xC81598AA` |
| `SOLVER=carried` | `0x79D3F0C5`, `0x0710E704`, `0x138FE8B0`, `0xC81598AA` — **4/4 exactly the packet-5 shipping bytes** |
| `SOLVER=carried REAR_BOW=legacy` | `0xE6DD5EBA`, `0xDE1F5918`, `0x75BC4777` — **3/3 exactly `b7c096c2`** |
| `ZHAO_U02_FOLD_DIP_PM=0` (particle exact-off) | inspect `0xE5C1D75B` ≠ `0xFD8D4D0E` |

Still (`0x138FE8B0`) and Taunt III (`0xC81598AA`) are **unchanged by the dent
shipping**, because neither authors a dip — the two clips that must not move did
not move, and that is a stronger statement than a toggle.

The last row is the point of the particle leg: the fold's agitation is read from
**B's sag below the A/C midline in the posed rig**, so switching solvers carried
it across with no extra wiring, and turning its gain to zero still reverts the
mana to its version-18/19 bytes. The reaction is tied to the dip that ships.

---

## 5. Bounds

None moved, in either packet. `kSpanStretchMaxPm`, `kSpanCompactionMinPm`,
`kSpanMinRunMm`, `kAntennaMaxAngularStepDeg/AccelDeg/JerkDeg`, every R1..R5
ceiling and every G5..G9 threshold are exactly as pass 19 left them. The
shipping configuration has **0 bound/margin breaches**.
