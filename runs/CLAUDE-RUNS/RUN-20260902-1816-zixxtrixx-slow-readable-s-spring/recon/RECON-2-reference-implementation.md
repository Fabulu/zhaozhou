# RECON 2 — What "the reference implementation" is, and what the passes since then lost

**Run:** RUN-20260902-1816-zixxtrixx-slow-readable-s-spring
**Scope:** read-only. Nothing in either repo was modified except this file.
**Method:** every frame of six jump-one banks extracted and looked at (contact
sheets, 3x key-pose stacks, side-by-side arming sheets), silhouette measured on
the comparison side only, pose tables pulled from `git show`.
Scratch evidence: `…/scratchpad/recon2/` (sheets, per-frame segmentations).

---

## VERDICT UP FRONT

**The reference implementation is Archive Generation Thirteen — the Direction-21
"whole-body S squeezed from the top" bank, zhaozhou commit
`a2f601ef6d9234e1550954cf3ce88b548602e9f5`.**

Confidence: **high (~85%)**. It is the only bank the owner has ever praised
("markedly improved"), it is the only bank in the whole archive that actually
performs Direction 23's described motion, and it is the immediate predecessor of
the rejected pass. The residual 15% is the word *"still"* in "still worse than
the reference implementation", which can be read as implying the D21 pass was
also worse than it. Section 4 handles that reading and rejects it.

---

## 1. The generation timeline

`1 key = 2 rendered frames` (60 Hz presentation of 30 Hz keys) — confirmed
against the `kSalto*Key` constants and the rendered frames. For every jump-one
clip: arming = keys 0–16 = **frames 1–33**; loaded hold = keys 16–18 = frames
33–37; release = keys 18–22 = frames 37–45; flight from frame 45.

| Gen | Published | Direction it answered | zhaozhou state | Owner's verdict on it |
|---|---|---|---|---|
| Nine | 2026-08-30 | pre-D19 | — | rejected: tail-led / root-sliding spring |
| Ten | 2026-09-01 | D19 whole-S groundspring | — | rejected (superseded by the D20 bank) |
| Eleven | 2026-09-01 | D19 second attempt | pre-`65100652` | D20: *"still awful. It's slightly better. But it's rigid."* — rejected on six counts |
| Twelve | 2026-09-01 | D20 lean-back / arm-slowly | `3d91114815f7c687a2b718ce0c1649cbf384c345` | D21: *"completely ridiculous now in an extraordinarily bad way"* |
| **Thirteen** | 2026-09-02 | **D21 squeezed-S** | **`a2f601ef`** | **D22: *"Zixx jump and salto were markedly improved."*** — accepted with faults |
| live | 2026-09-02 | D22 extreme coil | `002022700f8b5d6b5f6d33f09d5d8c4f40cabcd2` (bank `dc6bb088`) | D23: *"a spazzy fuckfest… you can't even see what it's supposed to be"* — REJECTED |

Other SHAs an implementer will want:
`65100652103807ba9daa3ef3237360fa4ed5f925` (D20 first commit — carries the unit
fix), `f9c75809a1de18acd6da5d6dd2570b43ba2200a2` (first D22 commit — where the
coil begins), `d26255df76118e41c2136ff186ab0da770f39d54` (D21 pass close-out log).

`tools/reel/zixxtrixx.h` was touched by exactly these commits in this window, in
order: `1fb1fcc3 → d499b0e0 → 65100652 → 3d911148 → a2f601ef → f9c75809 →
e25b5121 → 91373558 → 05c606bd → e5402d7a → c7191479 → 00202270`. **Nothing
touches it between `a2f601ef` and `f9c75809`**, so Generation Thirteen's code
state is exactly `a2f601ef` — no archaeology needed.

---

## 2. What each bank actually does (looked at, every frame)

Heading units are **Q16 turns: 65536 = 360°, so 1° = 182.04**. Degrees below.

* **Gen Nine / Ten** — no anticipation worth the name. Gen Ten's silhouette
  height never leaves its idle 61 px band before the launch. Not candidates.
* **Gen Eleven** — a shallow lying serpentine wave. Measured minimum bbox height
  during arming = **60 px against an idle 61 px**: there is essentially *no
  compression at all*. Readable, but nothing happens. Not a candidate.
* **Gen Twelve** — the front half rears into a vertical cobra "3" and the rear
  stays a rail on the dirt. `kSpringCollapsedHeading[0] = 30000 = 164.8°`: the
  head segment is rolled right over. Max silhouette width is at **frame 1** — the
  shape only ever gets narrower and taller, i.e. it never compresses. Exactly the
  owner's "you turned the front part of the snake around by 180 degrees… then it
  just jumps, without compressing." Not a candidate.
