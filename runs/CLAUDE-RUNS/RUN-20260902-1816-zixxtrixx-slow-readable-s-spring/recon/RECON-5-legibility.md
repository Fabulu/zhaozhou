# RECON 5 — Why the accepted clips READ and the spring does not

**Run:** RUN-20260902-1816-zixxtrixx-slow-readable-s-spring
**Scope:** read-only. Nothing in either repo was modified except this file.
**Evidence:** every frame of 20 published banks extracted and measured; contact
sheets, zoom sheets, mask overlays and per-frame plots in
`…/scratchpad/recon5/out/`. Scripts in `…/scratchpad/recon5/*.py`.

Owner's clarification, mid-recon: *"with you can't see what it's supposed to be
I meant not shape but the movement. It's so jittery and fast there's no
smoothness. But yeah, sure, shape too."* This report leads with motion.

---

## VERDICT UP FRONT

**There are two separate failures, and they need two separate fixes.**

1. **PACE — the whole jump family fails, the live bank worst.** Measured against
   the balance clip, *no* jump bank ever authored has been slow enough to read.
   Balance replaces its silhouette on a **37-frame** half-life; the live spring's
   arming does it in **5**. This is the owner's "too fast, no smoothness", and it
   is the fault that has survived three directions. It is **not** fixed by
   reverting — Generation Thirteen's arming half-life is 7 frames.
2. **SILHOUETTE — only the live bank fails.** For 17 frames on the ground the
   animal is a solid knot: convex solidity 0.85, an enclosed hole 25% of its own
   area, head 0.05 body-lengths from the tail, visible spine collapsed to 44% of
   its median. Generation Thirteen never does this, and neither does any
   accepted clip.

So: **the reference bank already satisfies the shape criteria, and the balance
clip's pace does not exist anywhere in the jump.** An architect who only repairs
the coil will ship another too-fast spring.

**Note on banks.** The live published `zixxtrixx-jump-one.webm` (md5
`6ee4a61d…`) is *not* `archive-2026-09-02-generation-thirteen-jump-one.webm`
(md5 `bf02c03a…`). Live = the rejected D22 coil. Gen Thirteen = the D21
squeezed-S, the bank Recon 2 identifies as "the reference implementation". Both
were measured; they are labelled **LIVE** and **REF** below.

---

## 0. Two measurement traps I walked into (declared, so nobody repeats them)

* **Background subtraction is unusable here.** A per-pixel median plate looked
  right and produced confident numbers — but most clips have a *moving camera*
  (the horizon row travels 89 to 163 px in jump-one, 81 to 164 in walk), so the
  plate masked in a wide band of terrain. Solidity, hole and change numbers from
  that pass were all wrong. Every number below comes from a **colour**
  segmentation (`lum<52 | G>R+10 | B>R+10 | magenta highlight`), verified by eye
  against frame-beside-mask overlay sheets.
* **Whole-clip statistics hide the fault.** Jump clips are half flight. The
  whole-clip half-life is 8 frames for REF and 8 for LIVE, because the airborne
  tuck dominates both. Everything discriminating lives in the **arming window,
  frames 1–50**. QA must window; a whole-clip gate will pass the knot.

---

## 1. Survey — the accepted clips (full clip, colour segmentation, 384x240)

| clip | frames | shape %/frame med / p90 / max | pose half-life med / min | frames HL<10 | solidity max | hole % of area max | closure min |
|---|---|---|---|---|---|---|---|
| **balance** | 493 | **4.4 / 10.2 / 28.9** | **32 / 4** | 5.9% | 0.58 | 4.4 | **0.49** |
| idle | 576 | 5.0 / 9.6 / 20.2 | 34 / 12 | 0% | 0.88 | 6.4 | 0.25 |
| walk | 160 | 6.9 / 10.7 / 15.4 | 37 / 26 | 0% | 0.73 | 7.0 | 0.54 |
| look | 384 | 2.0 / 4.1 / 5.9 | 54 / 30 | 0% | 0.85 | 3.0 | 0.48 |
| taunt | 224 | 6.5 / 10.2 / 12.3 | 32 / 11 | 0% | 0.75 | 2.0 | 0.57 |
| slow-taunt | 239 | 2.4 / 3.8 / 6.1 | never halved | 0% | 0.76 | — | — |
| run | 96 | 14.1 / 21.3 / 27.8 | 15 / 6 | 12.5% | 0.69 | — | — |
| salto-six | 253 | 29.9 / 55.9 / 82.6 | 3 / 1 | 70% | 0.72 | — | 0.10 |
| salto-nine | 355 | 28.4 / 54.1 / 92.8 | 4 / 1 | 74% | 0.96 | — | — |

