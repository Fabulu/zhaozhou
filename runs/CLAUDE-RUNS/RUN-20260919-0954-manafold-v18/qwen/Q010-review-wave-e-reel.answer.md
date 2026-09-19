# Q010 answer — review-wave-e-reel

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 712741e4a98348b88e52b252eb23705f
- when: 2026-09-19T18:25:23  seconds: 144  finish: stop  status: ok
- usage: {"completion_tokens": 11494, "completion_tokens_details": {"reasoning_tokens": 9638}, "prompt_tokens": 8370, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 19864}
- inputs: [{"input": "git show 0381bdec -- tools/reel/zhao_reel.cpp", "sha256": "5c9657fce3239cb0", "chars": 22066}]
- kind: task  continues Q009

## Answer

## Findings

**Q1 — Mote fade math**

The 120 mm band is applied in *mm depth space*, not in raw 1/w. Both the mote and the surface pixel are converted with `65536000LL / d` (line ~3823: `zm = 65536000LL / pm.s.d`; line ~3836: `zs = 65536000LL / depth[si]`), which yields millimetres when `d` is a 16.0 fixed-point 1/w (1/w in metres → 65536·1000 / (1/w) = w in mm). The gap is the absolute mm difference (`gap = zm > zs ? zm - zs : zs - zm`), so the fade is symmetric: a mote 60 mm in front *or* 60 mm behind the surface is faded equally. A mote 200 mm away (gap ≥ band = 120) gets f = 1000 → no fade. Sign is correct.

The smoothstep `f = t·t·(3000−2t)/10⁶` with `t = gap·1000/band` is the standard cubic in [0,1000]; slope is zero at both ends (f′(0)=f′(1000)=0). No overflow: max `t` is 999 (integer division), so the numerator is < 10⁹, well within `int64_t`.

Bounds: taps outside the frame get `f = 1000` (no fade), so a mote at the frame edge is not falsely faded. All 25 taps out-of-bounds → `f_sum = 25000` → `f = 1000` → no fade.

"Off" path: when `g_u02_mote_surface_fade` is false the entire `if` block (zhao_reel.cpp:~3818) is skipped; the local copies `gain_pm/opacity_pm/soft` equal the struct fields, so `glow_frame_cached` and `glow_splat` receive identical values to the pre-commit code. Data-only. ✓

**Q2 — Antenna mask and per-frame construction**

The per-tap predicate is `depth[si] > 0 && depth[si] != pre_depth[si] && !u02_body_cover[si]` (line ~3835). This is "creature-owned pixel that is not body" — the antenna. `pre_depth` is size-checked against `w*h`; if it mismatches the fade is skipped. `u02_body_cover` is guarded only by `.empty()` (line ~3818). If the vector is guaranteed to be `w*h` by the code that builds it (not shown in this diff), the frame-bounds check on `sx/sy` is sufficient. If it could be a different size, the per-tap index `u02_body_cover[si]` would be OOB. P3: the guard is asymmetric with `pre_depth`'s exact-size check.

If the mask is empty (no creature on screen), the `.empty()` guard skips the fade entirely. If all taps hit pre-existing scene depth (no antenna), every tap keeps `f = 1000`. Both are correct no-ops.

**Q3 — Live list and self-check**

The list `kU02LiveSiteSubjects` (zhao_reel.cpp:~4010) is 22 entries enforced by `static_assert`. `u02_is_live_site_subject` does exact string match. A name typo in the array would cause `u02_is_live_site_subject` to return false for that subject, silently skipping the check — but the committed Python gate (`manafold_live_history_gate.py`) cross-validates the table against `creatures.json`. A subject added to the JSON but not the C++ array would also escape the in-binary check (caught by the Python gate).

The self-check (zhao_reel.cpp:~4910) reads both the declared flags (`sub.u02_mist`, `sub.u02_smear`) *and* the executed receipts (`u02_mist_frames_run`, `u02_smear_frames_run`). The receipts are incremented inside the actual render blocks (zhao_reel.cpp:~3605 and ~3703), so a future code path that activates a plane without the flag is still caught.

`live_history_rc` is returned from both the `!g_write` and `g_write` paths (zhao_reel.cpp:~4930, ~4990) and ORed into the global `rc` in `main` (`rc |= render_scene(s)`). RC 5 propagates to process exit. ✓

`ZHAO_U02_LIVE_MIST=legacy` sets `g_u02_live_mist_legacy = true`, which makes `subject_u02_clip` set `s.u02_mist = true` for all non-slot-7 clips. The self-check then fires (dirty → RC 5). This is the positive control. ✓

**Q4 — Crackle legacy isolation**

`manafold-crackle-legacy` (zhao_reel.cpp:~9090) is **not** in `kU02LiveSiteSubjects` (only `"manafold-crackle"` is), so the self-check does not fire. It explicitly sets:
- `s.u02_mana = 4` (old candidate)
- `s.u02_smear = 0`
- `s.u02_mist = true` (overrides the builder's new default of false)
- `u02_backdrop(s, 58, 96, 132)` (night sky)
- Saves/disables/restores `g_u02_mote_surface_fade`

No fade, no live-list check, no new mist retirement. The only shared mutable state is `g_u02_mote_surface_fade`, which is save/restored around the `render_scene` call. If `render_scene` returns early internally (e.g. file open failure → `return 2`), control returns to `main` and the restore line still executes. No leak. ✓

**Q5 — P1/P2 findings**

None found.

P3 notes (not inflated):
- P3: `u02_body_cover` guard is `.empty()` while `pre_depth` is exact-size; if the body-cover vector were ever sized differently from the frame, the per-tap read would be OOB. In practice it is frame-sized by construction.
- P3: The 22-name array relies on the external Python gate for typo/mismatch detection; the `static_assert` only guards count.

## CONTINUATION
- Done: Fade math is correct (mm-space, symmetric, smoothstep, bounded, off-path clean) at zhao_reel.cpp:~3818–3870. Antenna mask is creature-minus-body, per-tap, with frame-bounds guard. Live self-check reads declared flags *and* executed receipts, returns RC 5, wired to process exit. `manafold-crackle-legacy` is isolated from fade, live-list check, and mist retirement. No P1/P2.
- Open: (a) `u02_body_cover` sizing guarantee (built elsewhere, not in this diff). (b) Default value of `g_u02_mote_surface_fade` and `g_u02_mote_surface_fade_mm` in `manafold_fx.h` (presumably `true` / `120` respectively, but not confirmed here). (c) The Python gate `manafold_live_history_gate.py` — does it actually run in CI and fail on mismatch?
- Next chunk: Confirm the default initialisation of `u02::g_u02_mote_surface_fade` and `u02::g_u02_mote_surface_fade_mm` in `manafold_fx.h` (or wherever the globals are defined), and verify `u02_body_cover` is always sized `w*h` before `creature_hook` reads it. Inputs needed: `manafold_fx.h` (global declarations/initialisers) and the code path that populates `u02_body_cover`.

## Coordinator verdict

**verified** — spot 4/4: mm conversion 65536000/d at 3829/3842 OK; executed-plane receipts at 3608/3706 feed self-check 4919 OK; crackle-legacy sets mana 4 + mist true + saves/restores fade (9104-9114) OK; no P1/P2 - consistent with worker's 97/97. Independent Wave-E source review: clean.
