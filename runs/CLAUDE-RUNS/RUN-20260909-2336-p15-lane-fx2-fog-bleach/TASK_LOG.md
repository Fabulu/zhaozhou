# Task Log: RUN-20260909-2336 — LANE-FX-2, the fog bleaches the body

**Created:** 2026-09-09 23:36 UTC+02:00
**Status:** In Progress
**Lane:** `manafold-p15-fx/{zhaozhou,Upheaval}`

---

## Objective

`PASS-15-REVIEW.md` §1 holds the pass-15 publish. Make the shell read as **fog,
not bleach**, with the terminator back, without turning it off.

---

## Progress Timeline

### 23:36 — lane reset, run opened

- `git fetch && git reset --hard origin/main` in BOTH repos, as the first action.
  Three lanes had landed since the clone.
  - zhaozhou `9e0d5a33` → **`7f9a7861`** (LANE-ANTENNA, pass-15 QA, the by-eye review)
  - Upheaval `4324006` → **`55e42b3`**
  - Both trees were **clean** before the reset — checked first, nothing discarded.

### 23:38 — required reading

`PASS-15-REVIEW.md`, `PASS-15-FINDINGS-FX.md`, `OWNER-DIRECTION-11` §2.3,
`10-GATE-CHECKLIST` §0 + all 43, `09-ENGINE-GOTCHAS` all 22, `08-LIGHTING.md`.

### 23:40 — LOAD CENSUS (09-ENGINE-GOTCHAS §22), before anything heavy

| time | what was alive | lane | mine? |
|---|---|---|---|
| 23:40 | `zhao-reel-cel.exe` PID 23844 | **p15-qa** | no — do not touch |
| 23:47 | `g++`/`cc1plus` building `build-eyeneg` | **p15-qa** | no — **so my build waits** |
| 23:52 | `zhao-reel-cel.exe` PID 5280 | **p15-qa** | no — **so my render waits** |

Processes identified **by command line**, never by image name. Nothing killed.
The QA lane alternates build/render; I take the gap its build leaves.

### 23:52 — the mechanism, read rather than inherited

`manafold_fx.h:897 shell_paint` composites a straight lerp toward
`g_u02_shell_tint` at `alpha_at(dist)`:

    px[k] = (px[k]*(1000-a) + tint[k]*a) / 1000

with `peak_at = out_px + ann_px`, so **the profile's peak sits `ann_px` INSIDE
the silhouette** — at the shipped `kShellFogDepthPm=520` that is **0.52·R in from
the outline, the middle of the ball**, exactly as the review says. Below the
peak it decays over one more `ann_px` to `floor` and **holds that plateau across
the entire core**, so every interior pixel takes `560 × 180/1000 ≈ 10%` of
near-white `{255,214,232}`.

**Confirmed at the source, and both halves are faults:**
* the annulus is **not an outer layer** — it is most of the body;
* the plateau paints the core, where the terminator is.

**All six knobs already have env overrides** (`zhao_reel.cpp:7861–7930`), tint
included (`U02_SHELL_TINT=r,g,b`) — so **the whole ladder comes from ONE binary**
(item 26) and no rung needs a rebuild.

### 23:55 — build launched (`build-fx2a`, `--clean`)

`--clean` because every file in question is a **header**, and `build-direct.sh`
has no header dependency tracking at all (`09-ENGINE-GOTCHAS` §19). Exit code
captured to its own `.rc` file — **never a pipeline's status** (CLAUDE.md).

⚠ **The live-tree trap:** the build reads the working tree, so **no source edit
until it returns.**

---

## The plan, written down BEFORE the first plate

**Order of work, and why the ladder comes before the edit.** Every knob has an
env override, so the shipping constants are laddered *without touching them*:

1. build the current tree → **one binary**;
2. ladder profile, then tint, from that binary, on **both backdrops**;
3. pick **by eye** at native and 3×, against a **pass-14 control**;
4. only then edit the constants;
5. rebuild `--clean` and prove the shipped default frame is **byte-identical**
   to the env rung that was picked — that is the cheap proof the edit landed and
   matches what was judged;
