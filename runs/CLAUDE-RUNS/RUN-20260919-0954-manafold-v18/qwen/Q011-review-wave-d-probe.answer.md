# Q011 answer — review-wave-d-probe

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: b9c8a14e9dc14d0094814691b646e8f1
- when: 2026-09-19T18:35:47  seconds: 80  finish: stop  status: ok
- usage: {"completion_tokens": 6782, "completion_tokens_details": {"reasoning_tokens": 5240}, "prompt_tokens": 4384, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 11166}
- inputs: [{"input": "git show 50803207 -- tools/reel/manafold_probe.cpp", "sha256": "b155b1ded978c955", "chars": 12048}]
- kind: task

## Answer

## Findings

### Hardening 1 – Ties must not count as owned

**Correctly implemented.** The ownership test at the new line (the `owned` assignment inside the `if (in_window)` block) uses strict less-than:

```cpp
const bool owned = sample_support_min < sample_nonsupport_min;
```

A tie (`==`) makes `owned` false, so a shared contact is counted as a miss. The only edge case is "no non-support vertices exist," which leaves `sample_nonsupport_min == INT32_MAX`; the support value (a real Q16.16 y) is still `< INT32_MAX`, so support is correctly deemed to own the sole contact. No path lets a non-support vertex at the same or greater depth pass as owned.

### Hardening 2 – Region membership on the UNDEFORMED bind vertex

**Correctly implemented.** The call site reads:

```cpp
const zc::SkinVertex& bind_v = m.verts[vi];   // original, pre-deform
zc::SkinVertex sv = bind_v;
if (!m.deform.empty()) sv = zc::deform_skin_vertex(sv, m.deform[vi], d);
```

and the membership test is invoked on `bind_v`:

```cpp
if (is_trick_support_vertex(bind_v)) {
```

Inside `is_trick_support_vertex`, the Y used for the station band is `v.y` (the bind Y in Q16.16), converted to mm. The deformed copy `sv` is never passed to the membership test. A vertex cannot drift in or out of the support region frame-to-frame.

### Hardening 3 – Nonzero weight on the carrier bone

**Correctly implemented.** The weight logic:

```cpp
const bool b0_live = v.b0 == bone && v.w0 > 0;
const bool b1_live = v.b1 == bone && v.w0 < 64;
```

The in-code comment states "w0 is b0's weight in 1/64; b1 carries 64 − w0." So `b1_live` requires `64 − w0 > 0`, i.e. `w0 < 64`. Both branches demand the vertex actually carries weight on the declared carrier bone. A vertex whose carrier is the other bone (or whose weight on the carrier is zero) is excluded.

### Hardening 4 – Wrong-support control fails ONLY through ownership

**Correctly implemented.** Two sub-points:

- **Ownership operand is lift-free.** `sample_support_min` is the raw deformed y; the +40 mm lift is added only to the local `support_mm` variable:
  ```cpp
  const int32_t support_mm = static_cast<int32_t>(
      (static_cast<int64_t>(sample_support_min) * 1000) >> 16) +
      (g_fail_trick_support_depth ? kTrickSupportDepthControlLiftMm : 0);
  ```
  The `owned` comparison uses `sample_support_min` directly, so the depth control cannot perturb ownership.

- **Depth check is gated on ownership.** The depth-band test is inside `if (owned) { … }`, so when the wrong-support flag swaps the carrier to A (making `sample_support_min` the A-bound vertex, shallower than the true B-bound deepest point), `owned` is false and the depth test is never reached. The failure is attributed solely to `trick_support_owned < expected_samples`.

Conversely, the depth control (`--fail-trick-support-depth`) leaves `g_fail_trick_support` false, so carrier = 2 (B), ownership passes, and only the lifted `support_mm` exceeds the band. Each control fires for exactly one reason.

### Question 3 – Sample-count window consistency

**Correctly derived.** The expected count:

```cpp
const size_t expected_samples = static_cast<size_t>(
    (u02::kTrickLiftKey - u02::kTrickPlantKey) * 2);
```

uses the same two constants that define the loop guard:

```cpp
const bool in_window = has_window && f >= u02::kTrickPlantKey && f < u02::kTrickLiftKey;
```

The outer frame loop runs `f < clip.frame_count` and the inner loop covers `sub ∈ {0,1}`, so the number of `in_window` iterations is exactly `(kTrickLiftKey − kTrickPlantKey) × 2`. Both the counter and the expected value reference the identical constants; a change to either constant rescales both together. (If `clip.frame_count < kTrickLiftKey` the counter would be short and the check would fail—correct fail-closed behaviour.)

### P1 / P2

**None found.**

### P3 (nit)

`sample_all_bind_y` is assigned `sv.y` (the **deformed** Y) but the name says "bind." It is consumed only in a diagnostic `printf` (the "bind-y" field of the miss message); it plays no role in any pass/fail logic. Misleading name, no functional impact.

---

## CONTINUATION
- Done:
  - All four hardenings (tie-rejection, bind-vertex membership, nonzero-weight, control-isolation) are implemented correctly; cited above.
  - Positive controls fire for the intended single reason each; ownership and depth are independent operands.
  - Expected sample count and the actual counting loop share the same `kTrickPlantKey`/`kTrickLiftKey` constants; a window change rescales both.
  - No P1 or P2 defects identified in the diff.
- Open:
  - The outer guard that limits the support-check block to trick clips is not visible in this diff chunk (the `wok` block's enclosing `if` is above the diff hunk). If that guard were ever removed, a non-trick clip (where `has_window` is false, so `trick_support_samples` stays 0) would fail `support_ok` because `expected_samples > 0`. Worth confirming the enclosing condition in the full file.
  - The values of `kLoopCarrierCoreAtMm[1..2]`, `kKnuckleSwellHalfMm[1..2]`, `kLoopNeckExitYMm`, `kLoopBuryMm`, `kTrickDepthMinMm`, `kTrickDepthMaxMm` are in the `u02` header, not shown here; the ±1 mm tolerance on the station band is small but its adequacy depends on those constants.
- Next chunk: Verify the enclosing guard around the `wok` / support-check block in `manafold_probe.cpp` (the `if` that limits execution to trick clips) to confirm a non-trick slot never enters the support-ownership assertion. Input needed: the ~20 lines above the diff hunk starting at the `if (!wok) rc = 1;` line.

## Coordinator verdict

**verified** — spot 2/2: membership uses undeformed bind_v (probe.cpp:287); P3 misnamed sample_all_bind_y=sv.y (282) is diagnostic-only - correct. Wave-D probe: no P1/P2.
