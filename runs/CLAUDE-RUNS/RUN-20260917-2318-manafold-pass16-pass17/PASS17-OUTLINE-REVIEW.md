# Manafold Pass 17 Direction-17 outline review

**Date:** 2026-09-18
**Scope:** independent review of the complete-O outline implementation
**Verdict:** **PASS — source, gates and integrated durable evidence closed**

## Review result

No production-code defect survived review.

- `manafold_outline.h` keeps the O topologically negative: viewport-connected background is classified separately, and enclosed-boundary ink is written only to `mask == 1` creature pixels.
- The lower/inside owner is scoped to Manafold by the presence of the Manafold body-only auxiliary; no new enclosed-hole law runs on unrelated creatures.
- The accepted upper/head line still requires `body_cover` and exact body-only/full-frame depth equality. Nearer antenna geometry therefore suppresses hidden body ink exactly as before.
- The initial edge union feeds the same ink RGB, shell-protection mask and mist cover. O pixels are never added to edge or cover.
- The stale post-energy RGB restore is gone. Internal ink is painted once before the existing depth-tested population/splat passes, so a nearer effect remains the final owner while a behind effect still fails its own depth test.
- No geometry, animation, material, shell, mist, lightning, camera or ink-width value changed.

## Gate/build verification

Fresh direct build in `.tmp/p17-outline-review`:

```text
build-direct.sh --output .tmp/p17-outline-review --clean moutline
```

Result: **PASS**.

Fresh direct gate runs:

| Invocation | RC | Expected observation |
|---|---:|---|
| normal | 0 | 9/9 enclosed O pixels; 12/12 creature-side witnesses; 0 O pixels inked; top line kept; nearer creature suppressed; nearer effect retained; mist cover correct |
| `--selftest-no-opening-owner` | 1 | only complete inner-boundary ownership fails (0/12) |
| `--selftest-force-post-repaint` | 1 | nearer-effect ownership fails; stale ink erases it |

CMake registration was independently configured and built from PowerShell with `zhao-env.ps1` sourced:

```text
cmake --preset windows-native
cmake --build --preset windows-native --target manafold-outlinegate
build/tools/manafold-outlinegate.exe
```

The CMake-built executable returned **RC 0** with the same six green checks.

`git diff --check` on the outline packet is clean.

## Scope isolation

The implementation replaces the old inline exterior flood with the shared pure helper, but preserves its four-neighbour/border semantics. The new enclosed owner runs only when the Manafold body auxiliary exists.

Protected Zixxtrixx evidence was independently rehashed:

```text
before: 577 files
 after: 577 files
compared: 577
mismatch: 0
```

Thus the non-Manafold cel path remains byte-identical in the supplied protected render.

## Visual review

Reviewed in this isolated image context:

- `.tmp/p17-outline-fixed-sheet.png` — all 420 fixed-camera presentation frames;
- `.tmp/p17-outline-quarter-sheet.png` — all 420 oblique presentation frames;
- `.tmp/p17-outline-channel-sheet.png` — all 420 effects-on presentation frames;
- `.tmp/p17-outline-fixed-4x.png` — exact nearest-neighbour enlarged witnesses;
- `.tmp/p17-outline-channel-4x.png` — exact enlarged effect crossings.

The normal render is visually correct:

- the full inner O boundary, including both lower/inside stick edges, carries coherent black ink;
- the O stays open rather than becoming creature fill;
- the accepted upper/head-top line remains present;
- front and oblique motion retain coverage without an unowned gap;
- cyan lightning remains visibly in front where it crosses the inner line—there is no stale black repaint through it;
- shell/mist remain outside their previous ownership boundaries.

## Integrated evidence closure

The evidence was regenerated from the final 23-bone posed-ring-checked binary (MD5 `F7C2DAA085661C1E00CADF09C65B5F8A`):

1. all 420 fixed, 420 quarter and 420 effects-on Channel frames are durable final sheets in the run folder;
2. same-binary `no-opening-owner` native/4× A/Bs visibly remove only the lower/inside O ink while the O stays open and the accepted top line remains;
3. same-binary `force-post-repaint` native/4× A/Bs visibly restore the rejected black-through-lightning fault (clearest at Channel f0345), while normal preserves nearer cyan ownership.

The explicit `none` control is 577/577 Zixxtrixx Idle files byte-identical to unset. Final normal CRCs are fixed `0x8F08D358`, quarter `0x2CC13DC4`, and Channel `0xB89B63D1`. The `PASS17-OUTLINE-FINAL23-*` files supersede earlier 22-bone evidence. Direction 17 is closed.
