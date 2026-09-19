# Q009 answer — review-wave-e-fx

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: e681bde4ce4f4f98a51dfdf208ee83f9
- when: 2026-09-19T18:23:27  seconds: 90  finish: stop  status: ok
- usage: {"completion_tokens": 6264, "completion_tokens_details": {"reasoning_tokens": 4175}, "prompt_tokens": 2507, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 8771}
- inputs: [{"input": "git show 0381bdec -- tools/reel/manafold_fx.h", "sha256": "42351131d193fca8", "chars": 6017}]
- kind: task

## Answer

# Review: Wave E Mote Surface Fade (0381bdec) — `manafold_fx.h` chunk

## Q1: Is the "off" path byte-identical?

From this file alone: **yes, no differing arithmetic/rounding/clamping/branch executes when off.**

- `mana_push` (line ~2219 in the new file) gains a `bool surface_fade = false` parameter. Its body is unchanged except it stores the bool into the new struct field (`tools/reel/manafold_fx.h`, the `out.push_back(...)` line). No comparison, multiply, or clamp on `surface_fade` exists in `mana_push`.
- The only new executable code in this file is the `static_assert` (compile-time, zero runtime cost) and the initialisation of the two globals `g_u02_mote_surface_fade` / `g_u02_mote_surface_fade_mm` (one store each at program start, identical regardless of flag).
- No `if (g_u02_mote_surface_fade)` or branch on `splat.surface_fade` appears in the shown diff. The gate must live in the renderer splat-pass code (not shown).

**Caveat:** the `ManaSplat` struct gains a `bool` member (diff line adding `bool surface_fade;`). This changes `sizeof(ManaSplat)` by at least 1 byte (plus padding). If the struct is ever serialised, memcpy'd into a fixed-size buffer, or compared byte-for-byte between builds, the size change alone breaks "byte-identical." No such usage is visible in this diff. Verdict: *no arithmetic difference; struct-size difference is a potential issue only if the struct is used in a size-sensitive context not shown.*

## Q2: Depth-space / mm-to-1/w conversion

**Not shown.** The constant `kMoteSurfaceFadeMm = 120` (line ~2216) is in millimetres, but the depth buffer holds 1/w values (larger = nearer, per the brief). The conversion code—whether it converts the 120 mm delta into a 1/w window at the mote's depth, or converts both the mote and surface 1/w back to mm before comparing—lives in the renderer pass that was not included in this diff.

Because 1/w is non-linear in z, a fixed 120 mm window in *z* maps to a different 1/w width at different depths. A naive "add/subtract a constant to the 1/w value" would be wrong (too wide far away, too tight up close, or vice-versa depending on camera model). I cannot confirm or deny correctness from this file.

**Sign / direction:** The comment states "Motes clearly in front of or behind a stick… are untouched" and that "240 began fading motes genuinely in front of a stick," which implies the author verified the sign empirically. No comparison operator is visible in this diff to confirm the direction programmatically.

## Q3: 5×5 footprint — bounds, overflow, zero-division, depth source

- `kMoteSurfaceFadeTaps = 5` (line ~2217); 25 samples total.
- **Bounds at frame edges, integer overflow, and the zero-valid-taps guard are all in the renderer code, not shown.** No loop, no accumulator, no `/25` or `if (count == 0)` appears in this diff.
- **Depth source:** The comment (line ~2205) states the fade "uses depth the renderer already has" and the brief confirms depth is not written by splats. The comment further specifies the mask: "wherever a pixel is covered by the creature but NOT by the body-only pass (i.e. by the antenna)." This implies a pre-existing scene depth buffer and a separate body-only mask, neither of which is generated or modified in this file. So the 5×5 tap reads scene depth, not particle depth. **Confirmed from the comment; the actual load code is not shown.**
- 25 taps × 1000 (permille max) = 25 000 — no 32-bit overflow risk, but again the accumulation code isn't here.

## Q4: Which splat kinds are affected?

From every `mana_push` call visible in the diff:

| Caller | surface_fade arg |
|---|---|
| Surge halo (`mana_lightning`, ~line 2222) | `true` |
| Surge core (opaque, ~line 2225) | `true` |
| Fold halo (`mana_fold`, ~line 3673) | `true` |
| Fold core (opaque+soft, ~line 3677) | `true` |

