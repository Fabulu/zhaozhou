# Q012 answer — review-wave-d-spangate

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: c701c14b188842b193e3627546d51d94
- when: 2026-09-19T18:37:06  seconds: 170  finish: stop  status: ok
- usage: {"completion_tokens": 13480, "completion_tokens_details": {"reasoning_tokens": 11606}, "prompt_tokens": 7275, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 20755}
- inputs: [{"input": "git show 50803207 -- tools/reel/manafold_spangate.cpp", "sha256": "7a32f9d1919368d5", "chars": 19484}]
- kind: task

## Answer

## FINDINGS

none found

## Explanation

I reviewed the three new checks in `manafold_spangate.cpp` (commit 50803207) against the five questions in the brief.

### Q1 – Terminal cap: confinement and burial

`check_terminal_cap()` (diff lines ~1230–1280) builds three `RingPart`s via `u02::make_loop()` under different `g_u02_terminal_cap_control` values: `collapsed` (false), `authored` (true), and `actual` (saved/production). The loop over all rings counts:

- `profile_diffs` – rings where `rx` or `rz` differs between collapsed and authored.
- `nonterminal_diffs` – any such ring that is **not** the last (`i + 1 != rings.size()`).
- `collateral` – any non-rx/rz field (`y, radius, cx, cz, segments, b0, b1, w0`) that differs between collapsed and authored, on **any** ring.
- `actual_mismatch` – any field that differs between collapsed and actual.

The final assertion requires `profile_diffs == 1`, `changed_ring + 1 == rings.size()`, `nonterminal_diffs == 0`, `collateral == 0`, and `actual_mismatch == 0` (unless the positive control fires). This proves exactly one ring differs and it is the last. The `exact_profile` sub-check pins the cap to `kReturnTipCapRxMm`/`kReturnTipCapRzMm` and the full ring to 42/26 mm via `kLoopBladeRxMm[6]`/`kLoopBladeRzMm[6]` (file:~1267).

Burial is **not** checked inside `check_terminal_cap` itself. The `--fail-terminal-cap` control sets `g_u02_terminal_cap_control = true` (diff ~line 1902), and its `select_mutant` allows `kCatTerminalCap | kCatRootAuthority` (diff ~line 1962). The burial detector lives in the RootAuthority check (not shown in this diff), so it fires under the same run and is permitted by the allowed-category mask. Whether it inspects every shipping key and midpoint is not visible in this diff — **not shown**.

### Q2 – Swell size: independent operands?

`check_swell_size()` (diff lines ~1175–1230) calls the **same** `u02::make_loop()` four times with different global settings:

| build | `g_u02_swell_pm` | `g_u02_swell_legacy` |
|-------|-------------------|-----------------------|
| `selected` | 1000 | false |
| `legacy` | 1000 | true |
| `no_swell` | 0 | false |
| `actual` | saved (production) | saved (production) |

The distinctness assertions (`selected_legacy_diff != 0`, `selected_live_rings != 0`) compare **rx/rz** between `selected` and each of the other two modes. If a bug made `make_loop` ignore its globals entirely, all four `RingPart`s would be identical, `selected_legacy_diff` would be 0, and the check would fire. If a bug made production use a non-selected mode, `actual_mismatch` (which also compares `y, b0, b1, w0`) would fire. The two operands that "move together" (`selected` and `actual`) are only compared to verify production-mode agreement; the real canary role is carried by the distinctness checks against `legacy` and `no_swell`. A detector wired to two identical operands *can* fire here because the distinctness comparisons provide independent operands.

### Q3 – Front flex: 20 mm floor on visible vertices

`check_front_flex()` (diff lines ~1100–1175) rebuilds each of the 10 clips with `mute=false` (`normal`) and `mute=true` (`muted`) inside the **same process** via `build_front_flex_clip(slot)`. Per-frame, it calls `pjm::core_delta(type, normal, muted, f, front)` where `front = pjm::carriers()[0]`, and takes `d.max_vertex_mm` (a vertex-displacement metric on the front carrier). The floor check is `weakest_max_mm < pjm::kVisibleEffectMinMm` (diff ~line 1160). A clip with zero Front motion would set `clip_changed == 0` → `missing_core++` → the `clips != 10 || changed_frames == 0 || missing_core != 0` guard fires, and `weakest_max_mm` would be 0.0 (or stay at `infinity` if every frame has `d.count==0`, in which case `missing_core` still trips). A clip cannot pass with no visible Front motion.

The `kVisibleEffectMinMm` constant value is **not shown** in this diff; the brief states 20 mm.

### Q4 – Attribution isolation

Each `fail()` call sets `g_failure_bits |= g_current_category`, and the caller sets `g_current_category` to the single category under test before invoking the check (diff ~lines 2079–2088). The three new controls are registered via `select_mutant` with allowed-category masks:

- `--fail-front-flex` → expected `kCatFrontFlex`, allowed `{kCatFrontFlex}`
- `--fail-swell-size` → expected `kCatSwellSize`, allowed `{kCatSwellSize}`
- `--fail-terminal-cap` → expected `kCatTerminalCap`, allowed `{kCatTerminalCap, kCatRootAuthority}`

No control can silently fire another category's bit. The only cross-category allowance (terminal-cap → RootAuthority) is by design per the brief ("must fire the burial detector"). The `--fail-root-authority` control's allowed set was widened to include `kCatFrontFlex | kCatCrown` (diff ~line 1955), which is correct because muting root authority can cascade into front-flex and crown-order checks.

### Q5 – P1/P2

None found. The checks are internally consistent, the attribution masks are correctly scoped, and each positive control (`saved_mute`, `saved_control`, `saved_legacy`) fires its own category bit.

## CONTINUATION
- Done: Verified (1) terminal-cap check confines diff to the last ring via `profile_diffs==1` + `nonterminal_diffs==0` + `collateral==0` and pins cap to 2 mm / full to 42/26 (diff ~1240–1280); (2) swell-size distinctness checks compare rx/rz across three independently-parameterised `make_loop()` calls, catching global-ignoring bugs (diff ~1175–1230); (3) front-flex measures `core_delta().max_vertex_mm` per frame on `carriers()[0]`, compares normal-vs-mute in-process, and floors at `kVisibleEffectMinMm` (diff ~1100–1175); (4) each `--fail-*` control fires only its own category bit, with terminal-cap→RootAuthority as the sole allowed cross-fire (diff ~1955–1965). No P1 or P2 identified in this diff.
- Open: The implementation of `pjm::core_delta`, `pjm::kVisibleEffectMinMm`, `u02::kReturnTipCapRxMm`/`RzMm`, and the RootAuthority burial check are not in this diff; their absolute correctness and the "every shipping key AND midpoint" burial coverage remain unverified.
- Next chunk: Review the RootAuthority check (wherever it lives, likely earlier in `manafold_spangate.cpp` or in a `u02` header) to confirm the burial detector inspects all shipping keys **and** midpoints of the terminal ring, and verify `kVisibleEffectMinMm == 20.0` and `kReturnTipCapRxMm/RzMm == 2` in the u02 constants header. Inputs needed: the full `manafold_spangate.cpp` (pre-diff context for `check_root_authority`), the `u02` constants/geometry header, and the `pjm::core_delta` implementation.

## Coordinator verdict

**verified** — spot 2/2: terminal-cap check counts profile/nonterminal/collateral diffs (spangate.cpp:1280-1315); terminal-cap->RootAuthority cross-fire matches worker's 0x4800. No P1/P2; honest 'not shown' on burial-coverage internals.