*Definitions.* **shape %/frame** = translation-compensated XOR/union between
consecutive silhouettes (camera- and travel-invariant; pure articulation).
**pose half-life** = frames until translation-compensated IoU with frame *i*
drops below 0.5 — how long a pose survives on screen. **closure** =
(head tip to tail tip distance) / (skeleton arc length): 1.0 straight, 0.0 head
touching tail; invariant to camera scale. **solidity** = silhouette area /
convex hull area.

**The split is not "accepted vs rejected", it is "ground vs air".** The ground
clips the owner blesses — balance, idle, walk, look, taunt — all sit in one
narrow band: **shape-rate p90 at or below 10.7 %/frame, half-life at or above 11
frames, not one frame under 10.** The salto clips sit in a completely different
regime (half-life 1–4 frames, shape-rate p90 55%) and the owner accepts them
anyway, because they are *flight* — a body rotating in the air reads as one
gesture even when the silhouette churns. **The spring's arming is a ground beat
and is currently animated at flight rates.**

---

## 2. The arming window, bank by bank (frames 1–50, the only fair comparison)

| bank | half-life med | % frames HL<10 | shape %/f med | shape %/f p90 | jolts/s | solidity max | hole% max | closure min | spine min/med |
|---|---|---|---|---|---|---|---|---|---|
| balance (whole clip, the model) | **37** | **0%** | **4.6** | **8.3** | 4.1 | 0.58 | 4.4 | **0.49** | 0.68 |
| Gen Nine | 7 | 66% | 9.5 | 29.1 | 3.6 | 0.59 | 4.2 | 0.47 | 0.91 |
| Gen Ten | 19.5 | 22% | 6.5 | 16.5 | 3.6 | 0.60 | 4.3 | 0.71 | 0.78 |
| Gen Eleven | 13.5 | 42% | 7.4 | 54.0 | 6.0 | 0.63 | 6.1 | 0.47 | 0.71 |
| Gen Twelve | 6 | 80% | 13.1 | 42.9 | 3.6 | 0.59 | 4.9 | 0.19 | 0.82 |
| **REF (Gen Thirteen, D21)** | 7 | 62% | 12.4 | 46.9 | 4.8 | **0.65** | **4.5** | **0.42** | **0.73** |
| **LIVE (D22, rejected)** | **5** | **84%** | **17.1** | **56.5** | **8.4** | **0.85** | **25.1** | **0.05** | **0.44** |

*jolts/s* = local maxima of the shape-rate curve with prominence at or above
4 %/frame, per second — how many separate jerks the arming contains.

Read the table twice.

* **Down the shape columns** (solidity, hole, closure, spine) REF sits inside
  the balance band on all four. LIVE is outside on all four, by 1.4x to 8x.
  Every earlier bank is also inside. **The coil is a new, isolated regression.**
* **Down the pace columns** (half-life, shape rate, jolts) *every* bank is far
  outside the balance band, REF included. Balance never has a frame with a
  half-life under 11; REF has 62% of its arming under 10 and LIVE 84%. **The
  arming has never once been animated at ground pace.**

---

## 3. Motion smoothness, measured

Tracked quantities are camera-invariant: head position **relative to the
silhouette centroid** (isolates articulation from camera and body travel) and
the translation-compensated shape rate.

| clip | head-rel speed med / p90 (px/f) | head-rel accel med / p90 | shape rate med / p90 / max |
|---|---|---|---|
| balance | 0.41 / 1.15 | 0.50 / 1.21 | 4.4 / 10.2 / 28.9 |
| look | 0.32 / 0.83 | 0.38 / 0.81 | 2.0 / 4.1 / 5.9 |
| walk | 1.02 / 1.72 | 0.49 / 1.05 | 6.9 / 10.7 / 15.4 |
| taunt | 1.14 / 4.21 | 0.92 / 3.44 | 6.5 / 10.2 / 12.3 |
| REF arming | 1.72 / 9.20 | 0.62 / 4.42 | 12.4 / 46.9 / 58.3 |
| LIVE arming | 2.04 / 9.51 | 0.69 / 5.33 | 17.1 / 56.5 / 62.4 |