* **Gen Thirteen** — a continuous, legible transformation of the lying S that
  presses **shorter and broader**, then launches. See §3.
* **live (D22)** — the animal winds into a knot in 5 frames, sits as an
  unreadable blob for 16 frames, snaps back open in 2 frames, then launches.
  See §3.

---

## 3. Thirteen vs live, measured (silhouette bbox, comparison side only)

Segmentation: body = saturated green/cyan/violet plus the dark outline. Fixed
side-tracking camera, identical framing in both clips — like compared with like.

| frame | key | GEN THIRTEEN w×h, area | LIVE w×h, area |
|---|---|---|---|
| 1 | 0 | 186×61  a=3933 | 186×61  a=3933 |
| 9 | 4 | 179×62  a=3938 | 189×67  a=3968 |
| 17 | 8 | 159×66  a=3880 | 172×76  a=3559 |
| 21 | 10 | 179×70  a=3769 | **120×53  a=2392** |
| 23 | 11 | 195×70  a=3744 | **87×54  a=2362** |
| 29 | 14 | 220×55  a=3617 | 99×55  a=2870 |
| **33** | **16** | **222×51  a=3554** | 100×53  a=2619 |
| 37 | 18 | 217×52  a=3542 | 100×52  a=2605 |
| 39 | 19 | 176×71  a=3809 | 122×54  a=2404 |
| 41 | 20 | 165×63  a=3907 | **176×79  a=3967** |
| 45 | 22 | 186×61  a=3804 | 186×61  a=3803 |
| 49+ | 24+ | *identical to live* | *identical to Thirteen* |

**Frames 45 onward are the same clip.** Areas agree to within 3 px. The launch,
the airborne wheel, the landing and the settle are byte-equivalent between
Generation Thirteen and live. **The entire regression is contained in frames
1–44 — keys 0–22 — and nothing else needs to be touched.**

### The four numbers that decide it

1. **Legibility.** Thirteen's silhouette loses **11%** of its area across the
   whole arming (3933 → 3518) and *gains* width (186 → 222 px). Live loses
   **47%** (3933 → 2079 at frame 22) and drops to **93×46 px** — a quarter of
   the projected area and half the width. At 384×240 that is a green smudge.
   This is the measurable form of *"you can't even see what it's supposed to be."*
2. **Pace.** Live's whole shape change happens between frames 17 and 22 —
   **5 frames, 83 ms**. Thirteen changes continuously from frame ~9 to frame 37
   — **28 frames, 470 ms**. For scale, the tail-balance clip the owner names as
   the pace reference takes **~55 frames (0.92 s)** to make its whole-body change
   (bbox height 45 → 78 px between frames ~75 and ~130), and its silhouette area
   moves only ~10% while doing it. **Thirteen is in the same family as the
   balance clip; live is 11× too fast and shrinks instead of reshaping.**
3. **The blob hold.** Live holds a ~100×53 knot from frame 23 to frame 38 —
   16 frames in which nothing legible happens — then snaps back open in
   **2 frames** (122×54 → 176×79, frames 39→41) before the release. The owner
   sees three abrupt events, not four readable beats. Thirteen's deepest pose is
   a continuous 222×51 press held over frames 29–38 that still reads as a snake.
4. **Head past tail.** At live frame 23 the tail fork has swung round to the
   **right of and under the head** — the two ends have crossed. That is literally
   *"the snake head shoots way past the tail."* In Thirteen the fork stays on the
   left through every arming frame.

### 3b. THE JITTER — Direction 23's primary fault, measured

Direction 23 was sharpened mid-run: *"with you can't see what it's supposed to be
I meant not shape but the movement. It's so jittery and fast there's no
smoothness."* Smoothness is now acceptance test #1, so it gets its own
measurement. Below: per-frame deltas of the tracked silhouette centroid, area and
head centroid, over each clip's own whole-body transformation window. **Jerk** =
max frame-to-frame change *in the delta*, i.e. the acceleration discontinuity —
that is what "jittery" is, and it is the column that matters.

| tracked signal | BALANCE (f75–130) | GEN THIRTEEN (f1–44) | LIVE (f1–44) |
|---|---|---|---|
| centroid x — max jerk | **1.5 px** | 7.2 px | **23.0 px** |
| centroid y — max jerk | **0.7 px** | 2.2 px | **17.6 px** |
| area — max jerk | **53 px²** | 157 px² | **1168 px²** |
| head x — max jerk | **4.0 px** | 9.3 px | **30.9 px** |
| head y — max jerk | **2.9 px** | 3.4 px | **15.3 px** |
| centroid y — max speed | **0.6 px/f** | 3.3 px/f | **10.4 px/f** |
| area — max speed | **40 px²/f** | 212 px²/f | **1070 px²/f** |

