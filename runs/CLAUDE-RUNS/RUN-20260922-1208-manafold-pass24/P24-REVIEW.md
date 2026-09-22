# Manafold pass 24 — independent review

**Date:** 2026-09-22
**Reviewer:** Claude (independent; no sub-agents, no Qwen, no HomeAI)
**Under review:** Zhaozhou `manafold-pass24` @ `8e5a3ee3`, Upheaval `manafold-pass24`
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-25-2026-09-22.md`

---

## VERDICT: **PASS** — clear to publish.

Every substantive claim in `P24-IMPLEMENTATION.md` reproduces on binaries I built
myself, most of them to the digit. The reported contradiction is not one — it is
two different selectors, and the owner's comparison is real as built. The
avoidance works and does not restyle; the splitting genuinely does nothing, which
is the pass's most useful result. The eye layer is subtle and stays in the lenses.

**One item is honestly not delivered and the site copy must say so:** Hover's
*back ball* is not visibly calmer. The implementer said this first, loudly, and
measured it; I confirm it and I have now measured the lever that would deliver it.

---

## 0. What I built and ran myself

Nothing below is quoted from the implementer's receipts unless it says so.

* `tools/reel/build-direct.sh --output .tmp/p24rev --clean`, g++ 16.1.0
  (MinGW-W64 x86_64-ucrt-posix-seh) from the `zhao-env.ps1` toolchain.
  **17 binaries, RC 0.** No CMake was used and no CMake result is claimed.
* Production ink on every render and every gate: `ZIXX_EXP=celmain`,
  `ZIXX_LIGHT=diagonal-cool-cross`.
* **Two complete 22-subject banks** (shipping and all-off), plus isolated
  single-layer renders for each of the four knobs, so every A/B below differs in
  exactly one thing.
* Frames read with the committed `rgbframe.py`; plates built with the committed
  `plates.py`; change locations found with the committed `framediff.py`. No new
  frame reader was written.

**The strongest single result:** my independently built renderer reproduces the
implementer's shipping bank **byte-for-byte on 22 of 22 subjects** (`diff` clean
against `P24-RECEIPTS/crcs-ship.txt`).

---

## 1. THE MECHANISM ASSIGNMENT — SETTLED. There is no contradiction, and no fix is needed.

The brief flagged two implementer statements that "cannot both be true". They can:
**they are about two different selectors.**

| item | selector | lives on | can it tell Hover from Inspect? |
|---|---|---|---|
| **1** — rear ambient, Front gain | per **CLIP SLOT** (`kRearAmbientClipPm[24]`, `kFrontFlexClipPm[24]`) | the clip | **No.** Hover and Inspect are both slot 0. |
| **2** — the lightning mechanisms | per **SUBJECT** (`SceneSubject::u02_bolt_avoid_rods`, `::u02_bolt_split_n`, zhao_reel.cpp 1104-1105 → 4098-4099) | the subject | **Yes.** |

The source says exactly this at `zhao_reel.cpp` 1090-1099: *"the selector lives on
the SUBJECT rather than on the clip slot because the bank cannot express it
otherwise … a per-clip one would have to pick one of the two."*

### What each shipped subject actually renders with — read from the source

| subject | slot | 3D avoidance | depth split N |
|---|---|---|---|
| `manafold-crackle` (9472-9479) | 23 | **ON** | 1 |
| `manafold-hover` (9062-9068) | 0 | **ON** | 1 |
| `manafold-inspect` (9070-9101) | 0 | off | **4** |
| the other 19 live subjects | — | off | 1 |

**This is exactly what Direction 25 asked for.** Avoidance on Crackle and Hover,
depth-splitting on Inspect, everything else untouched. The owner's comparison is
real. Nothing to fix.

State-leak check: `g_u02_bolt_avoid` / `g_u02_bolt_split_n` are assigned
**unconditionally** from the subject at every `render_scene`
(`manafold_art.h` 1282-1283), so subject N+1 cannot inherit subject N's mechanism.

### One thing the prose understates, and the owner must be told

The report's §4 heading names **two** subjects on slot 0's choreography. There is
a **third: `manafold-crackle`.** `manafold.h` 134/181 builds `build_hover_idle`
twice, and `knead_schedule_slot(23) → 0` (`manafold_art.h` 2365-2367) sends every
scheduled layer on Crackle to **slot 0's** entry. So Crackle takes rear 170 and
front 1350 too — the item-1 change — whatever slot 23's table says. (Slot 23's
entries are set equal to slot 0's defensively and are never read; correct.)

So **all three clips the owner will compare are the same animation**, differing
only in camera and, on Crackle, the every-frame bolt variant. This is disclosed
correctly in the byte-identity table and in the source comments; only the §4 prose
is loose. It is also the **right** call — the complaint is about an animation, and
calming it in one presentation but not the others would show two different back
balls doing one performance. It belongs in the site copy in plain words.

---

## 2. THE MEASUREMENT — re-run on my own build, confirmed exactly

`manafold-boltgate.exe --census --fail-no-avoid`, my binary, production ink.

| claim | reported | **my run** |
|---|---|---|
| total drawn segments | 503,304 | **503,304** ✓ |
| genuine 3D intersections | 54,595 (10.8 %) | **54,595 (10.85 %)** ✓ |
| crackle | 5,685 / 42,364 = 13.4 %, 561 of 600 frames | **identical** ✓ |
| death-drop | 16.6 % | **16.6 %** ✓ |
| drift | 15.3 % | **15.3 %** ✓ |
| hover / inspect | 4,705 / 38,152 = 12.3 %, 530 frames | **identical** ✓ |
| **all intersections are fold-figure edge links; free strands contribute ZERO** | claimed | **CONFIRMED — I summed the BREAKDOWN `free` column across all 22 subjects: 0** ✓ |

Per-subject rows `diff` **identical** to `P24-RECEIPTS/mbolt-census-before.txt`
except one derived column — Inspect's `gap_mm` (6.6 mine, 26.6 theirs), because
their "before" also disabled the split and mine disabled only the avoidance. No
intersection number differs. Nothing the verdict rests on is affected.

**It is a geometry fault, not a drawing-order fault.** One drawn bolt segment in
nine is literally inside the antenna's volume, up to 99 mm deep. `glow_splat`
already depth-tests per pixel, so ordering was never the problem.

---

## 3. THE TWO MECHANISMS

### 3a. Avoidance (Crackle, Hover) — works, and does not restyle

**Zero intersections on both**, on my own gate run: Hover 0 of 38,152, Crackle
0 of 42,364, every key and midpoint.

**"It does not restyle" is proven structurally, not promised.**
`bolt_avoid_rods(int32_t pts[][3], int n, int lo, int hi)` takes **only the point
array**. Radius, colour, gain, stamp density, morph clock, station identity and
topology are not in its scope and it cannot reach them. Direction 23's *"It is
good now as it is"* is enforced by the function signature.
(Second-order and correct: moving a path changes a segment's *length*, so its
stamp *count* follows — but `kBoltStampMm`, the density per millimetre, is
untouched.)

**And `msmooth` — the gate on persistent lightning/particle identities and 60 Hz
continuity — is clean on my build:** 0 blackouts, 0 shape-chain breaks, 0 morph
reversals, 0 fold/surge role or count changes. That is the evidence that the
avoidance moved a *path* and did not switch a figure or break a loop seam. Its
`--fail-lightning-switch` control fires (RC 1).

**By eye** (`P24-REVIEW-LOOKS/R8-crackle2.jpg`, Crackle f0123 — the probe's own
worst frame, 5×, A/B): in pass 23 the figure's lower-right run lies **across** the
pink rod and continues past it. In pass 24 it stops short and turns inside the
pocket, with a clear gap of band showing between lightning and rod. **Size, jag
character, line width, the dotted inner filament, colour and density are all
unchanged.** Same lightning, moved. The complaint is addressed, visibly.

### 3b. Splitting (Inspect) — confirmed to change nothing

The claim that **all 155 depth-straddling segments were already drawn partly
occluded at N=1** reproduces on my run (INFO B2b). The unsplit bolt stamps every
26.6 mm, finer than the 46 mm rod, so there was no resolution to win.

**Looked at, properly isolated** (`R9-split.jpg`, Inspect f0117, 7×, with rear /
front / eye held at pass-23 values so *only* the split differs): the figure is in
the **same place, the same shape and the same size, and it still crosses the
antenna identically.** The only difference is that the white core reads as a
smoother continuous filament instead of showing faint beading, and the glow is a
touch softer.

> **THE FINDING THAT DECIDES THE ROLLOUT: depth-splitting does not touch the
> fault. 3D avoidance removes it entirely. The comparison has a one-sided
> answer.**

### 3c. The declared side effect — looked at, and it is not a fault

`R2-hover471.jpg`, Hover f0471 (tightest loop closure), 8×, A/B.

* **P23:** the figure lies **flat across** the lower-left rod — it reads as
  lightning stuck to, or tangled behind, the antenna. Muddy. This *is* the
  owner's complaint.
* **P24:** the same figure runs **around the outside** of that rod, following its
  edge, then jumps the pocket. Same size, same jag, same colours, same density,
  continuous filament, no tear.

Yes, that run is now outside the loop where it used to be inside it, and the owner
has not seen that placement. **It reads as energy running along the antenna and
arcing across — deliberate, and more legible than pass 23.** I judge it
acceptable and would ship it. Declared honestly by the implementer; the
`kBoltRodClearanceMm` lever remains if he disagrees.

---

## 4. ITEM 1 — the Front is delivered; **the back ball is not**

Each layer isolated by env so only the knob under test differs; measured with the
committed `framediff.py`.

| isolated change | worst frame | changed px | bbox (native) | total over the clip |
|---|---|---:|---|---:|
| **rear ambient 400 → 170** | f0449 | **178** | x=187..207 y=143..159 (**20×16**) | 39,283 |
| **Front gain 1000 → 1350** | f0403 | **11,758** | x=101..270 y=39..231 | 3,411,491 |

**Front: delivered.** `R5-hover-front.jpg` (f0402, 5×, A/B) shows the loop leaning
plainly further through the beat. Same loop shape, same size, same timing, same
seam — only amplitude. That is *"could move a little more"*, and 1350 is right.

**Rear: NOT delivered, and I am saying so plainly.** `R6-rear.jpg` lays six
consecutive frames at **10×** across the worst-changing window, pass 23 above pass
24. They are **essentially indistinguishable** — a shade or two at a rod edge. The
owner will not see this change.

This is **not a defect in the implementer's work.** Direction 25 *named* this lever
("make that scale per clip … and lower it for Hover alone"); the implementer built
exactly that, chose 170 by eye, and then declared the honest size himself (§4 "THE
HONEST SIZE OF WHAT THIS BUYS", open issue 3) instead of claiming a win. That is
the right behaviour, and it is why this section can be short.

### The lever that *would* deliver it — now measured

`ZHAO_U02_REAR_CARRIER_CALM_PM` drives carrier C, *"the back nodule the eye reads"*
(Direction 22). It was readable by **one gate binary** from pass 20 until this
pass, so **nobody has ever seen what it does.** This pass put it in the shared
parser. I verified both halves of that repair, and then laddered it one rung:

| Hover render | sequence CRC | verdict |
|---|---|---|
| shipping (compiled default) | `0x8EDC6DE3` | — |
| `CALM_PM=1000` explicit | `0x8EDC6DE3` | **byte-neutral — the repair costs nothing** |
| `CALM_PM=300` | `0x3214C32E` | **LIVE — it now reaches the render** |

At 300 it moves **9,352 px on the worst frame** (2,655,165 px total). The lever the
direction named moves **178**.

> **The repaired knob is roughly 52× the named lever.** Open issue 4 guessed this;
> it is now measured. It moves more than the back ball alone, so it needs a proper
> ladder by eye — but it is the first thing to try in pass 25.

---

## 5. ITEM 3 — the eye ambience at 600

* **Rest** (`R4-rest-eyes.jpg`, 5×, five frames, P23 over P24): the stars drift
  within the lenses and change size a little. Side by side you see it; on its own
  it reads as **life, not acting**. The star never approaches the lens rim.
* **Taunt II** (`R10-taunt2-eyes.jpg`, 9×): the same, subtler still.
* **Hover** carries the same layer at the same gain (slot 0, table entry 600).

**Localisation measured, not assumed.** Rest eye-only vs pass 23 changes at most
**280 px** on the worst frame, bbox **x=171..243 y=158..174** — 72×16 native,
exactly the two lenses. It does not leak onto the body, the antenna or the
lightning. Taunt II: 352 px, x=168..248 y=148..167.

* **Curious, Startle and Taunt III are byte-identical**, verified on my own bank:
  shipping vs all-off differs on exactly **19 of 22**, and the three that do not
  move are precisely those three. No floor was slid under the authored beats.
* The diagnostics take gain 0, and Trick's planted window is muted — with a
  `static_assert` at `manafold_art.h` 3972 binding the mute window to the
  **authored** `kTrickPlantKey` / `kTrickLiftKey`, so the window cannot silently
  stop covering the plant. That is gate-checklist item 24 answered properly.

---

## 6. INSTRUMENTS — every control fired by me

| control | leg(s) red | RC | my reading |
|---|---|---|---|
| `--fail-no-avoid` | B1, both subjects | 0 (fired) | hover 0 → **4,705**, crackle 0 → **5,685** |
| `--fail-no-split` | B2 only | 0 (fired) | the 4× ratio collapses to 1× |
| `--fail-fat-rod` | B1, both subjects | 0 (fired) | **17,679 / 19,863** at radii ×220 % |

Each fails **for its own reason**, on its own leg; no control reddens a leg it
should not. `RC 0` on a control is correct **inverted polarity**, documented at the
site (`return fired ? 0 : 1;` — *"a control that does not fire is the failure"*) —
the committed-mutant pattern from CLAUDE.md.

### The positive control the brief asked for, on the gate's own exit code

B1's shipping reading is **zero**, and a detector reading zero is a claim. So I
fired the gate on a fault it *should* catch, with **no** `--fail` flag:

```
ZHAO_U02_BOLT_AVOID=off  manafold-boltgate.exe --gate   →  RC 1, "mbolt: LEGS RED"
```

The silence is worth something.

* **No gate default samples a subset.** mbolt measures all 22 (`kLiveSubjects = 22`,
  asserted against the renderer's own 22); mrod's help states *"with no slot
  argument the WHOLE BANK is measured"*.
* **No bound or `constexpr` was relaxed.** `git diff 1d449717..HEAD` removes no
  `constexpr` and lowers none. The only deleted logic line is the stamp count,
  replaced by the same call `* split` — an identity at N = 1.
* **Every gate green on my binaries, RC 0:** mbolt, msmooth, mspan, mrear, mprobe,
  mnodule, mjointpub, moutline, mshell, meyesize, mqa, mrod.

### Reviewer finding (non-blocking): mbolt's selector table is an **unbound mirror**

`manafold_boltgate.cpp` 391-403 holds its **own** per-subject `avoid` / `split_n`
columns and applies them itself (`u02::bolt_set_subject(r.avoid…, r.split_n)`,
line 428). It does not read the renderer's assignment.

Bound today: the subject **count** (22 == 22) and the live list vs `creatures.json`.
**Not bound: which mechanism each subject carries.** If `s.u02_bolt_avoid_rods`
were dropped from Hover tomorrow and mbolt's table left saying `true`, mbolt would
switch avoidance on *itself*, measure, and print **"B1 CLEAR manafold-hover: 0 of
38,152" — green while the shipped clip drew bolts through the rod.**

**Not a blocker for pass 24:** I read both sides and they agree exactly today. The
gap is about future drift. **The remedy is this codebase's own pattern** — hoist
the per-subject bolt config into one table with a single accessor, the way
`rear_ambient_clip_gain_pm()` and `front_flex_clip_pm()` already are (*"the ONE
production read, so the solver and every gate go through the same door"*), and have
both files read it. Recommended for pass 25.

---

## 7. BYTE IDENTITY — verified on my own independent build

| configuration | result |
|---|---|
| **everything off vs pass 23** | **22 / 22 byte-identical** (`diff` clean vs `baseline-pass23-crcs.txt`) |
| **my shipping bank vs the implementer's `crcs-ship.txt`** | **22 / 22 identical** |
| **shipping vs pass 23: scope** | exactly **19** changed; unchanged = curious, startle, taunt III |

---

## 8. OPEN ISSUES carried to pass 25

1. **The back ball is still finicky.** `kRearCarrierCalmPm` is ~52× the lever
   Direction 25 named and has never been laddered. Ladder it by eye first.
2. **mbolt's selector mirror is unbound** (§6). One shared table closes it.
3. **The rollout question.** Avoidance is the winner; the two worst clips by rate
   — **death-drop 16.6 %** and **drift 15.3 %** — are *not* in this experiment,
   and 39,500 intersections remain on the 19 untouched subjects.
4. Open issues 5 and 6 from the implementation report (the orthographic screen leg;
   the split's divide-based brightness compensation) stand as written, and neither
   affects a shipped value.

---

## 9. Evidence

| file | what it shows |
|---|---|
| `P24-REVIEW-RECEIPTS/mbolt-gate.txt` | my shipping gate, all legs OK |
| `P24-REVIEW-RECEIPTS/census-{before,ship}.txt` | my census, both states |
| `P24-REVIEW-RECEIPTS/ctl-{no-avoid,no-split,fat-rod}.txt` | the three controls, each firing |
| `P24-REVIEW-RECEIPTS/envoff-gate.txt` | **the positive control on the exit code (RC 1)** |
| `P24-REVIEW-RECEIPTS/bank-{ship,alloff}.crc`, `changed.txt` | the two banks and the exact scope |
| `P24-REVIEW-RECEIPTS/gate-*.txt` | every other gate, green on my binaries |
| `P24-REVIEW-LOOKS/R2,R5,R6,R7,R8,R9,R4,R10` | the A/B pairs this verdict rests on |