**The most legible signature of smoothness is the shape-rate curve's *shape*,
not its level** (`out/plots-balance-jump-one-salto-six-walk.png`):

* **balance** — four or five broad humps, each rising over 40–60 frames to a
  peak of 10–18 %/frame and decaying, separated by long floors at about
  1 %/frame. Speed varies slowly. That is what "organic" looks like on a plot.
* **LIVE** — a picket fence: five spikes to 45–62 %/frame inside frames 10–50,
  each 3–6 frames wide, with gaps of 3, 4 and 6 frames between jolts. Nothing is
  sustained; nothing settles.

**On direction reversals.** Recon 1's 18 per-station reversals come from the 3D
pose tables and are the right measurement. At 384x240 the image-domain head
track is floored by plus/minus 1 px quantisation (head-rel median speed is
0.4–2 px/frame in *every* clip), so reversal counts computed from renders are
noise — LIVE scores 1.86/s and balance 3.65/s, which is meaningless. **Take
reversals from the pose tables; take rate and half-life from the render.** Do
not gate reversals on pixels.

**On secondary layers.** Any travelling wave, per-station lag or damped bounce
whose period is under about 12 frames at 60 Hz will read as noise, not as life:
it falls below the pose half-life floor the accepted clips establish (11 frames)
and is indistinguishable from jitter at native resolution. Recon 1's finding
that `kBalBuckleLagK = 6` belongs to the topple and not to the rise supports
this — the beat the owner is pointing at carries **no** lag layer at all.

---

## 4. Silhouette — where the LIVE spring loses its read, by name

Runs of consecutive frames outside the accepted band (60 Hz frame numbers as
published; key = (f-1)/2):

| condition | LIVE (rejected) | REF (Gen Thirteen) |
|---|---|---|
| solidity > 0.75 (blob) | **f22–f38** | none |
| enclosed hole > 10% of area | **f22–f24, f26–f28, f30–f38**, f51–f115 | f51–f116 only |
| closure < 0.35 (head near tail) | **f23, f25, f27, f29**, f51–f67, f97–f119 | f51–f67, f97–f116 |
| visible spine < 0.6 x median | **f22, f24–f35, f37–f38** | none on the ground |
| half-life 3 frames or less | **f15–f22, f36–f42, f44–f52** | f52–f60 |
| shape rate > 25 %/frame | **f18–f24, f38–f54** | f43–f45, f54–f62 |