6. re-aim `mshell`, and check the re-aim still means something.

**The controls, and they are two different instruments:**
* **P14 PUBLISHED** — `website/public/renders/manafold-inspect.webm` f0300 through
  `webm2rgb.py`. A **different binary**: labelled a PAIR member, never a ladder
  rung (item 26). This is the terminator comparison the brief asks for.
* **SHELL OFF** (`U02_SHELL_ALPHA=0`) — the **ABLATED MECHANISM**, same binary
  (item 43). The review says this leg is unrun and is the first thing to do.

**Hypothesis, stated so it can be refuted.** The tint's **G channel (214)** is
the bleach lever: over a plum terminator (G≈30) a 10% lerp toward G=214 lifts G
by ~60% *relative*, which is desaturation, while R and B move much less. A tint
with G dropped should fog at the same alpha. **This is a bias-finder, not a value
chooser** (art law rule 4) — the shipped number gets picked by looking.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status |
|-----------|----------|---------|--------|
| 23:42 | `a5e72b6…` | READ-ONLY recon of build/render/plate tooling; explicitly forbidden from building, rendering or writing | dispatched |

---

## Files Created

- `SPEC_v1.md`, `TASK_LOG.md` (this file)

### 23:44 — the one binary

`build-fx2a`, `--clean cel`. **`BUILD_RC=0` read from the build's own exit
code**, captured to its own `.rc` file, never from a pipeline. The log's last
line is `build-direct: done` **after** the `LD` line — which is the tell that
matters, because `LD zhao-reel-cel` prints *before* the link and a lock failure
would leave it as the last line (`09-ENGINE-GOTCHAS` §13).

    md5 a5f028ffb56251c977ba8e10b0c6a3c9

**Every ladder rung below comes from this binary.**

### 23:45 — LOOKED at the review's own plates, before rendering anything

`J01` and `J02`, at native in the plate. The review's words are right and there
is one thing they do not say that the picture does:

**the wash is heaviest in the MIDDLE of the ball and the rim is comparatively
clean.** That is the profile's peak at 0.52·R made visible — the fog is not
merely too strong, it is **inside out** relative to the sentence it was built
from. In `J02` the lower body carries a hard, wandering horizontal seam: the
decay-to-plateau transition, landing across the body as a tidemark.

### 23:47 — the P14 control, decoded in-lane

`webm2rgb.py --frames 300` on `website/public/renders/manafold-inspect.webm`
(and `channel` f180) — **inside my own lane**, so no other lane's tree is
touched. `rgbframe.py selftest: PASS` was run before any frame was read.

### 23:50 — WHICH CHANNEL IS THE BLEACH LEVER (comparison-side measurement only)

Sampled the P14 control's own pigment and applied the composite's actual lerp —
`px*(1000-a) + tint*a` — at **a = 100 pm, i.e. the core plateau alone**:

| where | P14 pigment | → {255,214,232} shipped | → {255,120,205} | → {235,70,170} |
|---|---|---|---|---|
| terminator (dark left) | (88,26,72) **sat 70** | (104,44,88) **sat 58** | (104,35,85) sat 66 | (102,30,81) **sat 71** |
| mid body | (78,22,57) sat 72 | (95,41,74) **sat 57** | (95,31,71) sat 67 | (93,26,68) sat 72 |
| lit right | (184,52,106) sat 72 | (191,68,118) sat 64 | (191,58,115) sat 70 | (189,53,112) sat 72 |

**The terminator loses 12 points of saturation from the plateau ALONE**, before
the annulus is reached. The lever is **G**: the shipped tint lifts the
terminator's G by **+69%** (26→44); a mana magenta lifts it **+15%** (26→30).
And all three still raise R, so the gas is still *present* — it lightens without
killing the chroma.

⚠ **This is a BIAS-FINDER, not a value-chooser** (art law rule 4). It says *G is
the axis*; it does not say which magenta. The shipped number gets picked by
looking at it in scene, at 240p, on both backdrops.

### 23:52 — `U02_SHELL_INK` added (`manafold_fx.h`, `zhao_reel.cpp`)