Three findings, in order of importance to the architect:

1. **Live is 10–25× rougher than the balance clip on every signal.** Its
   silhouette area changes by up to 1070 px² in a single frame and the *change
   itself* jumps by 1168 px² frame to frame. That is not motion; that is a pose
   swap. This is the numeric form of "spazzy".
2. **Generation Thirteen is much smoother than live — but it is NOT as smooth as
   the balance clip.** 7.2 px of centroid-x jerk against the balance clip's
   1.5 px, and 157 px² of area jerk against 53 px². **Restoring Generation
   Thirteen verbatim will not pass Direction 23's first acceptance test.** The
   reference is the right *starting point*, not the finished answer; the arming
   still has to be re-eased on top of it.
3. **Do not grade smoothness by counting sign reversals.** The balance clip has
   *more* area-delta reversals (33) than live (15) — because its motion is a
   continuous living wobble of small amplitude, which is exactly the quality the
   owner wants. Amplitude of jerk separates good from bad here; reversal count
   does not. A gate built on reversal counting would reject the reference clip
   and pass the rejected one.

### The tables

```
segment            0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18
idle hook (5-9)                                    80.2 117.6 138.4 109.9  63.7
GEN13 assembled  -8.2   2.2   9.3  20.9  37.4  80.2 117.6 138.4 109.9  63.7   1.6   2.7   3.8   5.5   8.2 -24.7 -52.2 -71.4 -79.7
GEN13 COLLAPSED  -8.2   2.2   4.9   6.0   7.7  93.4 151.1 170.3  90.6  24.7   4.9   4.9   5.5   6.6   9.3 -15.4 -34.1 -26.4  -8.8
LIVE  assembled -15.0  -5.0   6.0  21.0  40.0  79.0 114.0 135.0 155.0 135.0  -4.0  -8.0 -12.0 -16.0 -45.0 -60.0 -70.0 -45.0 -25.0
LIVE  seating    30.0  10.0   4.0   8.0  24.0  62.0  86.0 106.0 176.0 205.0 280.5 213.5 182.0 180.0 135.0 118.0 106.0  12.0   6.0
LIVE  COLLAPSED -10.0  -4.0   0.0  18.0  42.0  78.0 110.0 131.0 176.0 205.0 280.5 213.5 182.0 180.0 142.0 124.0 106.0  12.0   6.0
GEN12 COLLAPSED 164.8 156.6 137.3 109.9  79.7  57.7  57.7  79.7 107.1 123.6 123.6 112.6  93.4  65.9  35.7  13.7   2.7 -16.5 -38.5
```

Read that: **Generation Thirteen's assembled pose reproduces the idle hook
exactly** (80.2 / 117.6 / 138.4 / 109.9 / 63.7 — the literal `kStanceSlope`
values). That is *why* it stays recognisable: the loaded shape is the idle S
pressed, not a different animal. Live winds segments 8–16 through 106°–**280.5°**
— three quarters of a turn in the mid-body — and the head hook opens *forward*
into the front of the knot.

---

## 4. Why not one of the other banks

The counter-reading of "**still** worse" is that Gen Thirteen was also worse than
some earlier reference. It does not survive contact with the archive:

* Direction 22 opens *"Zixx jump and salto were markedly improved"* about
  Generation Thirteen. The owner does not call an improvement "worse".
* Direction 23's own description — whole body slowly becomes the S, then the S
  compresses with everything descending — is a description of **what Generation
  Thirteen does**, plus one correction (a *slight* backward head travel) that is
  D22's ask dialled down. Nothing earlier does the first half at all: Nine and
  Ten have no anticipation, Eleven compresses by 1 px, Twelve rears up.
* Direction 21 §6 named Generation Eleven explicitly as "what was closer", so the
  owner does name banks when he means a specific one. Here he named none, which
  fits "the last one that was good" — and only one bank was ever called good.
* The phrase "reference implementation" elsewhere in these repos means the
  software renderer oracle (`Upheaval/docs/DESIGN.md:313`). That is a renderer,
  not an animation, and cannot be what the owner is comparing a jump to.

Best reading of "still": *"even after all this iteration, it is worse than the
one good bank."* Colloquial, not a claim about a second prior comparison.

---

## 5. LOST — what Generation Thirteen did that the D22 pass destroyed