**The single named fault: frames 21–39.** Verified by eye at 2x on
`out/zoom-jump-one-14-42.png`. At f14–f20 the animal is an open, readable S with
the head at the right and the fins extended left. At **f22** the tail has swung
up and over, the fins fold to a stub, and the body becomes a compressed hook
whose runs overlap. f22–f38 it stays a knot. At **f40** it snaps back open.
Entered in 2 frames, held 17, left in 2 — 0.28 s in which the animal is not an
animal. Recon 2 reached the same window independently by a different route
("winds into a knot in 5 frames, sits as an unreadable blob for 16 frames, snaps
back open in 2").

**Frames 51–116 are a closed ring — and that is fine.** REF does it too, and so
does every accepted salto. A ring in the air, rotating, reads as a tuck. The
fault is a ring *on the ground, during the arming*, where nothing else is
telling the viewer what is happening.

---

## 5. Pace, derived from the clips that work

Beat segmentation on the shape-rate curve (activity above the clip's own 35th
percentile, runs of 6 frames or more):

| clip | beat lengths (frames) | settle gaps (frames) |
|---|---|---|
| taunt | 19, 17, 18, 21, 19, 17, 18, 19 | 4, 5, 15, 17, 4, 5, 15 |
| walk | 22, 16, 19, 8, 28, 29 | 2, 3, 3, 22, 2 |
| balance | 19, 130, 25, 31, 67, 61 | 13, 6, 24, 3, 8, 29 |
| look | 18, 69, 21, 18, 28, 21, 20, 62 | 14, 36, 37, 7, 5, 32, 2 |
| LIVE arming | one 20-frame run then an 87-frame run — no beats resolve | — |

**Nothing in the accepted vocabulary changes shape in fewer than 16 frames.**
The shortest beat measured anywhere the owner has blessed is 16–17 frames
(0.27 s); the typical one is 19–31 frames (0.32–0.52 s); the ones he calls out
by name (the balance rise) run 61–130 frames. Recon 1 measures that same rise
from the tables as 49 keys = 98 frames.

**Derived requirement.** A beat registers when the silhouette's half-life across
it stays at or above **16 frames**, and it is followed by a settle of at least
**6 frames** in which the shape rate returns below about 3 %/frame. Direction 23
asks for four beats. The floor is therefore

> **4 x (20 travel + 8 settle) = about 112 frames = 1.9 s of ground time before
> the launch**, and at balance pace — the thing actually asked for —
> **150–180 frames = 2.5–3.0 s.**

The LIVE and REF armings are **50 frames**. There is no arrangement of four
readable beats inside 50 frames. **The clip has to get longer** — roughly
260–300 frames total against today's 161 — or beats have to be merged. This is a
duration decision the architect must make explicitly; retiming inside 161 frames
cannot satisfy Direction 23.

---

## 6. Contrast and lighting — measured, and NOT the discriminator

Weber contrast between the lit body and the 5-px background ring, Cool Cross rig:

| phase | LIVE | Gen Nine (same rig) | balance |
|---|---|---|---|
| on the ground, against terrain | 0.49–0.74 | 0.63–0.68 | 0.32–0.94 |
| airborne, against sky | **0.008–0.03** | **0.016–0.10** | n/a |

**The figure/ground collapse in flight is real and severe** — the body's mean
luminance (115–119) is within 1 to 4 of the sky's (113–122), so at the ring
frames the animal is separated from its background by its black outline alone.
**But it is a pre-existing property of the airborne phase, shared by the
accepted banks, so it does not explain the rejection and must not be blamed for
it.** (Measurement removing a bias, not choosing a value.)

What it does mean: **the arming has to carry the read, because it is the only
part of the clip with real figure/ground contrast, 0.5–0.7 against the dirt.**
Spending those frames on an unreadable knot spends the only legible part of the
shot.

One lighting note that *is* actionable: the **internal contour fraction** —
black outline pixels interior to the silhouette rather than on its boundary — is
**20–24% in balance** and **45–52% in every jump bank**. In balance the outline
does one job: separating the animal from the ground. In the jump, half the ink
is drawing self-overlap, which the eye must resolve at 384x240 on a tube about
10 px thick. Fewer crossings is not a stylistic preference here; it is what
keeps the contour doing its job.

---

## 7. CRITERIA — what QA checks, what the architect designs toward

Applied to the **arming window only** (frame 1 to the first frame of ground
release), on the shipped 384x240 webm, using the colour segmentation of section
0. Every threshold is the accepted clips' own measured range, not a theory.

### A. Motion smoothness (lead axis — these have never passed)

| # | criterion | threshold | today |
|---|---|---|---|
| A1 | **Pose half-life** at or above 16 frames on every arming frame | balance min 16 over f1–200; nothing accepted is under 11 | FAIL, 5 |
| A2 | **Shape rate** median at or below 7 %/frame, p90 at or below 12 %/frame, **no frame over 20 %/frame** | balance 4.4 / 10.2 / max 28.9; walk 6.9 / 10.7 / 15.4 | FAIL, 17 / 56 / 62 |
| A3 | **Jolts** (shape-rate maxima, prominence 4 %/f or more) at or below 5 per second, and **no two jolts closer than 8 frames** | balance 4.1/s, REF 4.8/s | FAIL, 8.4/s with gaps of 3, 4, 6 |
| A4 | **Four beats resolve**: the shape-rate curve segments into exactly four activity runs of 16 frames or more, separated by settles of 6 frames or more below 3 %/frame | taunt 8 x (17–21); balance 19–130 | FAIL, nothing segments |
| A5 | **Direction reversals** per centreline station at most 2 across the arming, taken **from the pose tables, not the render** | Recon 1: balance rise 0, spring 18 | FAIL |
| A6 | Any secondary layer (wave, lag, bounce) has period **12 frames or longer**; anything faster reads as noise | the half-life floor | check by construction |

### B. Silhouette readability (second axis — REF passes, LIVE fails)

| # | criterion | threshold | today |
|---|---|---|---|
| B1 | **Convex solidity at or below 0.70** on every arming frame | balance 0.58, REF 0.65 | FAIL, 0.85 |
| B2 | **Enclosed hole at or below 6% of silhouette area** on every arming frame — no closed ring on the ground | balance 4.4, REF 4.5 | FAIL, 25.1 |
| B3 | **Closure at or above 0.40** on every arming frame (head tip and tail tip at least 0.40 body-lengths apart). This *is* Direction 23's "never approaches the tail", made checkable | balance min 0.49, REF 0.42 | FAIL, 0.05 |
| B4 | **Visible spine at or above 0.65 x its own median** — the body compresses, it does not vanish | balance 0.68, REF 0.73 | FAIL, 0.44 |
| B5 | **Head locatable in every frame** — the eye blob segments as one component, at least 8 px clear of any other body run | never lost in any accepted clip | check |
| B6 | **Internal contour fraction at or below 35%** of dark pixels | balance 20–24%, jumps 45–52% | stretch |

### C. Composition

| # | criterion | threshold |
|---|---|---|
| C1 | The arming stays on the ground, where Weber contrast is 0.4 or better; beats 1–3 are not spent silhouetted against sky | 0.49–0.74 on dirt vs 0.01–0.03 on sky |
| C2 | Ground time before release at least 112 frames, target 150–180 | section 5 |

**How to use these.** B1–B4 are hard gates: a single arming frame outside them
is a fail, and they are cheap to run. A1–A4 are the ones that decide whether the
owner says yes; they cannot be met by editing the coil, only by re-timing the
whole arming and lengthening the clip.

**And the standing law applies to all of it:** these are comparison-side
numbers. Not one of them chooses a pose, a radius or a heading. The pose is
authored by eye, rendered, looked at in motion at 384x240 beside the balance
clip, and adjusted — and *then* these gates catch what the eye missed.

---

## 8. Diagnostics worth producing (and committing)

1. **`legibility-probe`** — one committed tool, per clip, emitting a CSV of:
   frame, silhouette area, convex solidity, enclosed-hole %, closure ratio,
   spine arc length, translation-compensated shape rate, pose half-life,
   head-blob found, Weber contrast. Every criterion in section 7 is a threshold
   on one column. **Its segmentation must be colour-based** — the median-plate
   version silently mixes in terrain whenever the camera moves (section 0).
2. **Four-panel time-series plot per clip**: shape rate, pose half-life,
   solidity plus hole fraction, closure. Publish it on the same axes as the
   balance clip's. The picket-fence-versus-broad-humps difference is visible at
   a glance and is the single most useful image in this report.
3. **Full-frame contact sheet of the arming**, every frame, no sampling. The
   knot at f21–39 is invisible in an 8-frame sample and obvious on a full sheet.
4. **Centroid-locked 2x zoom sheet** of the arming — what made the
   fins-folding-to-a-stub read visible.
5. **Frame-beside-its-mask overlay sheet**, spot-checked every pass. This is
   what caught the terrain contamination; without it this report would have
   shipped confident, wrong numbers.
6. **Beat segmentation printout** — the run lengths and gaps of section 5. If it
   does not print four runs, the four beats do not exist on screen, whatever the
   pose tables say.

---

## 9. For the architect, in one paragraph

Undo the coil and you are back at Generation Thirteen, which already satisfies
every silhouette criterion in 7B and which the owner still called too fast. The
work is the *pace*: four beats, each at least 16 frames of travel followed by at
least 6 frames of settle, with the silhouette never more than half-replaced
inside 16 frames — which is what the balance clip does and what no jump bank has
ever done. That does not fit in 161 frames, so the clip gets longer. Keep the
head tip at least 0.40 body-lengths from the tail tip throughout, keep convex
solidity under 0.70, and let the compression shorten the visible spine to no
less than 0.65 of its own median: those four numbers are the whole of "slightly
backwards", "compress", and "you can still see what it is", stated so a gate can
check them.
