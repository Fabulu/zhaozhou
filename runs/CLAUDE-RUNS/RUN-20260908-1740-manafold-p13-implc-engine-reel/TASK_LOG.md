# Task Log: RUN-20260908-1740 - [Describe objective here]

**Created:** 2026-09-08 17:40 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-1740-manafold-p13-implc-engine-reel/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 17:40 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-1740
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

## IMPL-C — engine + reel, Manafold pass 13 (R2, R6, R7, R8)

Lane: `C:\programmieren\zencrifice\manafold-p13-c\{zhaozhou,Upheaval}`.
Base: zhaozhou `ffae071e`, Upheaval `64f48cf`.
Exclusively mine: `reference/src/zcreature/creature_core.cpp`,
`reference/include/zref/zref_creature.hpp`, `tools/reel/zhao_reel.cpp`,
`Upheaval/website/deploy.ps1`. NOT `manafold_clips.h` / `manafold_art.h`.

### Reading done first
CLAUDE.md (art law + build notes), PASS-13-PLAN §0/§2/§3/§4,
PASS-12-QA §5.3 §5.10 §6.1 §6.3 §7.1, 09-ENGINE-GOTCHAS §2 §16 §17 §19,
10-GATE-CHECKLIST §0.

### R2 mechanism, traced in source before editing
* Four partner sites, all the same expression
  `frame+1 >= frame_count ? (hold_last ? frame : 0) : frame+1`:
  - `creature_core.cpp:347` decode_pose ROOT midpoint  <- the only one that moves the root
  - `:365` decode_pose quats (cyclic by construction)
  - `:462` deformation_sample lane 0 (cyclic)
  - `:497` deformation_frame lanes 1..4 (cyclic)
* A FIFTH site nobody named: `bake_presentation_midpoints`'s `wrap` lambda
  (`:227-231`) feeds the root Catmull-Rom, so a `bake60` bank has the same
  fault in TWO segments (k=n-1 via p2, k=n-2 via p3). Manafold does not set
  `bake60` (only `zixxtrixx.h:8356` and `zixx_probe.cpp:2304` do), so it is
  inert for this creature -- but leaving it is exactly gotcha 16's partial fix.
  Fixing both.
* The reel renders each key TWICE (`anim_advance`, sub 0/1), so the last
  rendered frame of every clip is (key n-1, sub 1) -- the wrap blend.
* The grey splash: `zhao_reel.cpp:4383-4396` derives `u02_speed_mm` from
  `fa.body`, which is skinned off `decode_pose`'s output. So the smear reads
  the POSED root; fixing decode_pose fixes the smear. One site, not two.

### Landed
* `4dcfbaae` R2 — `zc::Clip::wrap_root_delta`, default OFF, plus BOTH core
  sites (runtime nlerp partner in `decode_pose`, and the Catmull-Rom taps in
  `bake_presentation_midpoints`). **THE FLAG IS LANDED — IMPL-B's four opt-in
  lines are unblocked.**
* `0f745f1c` R7 — `u02_cover` built on `u02_mist || u02_shell`, `u02_shell`
  species-gated to `kUnnamed02` in the same edit, lying `U02_FOG_THICKNESS`
  ladder log line deleted.
* Upheaval `07f1bc6` R8 — `-SkipMediaCheck` split into `-SkipDecodeSweep` /
  `-SkipFreshness`; the old flag is a hard error; every deploy writes its own
  record.

### R6 — VERIFIED, not redone (the coordinator's `ffae071e`)
* `s.planet` assignments in the whole reel: 5297 (`planet_subject`, unrelated),
  5412 (the skybox-bloom spike subject, unrelated), 5677 (`subject_u02_clip`
  slot 2, now behind `u02_planet_on()`), 8149 (crackle, now behind
  `u02_planet_on()`). Three u02 diagnostics set `s.planet = 0` explicitly
  (`antenna-fixed`, `nodule-solo`, `antenna-quarter`) — turning it off, so
  unaffected. **NO THIRD SITE.**
* Slot-2 u02 subjects that reach the `subject_u02_clip` bloom block:
  `manafold-channel`, `-antenna-fixed`, `-antenna-quarter`, `-trio`. The last
  three either force `planet = 0` or were already inside the same gate, so the
  helper did not change which subjects can raise a bloom.
* `ZHAO_U02_PLANET=1` restores both: channel through the slot-2 block (with its
  own sun params) and crackle through its own block (sun 58/96/132). The forced
  path is `(kU02BackdropBloom || forced) && !off`, so the constant and the env
  var cannot disagree.

### R7 — witnessed-fail leg, the number the plan asked for
`manafold-still`, `U02_SHELL_ALPHA` 440 vs 0, one binary per row, celmain +
diagonal-cool-cross, frames read through `rgbframe.py`:

    BASE ffae071e   f0000-f0003    0 px changed of 92160   <-- the plan's "it changes zero today"
    MINE 0f745f1c   f0000  8865 px  mean |delta| 17.5
                    f0001  8869            16.6
                    f0002  8888            18.3
                    f0003  8900            18.7

Corroborated rather than trusted (09-ENGINE-GOTCHAS §16 rule 1): PASS-12-QA
§5.3 measured the SAME shell on `manafold-hit` at 8654 / 8277 / 8177 px with an
ink lift of ~16 counts of 255. `still` now lands in the same band from a
separately built binary, so the magnitude is not a new number nobody can check.

### Instrumentation, committed rather than thrown away
`tools/reel/bitident.py` — the Zixxtrixx bit-identity harness. It has been
hand-rebuilt in at least four passes and discarded each time (PASS-12-QA §5.10
had to build its own; PASS-12-FIX built another). Two independent metrics
(printed `sequence_crc32c` + sha256 over frame bytes), EMPTY refused as a pass,
a CRC-distinctness check, per-frame counts, ids enumerated FROM SOURCE (71, not
a stale hardcoded 16), and frames deleted as they are hashed so it cannot
recreate the 129,000-`.rgb` disk fill.

