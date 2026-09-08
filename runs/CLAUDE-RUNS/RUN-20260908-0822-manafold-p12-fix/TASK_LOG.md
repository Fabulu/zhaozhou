# Task Log: RUN-20260908-0822 - [Describe objective here]

**Created:** 2026-09-08 08:22 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-0822-manafold-p12-fix/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 08:22 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-0822
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

## 2026-09-08 — lane setup and item 1 root cause

**Lane:** `C:\programmieren\zencrifice\manafold-p12-fix\{zhaozhou,Upheaval}`.
Cloned from the local checkouts, then `git remote set-url origin` to the GitHub
URLs and **verified both** (`untitled-game.git`, `zhaozhou.git`). The reviewer's
warning about `origin` silently resolving to a local path is handled: the lane
remotes are separate named remotes (`ln*`, `zn*`), never `origin`.

**Base chosen.** Both repos have five divergent lanes. Measured with
`git rev-list --left-right --count`:
- Upheaval: `manafold-p12-qa/main` (845e271) is 12 ahead of the coordinator
  branch and **0 behind** — a strict superset of coord, review, pub and
  GitHub `main` (e11de6c). Base = that.
- zhaozhou: `manafold-p12-qa/main` (178973a2) is 6 ahead of coord, 1 ahead of
  w3, **0 behind** either. Base = that.
- NOTE: the review's "133 commits unpushed, one machine" risk is **already
  resolved** — GitHub `main` (e11de6c, 2026-09-08) contains `creature/Manafold`.
- NOTE: this base **includes Wave 3** (eye travel, nodule vertical travel,
  flight bob), which was NOT in the build the reviewer judged. Renders will
  differ from the reviewed ones for reasons beyond my fixes. Declared, not hidden.

### Item 1 — THE CORPSE STANDS BACK UP: root cause found, and it is not a knob

The authored keys are **already correct**. `build_death_drop` / `build_death_gutter`
write the dead pose, `kDeathRestRootMm` and a bit-zero deform at every key from
`B.settle` to `K-1`. Nothing in the tail is alive.

The resurrection is in the **presentation interpolation**, not the authoring.
`reference/src/zcreature/creature_core.cpp` computes the sub-frame partner as

    frame + 1 >= clip.frame_count ? (clip.hold_last ? frame : 0) : frame + 1

at four sites (pose quats, root displacement, `deformation_sample`,
`deformation_frame`), plus the `wrap` lambda at line ~230 used to bake midpoints.
With `hold_last == false` the final key's sub-frame **blends toward key 0 — the
alive hover pose**. That is the 11 px jump and the ~6,500 changed pixels, and it
is two frames because a key is shown for two sim ticks.

`grep -rn hold_last` over the tree: **Zixxtrixx sets it on eight clips**, including
`zixxtrixx.h:4550` — *"one-shot: the corpse holds; no wrap-to-stance flash"*.
**`manafold_clips.h` sets it nowhere.** The sibling creature already solved this
and the fix did not travel.

**Why the previous pass could not tune its way out (09-ENGINE-GOTCHAS §18).**
`kDeathOpenKeys` (manafold_art.h:1930) exists *because they hit this exact seam* —
its comment says "at the clip's LAST key that stream interpolates toward key 0 …
no matter how carefully the tail is zeroed". They fixed it **for the deform only**,
by easing the deform in from zero at key 0. Root and quats were left wrapping.
The knob being turned (tail length, open keys, the caption) was never the thing;
`hold_last` is.

**Fix:** `c.hold_last = true` on both death builders. One line each.
