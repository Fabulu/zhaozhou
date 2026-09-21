# FINDINGS — POSEREAD (entry I29's consumer)

**2026-09-21. Branch `gz/poseread` from `28ddaef9`, 6 commits, merged.
Register 22 → 22. I29 REDUCED, not closed.**

Transcribed by the coordinator; the harness refused the lane's own write.

---

## 1. The measurement it was sent to make

**§10.1 does NOT determine the request side — and it is not a second open
decision either.**

* §10.1 is a **datapath** ruling (*"no new animation-specific arithmetic block,
  no direct GEOM.POSE ↔ MEM.HPS.BRIDGE"*) with **no field, width or naming in
  it**.
* §10.3 lists handles / generation / epoch under **"MAY require"** — *a menu,
  not a specification.*

**What determines it is `spec/memory_rules.md` §5f.1, ruled 2026-09-19 —
SIXTEEN DAYS AFTER the architecture document:**

```
key {index:24}  ->  row {slot:8, base:32, extent:32, kind:8}
                    plus MEM.UPLOAD's 16-bit generation
```

**Four blocks already carry it:** `zhao_mem_upload`'s `publish_*` (producer,
**composed**), `zhao_geom_drawjob`'s `dir_*` (**composed, driven from exactly
those ports**), plus `zhao_material_resolve` and `zhao_geom_ladderbank` (built,
uncomposed).

> **POSECMD asked the right question and got the wrong answer because the ruling
> lives in `memory_rules`, not in the architecture report.**

That is R216's rule for the seventh time this week: **search the SUBJECT, not
the title** — and note the shape here is nastier than usual, because the
architecture document is *owner-ratified* and *scoped to this exact seam*, so
stopping there looked like diligence.

## 2. Two corrections to POSECMD's handover

**The EPOCH is not on the wire.** `zhao_mem_upload` refuses
`req_epoch_i != cfg_epoch_i` as `V_EPOCH_STALE`, and **a refusal never reaches
`publish_valid_o`** — so a dead-epoch row cannot exist to be checked.

**§6's consequence 5 is wrong about the silicon.** `zhao_guard_req_t` is
`{valid, write, client, addr, len, be}`: **the guard does the region half; the
REQUESTER does the generation half** against the generation in the fetched
bytes (`zhao_geom_meshfetch`'s `dh(32) != gen_q`).

## 3. Built

**`fpga/rtl/geometry/zhao_geom_clipread.sv`** — the kind-8 body / kind-9
clip-frame reader that fills `zhao_geom_bonesrc`.

**It obeys I29's stated law and the bench asserts it rather than assuming it:**
`src_req_o` rises only after **both** stores are whole, proven by *zero reads
and zero fills after the handover*, **with the handover asserted separately so
neither check is vacuous.**

**`bone_mismatch_o` differences two bone counts loaded by two different
publications in two different states** — not a blind checker, which is the
`CLAUDE.md` metadata-bank law applied at design time rather than discovered
later.

`geom_clipread_directed` **443 checks RC 0**; `geom_clipread_elab_guard` proves
the `CLIP_ROWS` `$fatal` fires (bare exe exits 1 with the line and the reason).
7/7 green through the tree's own ctest, **including `geom_bonesrc_latefetch_mutant`
with its inverted polarity intact.**

## 4. THE BLOCKER IT SURFACED — `form -> clip bank` is genuinely unruled

**This is not POSECMD's blocker and it is the reason I29 did not close.**

`zref::creature_page::Record` keys ladder rows by `form_index`. But
**`zref::clip_page`'s header carries no form index, no type key, and no handle
of any kind.**

> Wiring the draw through at single-resident tier would silently assert *"the
> resident bank is this draw's bank"* — **a correct palette for the wrong
> animal, invisible to `bone_mismatch_o` when bone counts match.**

**Three options are costed in the lane's commit; option (i) is one `u24` field
and one golden rebuilt.** *An owner decision, surfaced rather than invented.*

**Composition is also a subsystem boundary** — 830 ALM + 10,240 M10K,
`zhao_mem_share_n` N=5→6, ~15 `geom_pose_*` ports retired — **and owes one fit
the lane was forbidden to run.**

## 5. Three instrument findings

**1. `check(clips_o > 0)` hid a lying bench.** Strengthened to
`== before + 1` it went **red**: `publish()` ticked outside `cycle()`,
**injecting a duplicate beat into a read in flight.** `clips_o` 4 → 5 after the
fix, *exactly as the case count predicts.* **A threshold assertion passed over
a real defect that an equality assertion caught.**

**2. The WILL_FAIL guard fired AND HUNG** — 120 s ctest timeout, **and a
timeout is a failure whatever WILL_FAIL says.** `$fatal` never reached
`zhao::exit_hard`. Fixed with `VL_USER_STOP` / `VL_USER_FATAL` **on the target,
not in the `.cpp`** — Verilator's `#ifndef` lives in `verilated.cpp`, so a
local define **silently would not take.**

**3. It read a pipeline's exit code in the packet whose brief warns about it**,
and said so. `completion_register.py` exits **1**, not 0 — **and so does the
unmodified coordinator checkout at the same 22.**

## 6. Discipline held at the bounds

**No tie-off declared** — `zhao_console_core.sv` is **byte-identical to the
branch point.**

**The parked bound was not crossed** — `zhao_geom_ladderbank` was read *from*
as the pattern, **never composed or edited**; it reads the ladder table, this
block reads the body at `body_off`.

**The mutant and `zhao_geom_bonesrc.sv` untouched** (empty diff over the whole
branch), so **no three-way merge was owed.**

**Smoke forms not run under R227, and the claim was CHECKED rather than
asserted:** `zhao_geom_clipread.sv` appears in `tests/prod/` and
`fit_targets.yml` **zero times** and the core is unchanged, **so the bench
cannot elaborate it.** What the shared `tests/CMakeLists.txt` edit *did* owe is
a configure — `cmake --preset windows-native` clean in 239 s.

**Quartus not run, so per R212 the block has NOT been shown synthesizable.**
Stated by the lane, not extracted from it.