### R2 — proved, by eye and by number

`tools/reel/wrapseam.py` (committed). Mean absolute inter-frame difference over
the whole rendered clip, wrap frame stated as a multiple of THAT CLIP'S OWN
interior median (a shared absolute band would be a number nobody derived):

    clip     n    median   MAX interior   INTO wrap OFF      INTO wrap ON
    flight  352   1.587    2.640          12.211 = 7.69x     1.393 = 0.88x
    fall    340   0.599    1.729           4.029 = 6.73x     0.228 = 0.38x
    drift   300   0.714    1.554           4.657 = 6.52x     0.738 = 1.03x
    hasty   240   0.732    1.969           2.301 = 3.14x     0.785 = 1.07x

Flag off, the wrap frame moves further than anything the animation does on
purpose (above each clip's own interior maximum). Flag on, it is at the median.

SURGICAL: frame-by-frame diff of `drift` off vs on = **2 of 300 frames differ**
— f298 (the wrap blend, 12,784 px) and f299 (786 px, the smear plane's
one-frame memory of it). The other 298 are byte-identical.

Frame indexing pinned down (easy to be off by one and then measure the wrong
frame): the reel advances THEN renders, so frame i shows key (i+1)/2 at sub
(i+1)&1; for n = 2K frames the wrap blend is n-2 and n-1 is key 0 sub 0.
Corroborated: the architect named flight f349->350->351 by eye; the probe lands
on f350.

### The plan's R2 gate cannot be met, and should not be
`manafold_qa_p12.cpp:238-250` computes Q3's WRAP from `Clip::root` — the
AUTHORED KEYS. A presentation-only flag cannot move it. Measured:

    slot  1 drift  WRAP 6854.4 | slot  8 hasty  WRAP 8330.2
    slot  9 fall   WRAP 3599.9 | slot 22 flight WRAP 4375.2
    slot 19 lasso  WRAP   36.6 | slot 20 blown  WRAP    0.0

which incidentally CONFIRMS the plan's opt-in list numerically: lasso and blown
genuinely return to their start. One clip nobody named — slot 15,
`lab::build_manalab()`, WRAP 1191.0 mm — has the same fault; lane-only, not on
the page, recorded so it is not rediscovered.

### An orphaned process, exactly as CLAUDE.md warns
A `zixxtrixx-idle` render launched at 17:33 hit the tool's 2-minute timeout. The
shell died; **the process did not**, and was still burning a core at 18:33 with
1,348 s of CPU, writing into a directory that had been deleted underneath it.
Killed by PID. "Stopping an agent does not stop its background work" — and the
tell was that the CRC sweep's throughput did not match its worker count.

### Zixxtrixx re-proof — leg 1 GREEN, leg 2 RED as required
    LEG 1  base ffae071e vs mine 0f745f1c, ZIXX_EXP=celmain
           IDENTICAL 69   DIFFERS 0   EMPTY 2   of 71
           frames 15634/15634 identical   (the 1896 is inside it, exactly:
           attack 560 + idle 576 + moving-light 600 + walk 160)
    LEG 2  base vs MUTANT (wrap_root_delta defaulted true), rc 1
           6 DIFFERS / 2 IDENTICAL of the 8 that completed before the teardown;
           the 2 IDENTICAL are `corpse` and `death` -- the hold_last clips, so
           the corpse hold is untouched even with the flag forced on everywhere.
           63 subjects rendered nothing and the harness NAMED them rather than
           scoring sha256-of-nothing as a pass.
    LEG 3  default env, cut at 23/71 rows (all IDENTICAL) to free the machine.

One check of mine fired: `distinct CRCs 68 of 69` under celmain -- two subjects
share a CRC. Not chased (§6.2 of the findings); it cannot weaken an A/B of the
same subject and the hash leg agreed on every row.

### R6-bis (coordinator direction, mid-lane)
`planet_sun_mag` per subject (-1 = the PlanetDef's own, so everything that
predates it is bit-identical) + `u02_backdrop()`, one function with three
declared states. `kU02NightSunMagPx = 25` authored BY EYE at 4x on channel
f0292 -- the frame found by sampling for the most cyan, not by index -- from a
seven-rung ladder off ONE binary via ZHAO_U02_NIGHT_MAG.

25 beats BOTH shipped options: it keeps the violet night AND the lightning reads
better than on the day sky, because pale cyan separates further from dark violet
than from a bright warm ground. crackle, one binary, f100:

    ZHAO_U02_PLANET=1    near-white 15.16%   sky (55,37,95)
    default (mag 25)     near-white  0.57%   sky (27,17,62)    <-- ships
    ZHAO_U02_NOPLANET=1  near-white  0.15%   sky (167,97,109)

The rung I judged IS what ships: ladder mag25 (env override, pre-constant
binary) is byte-identical over all 420 frames to the default render from the
binary built afterwards. Publish bank NOT re-rendered, per instruction.

### Binary provenance (recorded, not remembered)
    build-base ae25b624e91d0ec9ffe03444d3c4fc64
    build-c    e69213a1014cd28faa61b23a2d2e2c8b
    build-mut  10ff7a8effdeb00c12cf577c0122f616
    build-r6   0c2002cbd6bf38180ffcab5eb6dab800
WARNING: build-r6 was rebuilt IN PLACE, so the ladder binary's md5 is lost. The
ladder is internally consistent by construction (one exe, one invocation) and
its verdict was re-proved byte-identical against the surviving binary -- but the
rebuild should have gone to a new directory.