All other `mana_push` callers in the diff (or the default `false` parameter) leave the flag off. The struct comment (line ~1831) explicitly states: *"Lightning, endpoints and every other splat leave it false and are never touched by that fade."* No lightning-trace, strand, bullet, or endpoint `mana_push` in this diff passes `surface_fade=true`.

**Residual risk:** the diff shows only the changed lines. If any other translation unit calls `mana_push` with positional arguments and omits the new trailing parameter, the default `false` applies — safe. But if any call site passes arguments positionally beyond `opacity_pm` (unlikely given the new parameter is last and defaulted), it would compile without error. No such call is visible here.

## Q5: P1 / P2 findings

**P1: none found in the shown code.** No arithmetic error, no sign error, no unguarded division, no off-by-one visible. The actual fade computation (where a P1 would most likely hide) is in the renderer pass, not in this diff.

**P2:**

1. **Struct size change is unverified as harmless.** Adding `bool surface_fade` to `ManaSplat` (line ~1831) changes `sizeof(ManaSplat)`. If any code does `sizeof(ManaSplat)` in a buffer allocation, a file-format header, a cross-process message, or a CRC over the raw struct, the "byte-identical" claim for the off path is broken by layout alone, even though no field value changed. Not verifiable from this diff — **not shown** whether such a consumer exists. *(P2, fragile / misleading if any such consumer is assumed stable.)*

2. **`g_u02_mote_surface_fade` is a mutable `inline bool` global** (line ~2219). Any code path that writes it (e.g. a test harness toggling the flag mid-frame, a debug command) changes behaviour globally with no synchronisation. For a single-threaded renderer this is fine; if the renderer is multi-threaded (e.g. GPU worker threads), a torn read is possible. **Not shown** whether threading is involved. *(P2, fragile.)*

**P3 (nit):** the comment "SELECTED BY EYE, Wave E" and the env-var name `ZHAO_U02_MOTE_SURFACE_FADE_MM=<1..2000>` imply the mm constant is tunable at runtime, but `kMoteSurfaceFadeMm` is `constexpr`. The runtime override must be in `g_u02_mote_surface_fade_mm` (the `inline int32_t`), which is a separate variable. The `constexpr` and the `inline` global coexist but the relationship (which one the renderer actually reads) is not shown. *(P3, minor naming/clarity nit.)*

---

## CONTINUATION
- **Done:** Established that in `manafold_fx.h` (commit 0381bdec) the "off" path introduces no new arithmetic, rounding, clamping, or branch; only a struct-field store. Confirmed that `surface_fade=true` is passed only for surge (halo+core) and fold (halo+core) `mana_push` calls; all other splat kinds default to `false`. Confirmed the depth buffer is scene-provided (not splat-written) per the in-file comment. The mm→1/w conversion, 5×5 sampling loop, bounds guard, zero-tap guard, and the actual `if (g_u02_mote_surface_fade)` gate are **not in this file**.
- **Open:** (a) Where in the renderer is `g_u02_mote_surface_fade` checked and where is the 5×5 depth-sample loop? (b) How is `kMoteSurfaceFadeMm` (or `g_u02_mote_surface_fade_mm`) converted into a 1/w window at the mote's depth — is it a depth-aware conversion or a flat 1/w offset? (c) Is `sizeof(ManaSplat)` used in any file-format, IPC, or CRC path? (d) Are there other `mana_push` call sites in other files that could accidentally pass `surface_fade=true`?
- **Next chunk:** Locate and review the renderer splat-pass code that reads `splat.surface_fade` and `g_u02_mote_surface_fade` (likely in the main render loop or a `render_splats` / `draw_splats` function). Needed inputs: the file containing the depth-buffer read loop, the 1/w↔mm conversion, and the env-var parser for `ZHAO_U02_MOTE_SURFACE_FADE`.

## Coordinator verdict

**verified** — spot 3/3: 4 surface_fade=true call sites (fx.h 2226/2229/3677/3681) OK; off path in fx.h is data-only OK; correctly said fade math is not shown instead of guessing. P2s (struct size, global) are low value - ManaSplat is in-memory only.
