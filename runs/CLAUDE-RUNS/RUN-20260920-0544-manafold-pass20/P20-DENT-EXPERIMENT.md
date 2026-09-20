# The dent: §9 falsifying experiment — result

**Date:** 2026-09-20 (packet 5)
**Design:** `P20-SOLVER-ARCHITECTURE.md` §9
**Built:** steps 1–2 of §8 only. Solver default still `kCarried`; nothing
rendered for the eye, no ladder, no gates added.
**Verdict: FALSIFIED on two of the four criteria.** Stopping as instructed. No
alternative mechanism improvised.

---

## What was built first

The solver exactly as §2.3 specifies: the dent's own chain walk (copied verbatim
from the closure walk, `>> 16` included), `dent_target_mm` as one rounded
function, two `nodule_aim` calls writing `kBHingeA` / `kBHingeB` and
`set_span_delta(1)` / `(2)`, and the renormalised world pin on `kBHingeC`. Plus
`knead_dip_window_env` factored out, the `Rig::dent_pm` guard, and the shared
`apply_knead_dip_env()` parser wired into every bank-building binary.

**The off path is bytes** (§4): hover `0x79D3F0C5`, inspect `0x0710E704`, still
`0x138FE8B0`, taunt3 `0xC81598AA` — 4/4 identical to the pre-change binaries,
all 11 gate normals green.

**Both positive identity controls pass** (§4): under `dent`, Taunt III and Still
are **byte-identical** to `carried` while Inspect changes
(`0x0710E704` → `0xE58175F8`). The dent did not leak through any path other than
`g.dent_pm`.

**One implementation bug was found and fixed before judging**, because it would
have falsified the design for the wrong reason: the ambient duck divided by
1 000 000 instead of 1 000, so at full envelope it scaled the ambient by 999/1000
— a knob that looked wired and did nothing. Every number below is post-fix. *A
knob that cannot move the thing it names is the inert-control trap again, this
time inside the new code rather than in a ladder.*

---

## The four criteria

### 1. F-A must not differ at all — **PASSES**

| | F-A signed delta |
|---|---|
| no dip | −198 … +199 mm |
| dent s=2000, duck 1000 | −198 … +199 mm |
| dent s=2000, duck 0 | −198 … +199 mm |

Unchanged across the whole bank. F-A has no term in the solve and behaves that
way.

### 2. C-E must not differ by more than 1 mm — **FALSIFIED (≈19 mm)**

The pin must be tested with the duck **off**, because the duck legitimately
shrinks the ambient and therefore the excursion; leaving it on hides the leak.

| | C-E signed delta |
|---|---|
| no dip | −687 … **+305** mm |
| dent s=2000, **duck 0** | −698 … **+324** mm |
| dent s=2000, duck 1000 | −687 … +279 mm |

**With the duck off the pin leaks: +19 mm on the stretch side and −11 mm on the
compaction side.** It is small, and it is the wrong sign for comfort — the
attachment span moves *outward* during the gesture, which is the exact thing the
design claims cannot happen "by construction, not by tolerance".

With the duck at its default the C-E compaction is *exactly* the no-dip value and
the stretch is *below* it. So under the shipping configuration the attachment is
not charged — but that is the duck compensating, not the pin holding, and a
compensation is a tolerance.

### 3. Interior compaction at the crossing ≈170 pm, ambient fully ducked — **FALSIFIED (3×)**

Estimate: −160 pm on A-B, −126 pm on B-C. Measured, duck at 1000:

| span | mm | pm | bound | over by |
|---|---|---|---|---|
| A-B | −124 | **−365** | −330 | **35 pm** |
| B-C | −183 | **−482** | −430 | **52 pm** |

Roughly **three times** the design's estimate, and over both interior bounds.
With the duck off it is worse (−159 / −234 mm → −468 / −616 pm). 94 bound
breaches with the duck on; 286 with it off.

The §2.2 arithmetic assumed the crossing gives up ≈102 mm shared between two
spans. The posed reality — rest tilts, the ambient schedule, the swallow beats,
and the 8% gap §11 declares — makes it substantially more.

### 4. R5 strictly lowest on Inspect at s=2000 — **PASSES**

At s=2000 with the full envelope, Inspect (slot 0) **does** reach strictly
lowest by the 20 mm margin. Bank-wide the dent reaches it on **13 of 21** clips
at s=2000 and **19 of 21** at s=3000.

So the geometry does what it was designed to do — and it is the first mechanism
in this pass that makes B genuinely the lowest ball on the owner's own witness
clip. That is worth recording even though the experiment stops here.

---

## Why this is a stop and not a tweak

Criterion 3 is not a threshold that can be nudged: the interior spans are asked
for three times the compaction the design budgeted, and clearing it would mean
relaxing `kSpanCompactionMinPm` on A-B and B-C — which is the widening this pass
has twice refused, one station further in. Criterion 2 says the pin, which is
the design's central claim, does not hold exactly.

Both are honest findings about *this* mechanism, not about the goal. The design
is right that a pinned re-fold never charges F-A, right that the mirror is
stretch-free at its endpoint, and right that the ranking is reachable — R5 at
s=2000 proves it on Inspect and on 13 clips.

**What the architect would need to decide:**

1. **Where the ~102 mm estimate went wrong** — the measured crossing cost is
   ~3× it. The estimate was from rest-pose arithmetic; the posed chain is the
   thing. This is §11's declared 8% gap turning out to be much larger.
2. **Whether the pin can hold exactly**, or whether the ~19 mm leak with the
   duck off is inherent to re-walking the chain through changed span lengths.
3. **Whether a deeper duck is the answer** — the duck already removes two thirds
   of the breaches (286 → 94), and at 1000 it holds C-E at or below its no-dip
   values. A duck that also scaled the swallow beats might close the rest, but
   §11 forbids touching the ambient schedule in this packet and I have not.

**Committed state:** the dent ships **off** (`kKneadDipSolver = kCarried`). The
shipping bank is unchanged and every gate is green. The solver, its knobs, the
shared parser and this result are committed so the next packet inherits a built
mechanism and measurements rather than an estimate.