| # | What Thirteen had | What live did to it |
|---|---|---|
| L1 | **Legibility at every arming frame.** Silhouette never below 3518 px², never narrower than 159 px. | Drops to 2079 px² / 93 px wide. The loaded pose is an unidentifiable knot. |
| L2 | **A continuous 28-frame arming**, one route, no waypoint. | The change is crammed into `kSpringCoilFormationWindBeginTick=13 → WindEndTick=21` (frames 13–21) with a new `kSpringSeatingHeading` waypoint at `kSpringArmSeatingAt=700`. 5 frames of motion inside a 33-frame budget. |
| L3 | **A legible 10-frame hold** (frames 29–38) of a pose you can read. | 16 frames of a static blob, then a 2-frame snap-open at `UnwindBeginTick=38 → UnwindEndTick=40`. |
| L4 | **The idle hook preserved** in the assembled pose (80/118/138/110/64°). | Assembled hook opened to 79/114/135/**155/135**°; the collapsed hook to 78/110/131/**176/205**°. The front is no longer the signature S. |
| L5 | **Head and tail on opposite sides** through the whole arming. | Tail fork crosses to the head's side by frame 23. |
| L6 | **Shorter-and-broader compression** — 186×61 → 222×51, the S visibly squeezed from the top. | 186×61 → 100×53. Not a squeeze; a wind. |
| L7 | **A mid-body lying along the ground** (segments 10–14 at 4.9–9.3°) — flat and readable, even though it was a rail. | Segments 10–13 at 180°–280.5°. Whatever else that is, it is not planar-readable at 240p. |
| L8 | **Modest, safe deform.** `kSpringBodyFlattenQ16 = 20000` (~31%), `Spread = 8000` (~12%). | 31500 (~48%) / 13800 (~21%), plus a **declared self-intersection** — `kSpringCoilFormationPressFullMm = 52`, `PressMicroMm = 96` — that exists only because the coil cannot form without the front pressing through the rear. Thirteen needed no such allowance. |

**Head-travel caution for the architect.** The D22 shipped note claims the coil
carries the head *"about 1.3 m back"*. On screen, in the fixed side view, the
live head's centroid ends the arming **~6 px FORWARD** of idle (x 225.5 → 231.7),
while Thirteen's moves **~5–9 px backward** (225.5 → 216–220) and ~5 px up. The
world-space number is real and the screen read contradicts it, because the head
finishes at the *front* of the knot. Direction 23 says "slightly backwards" and
the owner is reading the screen: **author and check the head's backward travel in
the fixed side view; do not accept a world-space metre count as evidence.**

---

## 6. GAINED — real fixes since Generation Eleven that must NOT be reverted

| # | Fix | Landed in | Present in Gen Thirteen? |
|---|---|---|---|
| G1 | **The planted-support metres/millimetres unit bug.** `spring_anchor_offset` returned raw fx16 metres (`fxm(1000 mm) == 65536`) where every consumer spent millimetres, so `rx >> 16` discarded the whole compensation: the real (−83, −311) mm correction returned (0, 0). Station 14 was never planted — the **head** was pinned instead and the body swung forward under a fixed skull. That *was* the owner's "it leans FORWARD and destroys the S", and it made `kSpringDeclaredBiteMm` and the entire `kSpringOpenSupportLift` route dead numbers. | `65100652` (D20) | **YES** — the `OWNER DIRECTION 20, THE PLANTED-SUPPORT UNIT BUG` block is present verbatim in `a2f601ef`. Verified by grep across all four commits. |
| G2 | **The support route re-authored** once the plant was honest — the pre-fix numbers buried the loaded animal 130 mm against a 34 mm declaration. Plus `kSpringDeclaredLoadedBiteMm` as a named, probe-checked deep-press allowance, and `kSpringHoldLivingDriftMm` replacing a 1 mm frozen-hold gate that had been enforcing exactly the rigidity the owner rejected. | `3d911148` (D20) | **YES.** Thirteen carries `DeclaredLoadedBiteMm=60`, `HoldLivingDriftMm=70`. |
| G3 | **No front-segment rollover past vertical.** `kSpringCollapsedHeading[0]`: **164.8° → −8.2°**. Segments 0–4 drop from 164.8/156.6/137.3/109.9/79.7° to −8.2/2.2/4.9/6.0/7.7°. | `a2f601ef` (D21) | **YES — this IS the reference.** live keeps it (−10.0°); do not lose it again. |
| G4 | **The accepted airborne salto restored.** Verified by eye at frames 60 and 75: Gen Twelve is a tight inward spiral with the head buried; Thirteen and live show the same open ring. | `a2f601ef` (D21) | **YES**, and unchanged in live. |
| G5 | **No-clip contact by flatten-and-spread** rather than intersection (D21 §3), with the flatten explicitly declared as the contact relief. | `a2f601ef` (D21) | **YES** at 20000/8000. Live raised the values and then had to declare a press on top; keep the mechanism, keep Thirteen's magnitudes as the starting point. |

**Everything on this list already exists in `a2f601ef`.** Starting from
Generation Thirteen forfeits none of the genuine fixes. Only the D22 coil
choreography — the seating waypoint, the wind/unwind windows, the 280° mid-body,
the 48% flatten and the declared formation press — is discarded, and all of it is
exactly what Direction 23 withdraws.

---

## 7. What Generation Thirteen still gets wrong (D22's surviving faults)

Restoring it verbatim is not the answer. D22's four faults were real and D23 does
not withdraw them — it withdraws only the *extreme coil* as the cure:

1. **The rear straightens at the deepest compression.** Assembled segments 14–18
   are +8.2 / −24.7 / −52.2 / −71.4 / −79.7°; collapsed they are +9.3 / −15.4 /
   −34.1 / −26.4 / −8.8°. The tail **loses** curl on the way into the squeeze.
   Visible in the frame-33 stack: a long flat noodle with a hook on the front.
2. **The head does not travel back** — the neck hook curls tighter instead
   (collapsed 93/151/170° against idle 80/118/138°). D23 asks for **slight**
   backward travel; ~5–9 px of screen travel already exists, so the ask is a
   modest increase, not the 1.3 m rewrite.
3. **The squeeze presses straight down**, not down-and-back on the launch
   diagonal.
4. **It bottoms at ~64% of idle nose height** — the owner wants lower. Live
   reached 54% by winding; the height can instead be bought with
   `kSpringBodyFlattenQ16` and the collapsed table without leaving the plane.

Direction 23 also adds two things neither bank has:

5. **The arming should be slower still.** Thirteen's 28 frames is half the
   balance clip's ~55. The key budget (`kSaltoCompressEndKey = 16`) is identical
   across all three banks, so slowing the arming means either raising that key
   count or redistributing the route so the change spreads across all 16 keys
   instead of settling early.
6. **The arming must be genuinely SMOOTH** — the sharpened Direction 23's first
   acceptance test. Per §3b, Thirteen carries ~5× the balance clip's jerk. Even
   the reference needs re-easing before it will pass. This is the one item the
   architect must not assume comes free with the revert.

---

## 8. Recommended starting point for the implementer

```
git show a2f601ef6d9234e1550954cf3ce88b548602e9f5:tools/reel/zixxtrixx.h
```

Take from it: `kSpringAssembledHeading`, `kSpringCollapsedHeading`,
`kSpringAbsorbHeading`, `kSpringArmAbsorbAt=220`, `kSpringArmAssembledAt=400`,
`kSpringBodyFlattenQ16=20000`, `kSpringBodySpreadQ16=8000`, and the six
`kSpring*SupportLiftMm` values (−12 / −10 / −5 / −8 / −10 / −26).

Delete from live: `kSpringSeatingHeading` and `kSpringArmSeatingAt`, the whole
`kSpringCoilFormation*` wind/unwind/echo/press block, and the raised
flatten / spread / loaded-bite trio.

Keep from live: the unit fix, the probe, the salto, the landing route, and every
non-spring change.

Then apply Direction 23's four beats over a **longer and re-eased** arming, with
the rear carrying its own lobe, the head travelling **slightly** back, and the
whole descent happening together — checked, as D22 insisted, from a fixed
orthographic render of the collapsed pose, not from the table.

**Smoothness target, from the balance clip itself (§3b):** in the fixed side
view, over the arming window, keep max frame-to-frame *jerk* under roughly
2 px on the silhouette centroid, 4 px on the head centroid and 60 px² on the
silhouette area. Generation Thirteen is at 7.2 / 9.3 / 157; live is at
23.0 / 30.9 / 1168. Grade by jerk amplitude, never by reversal count — §3b
shows a reversal-count gate would reject the balance clip and pass the
rejected one.

---

## Evidence index (scratchpad, not committed)

`…/scratchpad/recon2/` — `sheet-g09..g13,live-1.png` (every frame of each bank),
`arming-g13-vs-live.png` (paired arming, G13 above LIVE below),
`g13-key-poses.png` / `live-key-poses.png` (3x nearest-neighbour key poses),
`wheel-g12-g13-live.png` (salto restoration proof), `seg-*.txt` (per-frame
bbox/area), `head-*.txt` (per-frame head centroid), `seg.py`, `head.py`.
