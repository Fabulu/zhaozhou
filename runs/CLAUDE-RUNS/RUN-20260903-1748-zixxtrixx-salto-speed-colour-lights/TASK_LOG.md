# Task Log: RUN-20260903-1748 - [Describe objective here]

**Created:** 2026-09-03 17:48 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-1748-zixxtrixx-salto-speed-colour-lights/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-03 17:48 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-1748
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## 2026-09-03 — Direction 26 pass, both asks authored

**Read first:** OWNER-DIRECTION-26 (the two asks), -25/-24/-23, peel run QA.md
(what passes and must not regress), RECON-4 machinery map, zhaozhou/CLAUDE.md.

**Baseline:** built cel at HEAD `cf114f88` into `build-lane`, rendered all 22
subjects with one explicit `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`
invocation. All 22 sequence CRCs reproduce the peel-run QA table exactly
(`evidence/baseline-crc.txt`, all-match-QA: True).

### Ask 1 — the single salto is faster

* Target identified: `zixxtrixx-salto-dummy` (slot 33, page "Salto: grounded
  mark"), the plain one-turn salto. ASSUMPTION stated: salto-fly also computes
  one turn but the owner said "Only that one"; the grounded mark is the plain
  single-salto clip. One-line reversal if wrong.
* The slow read was quantified before editing: the dummy spent 18 keys on ONE
  turn where six/nine spend 7.3/8.0 keys per turn.
* Change: new owner knob `kSaltoSingleCoilKeys = 10` in `zixxtrixx.h`, applied
  in `zixx_variant_plan` for `kSlotAtkDummy` only — the exact pattern six and
  nine already use. `zixx_plan_lock_spear` never reads `coil_keys`; the target
  is static so the intercept lead is inert; spear/apex/plunge and the whole
  shared arming schedule untouched. No seam-table literal, no wobble-period
  arithmetic, no probe phase-band input changed.
* Probe: `ZIXX PROBE: PASS` on the edited build (build-work).
* Eye: clip 311 → 295 frames (exactly the 8-key coil cut ×2 ticks). The wheel
  forms, holds shape, and turns at the fast variants' rate; the arming and the
  spear dive are frame-identical in read. Sheets + native frames in evidence/.

### Ask 2 — three coloured moving lights

* The compositor supported ONE point source. Extended honestly:
  `g_creature_point_light` (single) → `g_creature_point_lights` +
  `g_creature_point_light_count` (contiguous array; count 0 = shipping default,
  same code path as the old nullptr = byte-identity boundary for every
  ordinary subject). Per-vertex/per-face response is now a per-channel
  gain-weighted SUM over sources (`PointShade3`), summed before the shade
  quantiser — linear transport, so intersecting pools MIX by construction.
  Files: `zref_creature.hpp`, `creature_sim.cpp`.
* Reel: the warm lamp is source 0, verbatim. Three new sources — blue (level
  2-lap orbit), orange (counter-rotating 3-lap orbit with bob), green (low
  near-side 3-trip longitudinal shuttle) — every colour, radius, gain, path
  extent, period and phase a named constexpr owner knob in `zhao_reel.cpp`.
  Depth-tested visible orbs, one per source, tinted per colour.
* Author-render-look loop, 3 iterations at native 384x240:
  * v1 (outer 2800, gains ~2.5): DISCO — pigment obliterated, all pools on the
    body at once. Rejected on sight.
  * v2 (outer 2000, gains ~60%): better, still 3 hues everywhere.
  * v3 (outer 1500, orbits widened, green pushed to -1000): pools are local
    and travelling; most frames rest near the accepted warm read; overlaps
    happen and read as mixed colour (magenta where warm+blue meet, the green
    pool riding the crown). Pigment and form read. ACCEPTED by eye.
    Evidence: ml-v3-sheet.png, ml-v3-f000/150/330/510.png vs ml-base-*.png.

## Publication prep (while the 22-subject render runs)

* zhaozhou committed + pushed: `6a400759` (both asks). Probe PASS from the
  clean build-lane rebuild at that SHA.
* **Generation Sixteen preserved BEFORE any encode**: 44 files copied `cp -p`
  from the committed live bank, SHA256 manifest at
  `Upheaval/creature/Zixxtrixx/ARCHIVE-GENERATION-SIXTEEN-SHA256.txt`.
  creatures.json: Sixteen entry + order slot; assemble.py
  MAX_ARCHIVE_GENERATIONS 15→16 and BOTH style.css selector families extended
  in the same edit. Live moving-light and salto-dummy notes updated to describe
  Direction 26. Upheaval committed + pushed: `c3809ca` (atop two doc commits
  another session pushed to origin — merged by fast-forward, no force).
* **TRAP FOUND before it fired**: scratch-reel still held the peel pass's
  frames; the renderer overwrites %04d.rgb but never deletes stale trailing
  frames, and tovideo.py encodes every NNNN.rgb it finds. salto-dummy shrank
  311→295, so 16 stale frames would have been glued onto the new clip's webm.
  Plan: let the running invocation exit (verified by PROCESS EXIT, not
  directory contents — every meta.txt already existed from the prior pass,
  so file checks prove nothing), then wipe scratch-reel and re-run the one
  fresh explicit invocation into clean directories.
* Plan refined: the running invocation IS the one fresh explicit invocation;
  every frame it writes is fresh, and only salto-dummy's trailing 16 frames
  (0295-0310, from the longer previous clip) are stale. After PROCESS EXIT,
  delete every NNNN.rgb with index >= that subject's fresh meta.txt frame
  count, verify by mtime, then CRC-sweep and encode. Poster index for
  salto-dummy moved 96 -> 176 in tovideo.py (the dict's stated intent is "the
  spear meets the dummy"; 96 had silently become mid-compression when the peel
  grew the ground phase, and the fast flip moves impact earlier still).
  Moving-light poster 412 re-checked by eye: warm-over-head with pink mixing
  on the neck, blue tail, green orb visible -- kept.

## The CRC proof

Publication render (one fresh explicit invocation, all 22 named) vs baseline
(`evidence/publication-crc.txt`):

* **20 subjects byte-identical** — sequence CRC-32C and frame count equal to
  the baseline that itself reproduced the peel QA's table exactly. This
  includes salto-fly (the stated assumption: it also computes one turn but the
  owner said "Only that one"), both jumps, the attack, six, nine, and the
  entire non-spring bank.
* **Exactly two changed**: salto-dummy `0x652E456A`/311f → `0x6D95E2FD`/295f
  (the 8-key coil cut), moving-light `0xDE5D2626` → `0xC4C83E50`/600f (the
  three added sources). Both new CRCs equal the build-work draft renders —
  determinism across two build trees confirmed.
* The 16 stale trailing salto-dummy frames from the previous longer clip were
  deleted after process exit and counts re-verified against fresh meta.txt.

## THE RUN'S OWN INSTRUMENT WAS LYING — caught before publish

The poster PNGs written by tovideo.py (which reads the 8-byte header
correctly) did not match what my rgb2png.py had been showing me. Root cause:
`%04d.rgb` carries a `u32 w | u32 h` header (zhao_reel.cpp:29) and my reader
fed those 8 bytes into the pixel stream — 8 mod 3 = 2, so EVERY pixel's
channels were rotated and the image shifted ~2.7 px. **Every colour judgement
to that point — including the v1 "disco" verdict and the two gain-cut
iterations it drove — was made through a psychedelic rotation of the real
palette.** The project's signature failure, in this run's own comparison tool;
the same class of fault peel-QA found in qa25_contactfront.py.

Repaired (reader now verifies the header and asserts dimensions), then EVERY
standing judgement re-made on true frames:

* v3 in true colour was much subtler than the rotated reader had shown. Green
  read; blue read as cyan on the green pigment (honest transport); ORANGE was
  indistinguishable from the accepted warm lamp.
* v4/v5: orange saturated to 2.50/0.85/0.08, pool 1700 mm, orbit 2000 mm.
  Closest-approach frames were located by a comparison-side path calculator
  (never used to author the paths) and looked at: f354 carries a distinct
  orange band on the neck beside the blue crown and the green back pool, with
  mixed boundaries. ACCEPTED by eye. The salto poster-frame choice (f176) was
  composition-based and survives the colour repair.
* zhaozhou `5f885a02`. Final clean-scratch 22-subject render invoked fresh
  from build-lane at that SHA; expected CRCs: salto-dummy 0x6D95E2FD
  (unchanged since 6a400759), moving-light 0x756E0BFF (v5 draft).

## Close-out — published

* A launched background render silently died once (only a partial idle dir; an
  independent sweep confirmed nothing running) and a second attempt raced my
  own scratch-reel wipe — that renderer was killed deliberately, scratch-reel
  wiped, and ONE final fresh explicit invocation rendered all 22 subjects into
  clean directories. Completeness verified per subject against its own fresh
  meta.txt: contiguous 0..N-1, every frame 8+384*240*3 bytes, no stale files.
* **Final CRC proof** (`evidence/publication-crc-final.txt`): 20 subjects
  byte-identical to baseline; exactly two changed — salto-dummy
  0x652E456A/311f -> 0x6D95E2FD/295f, moving-light 0xDE5D2626 -> 0x756E0BFF/600f.
  Both reproduce their accepted draft CRCs from a different build tree.
* Only the two changed subjects re-encoded (VP9 nondeterminism would otherwise
  churn 20 byte-identical clips); decoded frame counts verified 295/600.
* Probe: `ZIXX PROBE: PASS` at the final merged state. Mains merged without
  force; pushed.
* Deployed ONCE: `deploy.ps1 -Project upheaval -Branch main`. Production
  verified: HTTP 200 on page + both new webms + a Sixteen archive file; the
  served webm byte sizes equal the local encodes (606160 / 406407); exactly
  one noindex meta; archive tab lists 16 generations with Sixteen present.
* Background jobs stopped; process sweep clean (no zhao/ffmpeg/wrangler).
* Final SHAs: zhaozhou `5a6cd428`, Upheaval `84d89fa`.