`kShellOverInkPm` was the **one shell axis with no env override**, so the only
way to ask a question about it was a rebuild. The new profile moves the fog's
peak off the middle of the ball and onto the **rim** — the ink's own doorstep —
which is precisely when that ratio starts to matter. `g_u02_shell_over_ink_pm`
+ `U02_SHELL_INK`, same shape as the other five.

**And the rung banner now prints the TINT and the ink ratio.** It printed
neither, while the tint was the axis the previous lane's own findings called
*"fog versus bleach"* — a ladder log that cannot say what colour it rendered is
`10-GATE` item 38 in a log file.

### 23:58 — THE ABLATION LANDS. The shell is the bleach, and now it is PROVED.

`fx2-work/ablation-check.png` — three tiles:

| tile | what it is | the body |
|---|---|---|
| P14 PUBLISHED | the shipped pass-14 clip, **a different binary** — labelled so | deep plum left, magenta right, **round** |
| **P15 SHELL OFF** | **`U02_SHELL_ALPHA=0`, MY binary, one variable** | **the terminator is BACK, in full** |
| P15 SHIPPED | `560/520/180`, my binary | chalky pale mauve, **form gone** |

**This is the leg `PASS-15-REVIEW.md` declared unrun** — it says plainly *"I did
not ablate it; that leg is unrun, and it is the first thing pass 16 should do."*
Its own `J01`/`J02` are **cross-binary** and therefore isolate *the pass*, not
the mechanism (item 26, which the review states about itself, correctly).

**This isolates the mechanism.** Same binary, same frame, same rig, one variable
— and the bleach appears and disappears with `U02_SHELL_ALPHA`. So:

* the bleach is **the shell**, not the lighting, not LANE-EYE, not LANE-ANTENNA;
* pass 15's body pigment and terminator are **undamaged underneath** — with the
  shell off, the ball reads as round as pass 14's and arguably crisper;
* everything the other lanes landed is visible and intact in the ablated tile
  (the bolt is bright and blue, the lens stands proud and is deep violet).

⚠ **And SHELL OFF is not the answer** — it is the control. The owner has asked
for this shell since Direction 5 and it must be VISIBLE. The target is the
ablated tile's **terminator** *plus* gas at the **rim**.

### 00:02 — a second failure mode, found at 3x before it could be shipped into

`fx2-work/peek-300.png`, rungs OFF / SHIPPED 520 / SHELL 300 at 3x on the body.

`SHELL 300` is better than shipped on the lit side and **still washed on the
terminator** — but the useful finding is the new artefact: **a bright ARC
through the middle-left of the ball.** The distance transform's bands run
parallel to the silhouette, so a narrow, dense annulus does not read as a rim of
gas — it reads as a **concentric RING drawn inside the outline.**

**So the axis has a failure mode at BOTH ends**, and this is why "less fog" has
never been the whole answer:

    annulus WIDE   -> a veil over the whole body   (the shipped fault)
    annulus NARROW + dense -> a hard ring          (the fault waiting on the other side)

**`kShellRiseGamma` is the knob that decides between rim and ring, and I had not
planned to sweep it.** `t` runs 0 at the gas's outer edge to 1000 at the peak;
gamma 2000 is quadratic, so alpha stays near zero across the skirt and leaps up
only at the peak — **a band**. Gamma 1000 is linear and rises steadily from the
outer edge inward — **a soft rim**. The shipped **1600 is ring-leaning**, and
nothing in four passes has ever moved it.

That goes into the next ladder. Noted here rather than acted on, because the
current ladder must finish before its own conclusion is read.

### 00:03 — `build-fx2a` is the bit-identity BASELINE, and must not be overwritten

It was built from `origin/main` **before** I edited a single file, by me, in this
lane. That makes it exactly what item 27 asks for — *a baseline you built
yourself* — for the Zixxtrixx proof I owe because I touched `zhao_reel.cpp`.
The post-edit binary goes to a **different output directory**.

### 00:08 — THE BOLT FAULT, diagnosed at the source while the ladder rendered

