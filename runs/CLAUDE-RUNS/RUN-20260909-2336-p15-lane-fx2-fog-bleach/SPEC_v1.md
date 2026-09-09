# SPEC v1: LANE-FX-2 — the fog bleaches the body; make it fog, not bleach

**Run ID:** RUN-20260909-2336
**Created:** 2026-09-09 23:36 UTC+02:00
**Status:** Active
**Lane:** `manafold-p15-fx/{zhaozhou,Upheaval}`, both reset to `origin/main`
(zhaozhou `7f9a7861`, Upheaval `55e42b3`) as the first action.

---

## Objective

`PASS-15-REVIEW.md` §1 holds the pass-15 publish: the new fog shell composites
near-white over **every interior pixel** of the creature, bleaching its pigment
and erasing the terminator that made a flat-shaded ball read as ROUND — a
quality the owner explicitly praised.

**Success = the shell is still plainly VISIBLE as gas, and `inspect` f0300's
terminator is back**, judged by eye at native 384x240 and at 3x, against a
**pass-14 control in the same plate**.

Owner's sentence (D11 §2.3) is the acceptance test: *"the outer body part is made
of a thick fog that gets less thick the nearer to the outside it goes"* — a shell
**around the form**, not a veil over the whole interior.

---

## Scope

**In Scope:**

- `zhaozhou/tools/reel/manafold_fx.h` — the shell profile and its tint.
- `zhaozhou/tools/reel/zhao_reel.cpp` — the `shell_paint` call region and the
  shell env knobs ONLY.
- `rungsweep.py` — the ladder driver.
- `manafold_shellgate.cpp` / the `mshell` target — re-aim, and check the re-aim
  still means something after the profile changes.
- `Upheaval/creature/Manafold/PASS-15-FINDINGS-FX2.md` + the ladder plates.

**Out of Scope — MUST NOT BE TOUCHED:**

- The lightning (`kFoldStrand*`, `kManaStorm*`, `kRampShimmer`, `kShimmerHue`).
  The review calls it "excellent and emphatically not black" at its best.
- The green fold's byte-identity (`f99c9722...`).
- The deaths; the mist exclusion; `channel`'s violet night.
- The eye lane's and antenna lane's landed work — anything outside my three
  files. **Rebase, never merge blind** (pass 14 lost a sibling lane that way).
- Zixxtrixx bit-identity — re-prove it if `zhao_reel.cpp` is touched.

---

## Constraints

- **LOAD:** one build at a time, one renderer, **never encode the bank**. A QA
  lane and the hardware lane's Quartus share this machine. Identify processes by
  **command line**; kill only by **PID**; never `taskkill /IM`.
  - Census at 23:36: `zhao-reel-cel.exe` PID 23844 alive, and it is the **QA
    lane's** binary (`manafold-p15-qa\build-qa\bin\`). Not mine, not to be killed,
    and my own render waits for it.
- **`--clean` builds only** (`09-ENGINE-GOTCHAS` §19 — the direct builder does not
  track headers, and every file I am editing is a header). Never `cmake --build`.
  **Read the build's own exit code, never a pipeline's.**
- **Read `.rgb` ONLY through `tools/reel/rgbframe.py`** (selftest first).
- Render with `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`, always.
- **A ladder comes from ONE binary** (item 26) — every knob has an env override,
  so this is achievable and there is no excuse for a two-binary ladder.
- **A known-negative is an ABLATED MECHANISM, not a still frame** (item 43).
- **A contact sheet finds a candidate; a 2x crop of a named frame confirms one**
  (item 41).
- Both backdrops — the sunset day and `channel`'s violet night (`08-LIGHTING`,
  the backdrop law).

---

## The fault, as inherited

| knob | ships | what the review says |
|---|---|---|
| `kShellOutReachPm` | 55 | only **5.5% of R** outside the silhouette — the entire exterior |
| `kShellFogDepthPm` | 520 | the annulus runs **52% of R inward** |
| `kShellAlphaMaxPm` | 560 | peak **56%** blend, sitting **0.52·R inside** the outline |
| `kShellCoreFloorPm` | 180 | a plateau held across the whole core — every interior pixel lifted |
| `kShellTint` | {255,214,232} | near-white. **Never swept.** The declared fog-vs-bleach axis |

The previous lane declared **both** of these itself and shipped anyway:
`kShellTint` *"the axis that decides fog vs bleach"*, and its `wash-check.png`
showing the fog *"lifting deep magenta a long way."*

⚠ **And it is buying nothing.** The shell's stated purpose (D11 §2.3) was to make
a lens sinking into the bouncing body read as fog rather than clipping. LANE-EYE
cured that a **different way** — the lenses ride the deform and stand proud
(**N01**, `hit` f24–f52). There is no sinking left to absorb.

⚠ **But do NOT turn it off.** He has asked for the shell since Direction 5 and it
must be VISIBLE. The complaint is that it **bleaches**, not that it exists.

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

- The previous lane's ladder picked 520/560/180 by eye and was **wrong on the
  merged tree** — its ladder was `hit` f28 at 3x/6x on the EYE, judging whether
  the lens dissolved. It never put the whole body beside a pass-14 control, so
  the terminator damage was outside the frame it was looking at. **The plate has
  to be the whole body, and the control has to be in it.**

---

## Open Questions

- Is the fault the PROFILE (the interior plateau) or the TINT (near-white), or
  both? The review says *"the profile is the fault, not the hue"* and says do not
  ladder the tint first; the previous lane says the tint is the undeclared axis.
  **Both get laddered — they are separable and one binary can carry both.**
- The one-word owner picture, if the choice turns out to be his.