The review's two lightning failures — `taunt3` f0288–f0352 **broken into
capsules** and `channel` f0250 **fused into a solid mass** — are described as
*"opposite failures of the same mechanism."* They are, and the mechanism is one
ternary. `manafold_fx.h:2507`:

```cpp
int nst = strand ? per_seg
                 : static_cast<int>((adx + ady + adz) / stamp_mm);
```

* the **green fold** (the path he likes) subdivides each segment by **DISTANCE**
  — constant stamp density in space, whatever the segment's length;
* the **strand** (the lightning) subdivides by a fixed **COUNT**,
  `kFoldStrandPerSeg = 6`, *regardless of how long the segment is.*

So stamp spacing is proportional to segment length, and both failures follow
directly:

| clip | what the deform does | 6 stamps then | what you see |
|---|---|---|---|
| `taunt3` f0288–f0352 | the loop is most strongly deformed, segments LONG | spread far apart | **gaps — capsules with rounded caps** |
| `channel` f0250 | segments SHORT | pile into a few pixels | **additive saturation — a fused lozenge** |

**The rounded caps are the tell the review already spotted**, and they are what a
stamp looks like when its neighbour is too far to overlap it.

⚠ **The fix is already in the file, on the other branch of that same ternary**,
and — the part that matters for the protected work — **it cannot touch the green
fold's byte-identity.** The comment above the line explains the count path as
preserving *"the pass-13 outline… byte for byte, so `ZHAO_U02_STRAND=0` still
reproduces the shipped picture exactly"* — but that guarantee lives in the
**`: else`** branch, which no change to the `strand ?` branch can reach. The
`f99c9722…` hash is safe from this fix by construction.

**Declared, not built.** The fog is what holds the publish, the lightning at its
best is protected work the review calls *"excellent"*, and destabilising it to
chase a second fault in the same pass is how a lane loses both. Recorded here
and in the findings with the line number, so pass 16 starts at the mechanism
instead of at `perSeg`'s value — which is the knob four passes would otherwise
reach for, and `09-ENGINE-GOTCHAS` §18 is exactly about that.

### 00:14 — ladder 1 read (profile axis), 3x on the body, `inspect` f0300

| rung | reach/alpha/floor/out | the read |
|---|---|---|
| SHELL OFF | ablated | the target FORM: deep plum core, crisp terminator |
| P15 SHIPPED | 520/560/180/55 | milky over the whole ball; **form gone** |
| SHELL 300 | 300/560/80/110 | core returning; a broad pale ARC still crosses the body |
| **SHELL 200** | 200/600/0/130 | **core dark plum, terminator BACK**; gas is a wide soft rim |
| SHELL 130 | 130/640/0/150 | terminator back; rim tighter, reads more like a white OUTLINE |

**Two conclusions, and the second is the one that matters.**

1. **The profile fix works.** Dropping the annulus to 130–200 pm and the floor to
   **0** restores the terminator completely — at 200 the core is indistinguishable
   from the ablated tile — while leaving plainly visible gas at the rim. So the
   shell can be VISIBLE and the pigment can SURVIVE; they were never the trade
   the last four passes assumed.

2. ⚠ **What is left is entirely the TINT.** At both 200 and 130 the rim is a
   **chalky WHITE-pink band**. It reads as a glowing outline stuck on the animal,
   not as the animal's own gas. Moving the fog to the right place stopped it
   bleaching the *core* and did nothing about it being the wrong *colour* where
   it now lives.

**So the review and the previous lane were each half right, and they are
separable axes:**

* the review — *"the profile is the fault, not the hue"* — is right that the
  profile is what destroyed the FORM, and right that it had to move first;
* the FX lane — *"the axis that decides fog vs bleach"* — is right that the tint
  is what decides whether the gas reads as gas, and that is now the whole
  remaining fault.

**Neither alone would have fixed this.** Fixing only the hue would have left a
coloured veil over the terminator; fixing only the profile leaves the white ring
above. That is why "less fog" failed for four passes — it was one knob on a
two-axis fault.

**Ladder 2** therefore sweeps the tint as a **single-variable** ladder at the
picked profile, on `channel`'s violet night — plus one gamma rung, because the
rim's hardness is the other thing the eye is objecting to.
