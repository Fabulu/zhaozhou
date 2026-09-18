# Manafold Pass 17 Fall one-shot review

**Date:** 2026-09-18
**Scope:** exact f0338/f0339 normal and same-binary legacy-wrap control; source and site playback contracts
**Session preview (deferred from this evidence checkpoint):** `PASS17-FALL-HOLD-VS-WRAP.png`

The preview's frame hashes and visual comparison are preserved below, but no durable run record ties its renderer binary to an exact source state. The PNG is therefore excluded from the structural evidence commit and must be regenerated from the final clean Pass-17 bank. Only the source Q5 and site playback contracts are accepted durably here.

## Visual verdict

**SESSION VISUAL PASS, pending final-bank reproduction.** The comparison clearly answers the art question, but the preview is not curated evidence without a renderer/source receipt.

At native resolution, `HOLD338` and `HOLD339` keep the conduit grounded in the same recovered pose. The small visible change is living scene/effect motion: the antenna/body silhouette, eye placement, ground contact and framing do not reset. The one-shot therefore reaches recovery and holds it rather than freezing the whole rendered world.

The same-binary control reproduces the rejected defect unambiguously. `WRAP338` is grounded recovery; `WRAP339` jumps the complete creature to the high opening pose at the top of frame. This is not a subtle interpolation change or camera drift—the grounded subject disappears and the opening tumble reappears in one presentation half-frame.

The native four-tile plate is already decisive; an enlarged crop is unnecessary and would be misleading because the defect moves the creature between different parts of the frame.

## Exact frame evidence

| Path | Changed pixels f0338→f0339 | Mean absolute channel delta | Maximum channel delta | Read |
|---|---:|---:|---:|---|
| hold-last | 1,382 | 0.1784 | 154 | ambient/effect motion; creature pose holds |
| legacy wrap control | 8,873 | 4.8073 | 189 | whole-pose restart to opening height |

SHA-256 receipts:

- hold f0338: `f221a60833f79d66f87911b60766eb41777c160dd30b7f125cc6fa203edabcc5`
- hold f0339: `7dfb664c4c4376deaddd0a46428c9f211e2776967b7ca5acd8bba015912c5ffd`
- control f0338: `c1ea18a8d12f807d5781b3efa8702ef126ee448620e2696046915310c75d2347`
- control f0339: `408845dfadd39ad8646fd43e490a4739f7bdcbe3caeecdcae73407306f055099`

The numbers corroborate the picture; they do not substitute for it.

## Source contract

`tools/reel/manafold_clips.h` makes the intent explicit in `build_fall()`:

```cpp
c.hold_last = !g_u02_fall_wrap_control;
c.wrap_root_delta = g_u02_fall_wrap_control;
```

Normal production therefore holds the final partner for quaternions, local translations, deformation and root. `ZHAO_U02_FALL_WRAP_CONTROL=1` restores the Pass-16 root-delta-only wrap in the same binary and produced the red visual control above.

Committed Q5 in `manafold_qa_p12.cpp` now checks the source contract directly. `--fall-only` returns RC 0 with exact f338/f339 equality across resolved root XYZ, every bone quaternion, local translations, uniform scale, primary deformation and all extra lanes. `--fail-fall-wrap` returns RC 0 only after Q5 rejects the legacy control for its flags and non-root pose reset. The rear midpoint finalizer also preserves bit-identical authored quaternions on a held segment rather than renormalising identical endpoints or re-solving the held final closure.

## Site playback contract

The website path is coherent:

- `creatures.json` declares live Fall with `"loop": false`.
- `assemble.py` validates `loop` as boolean and emits a one-shot WebM as `controls muted playsinline`, with neither `autoplay` nor `loop`.
- Generated `public/index.html` has exactly that Fall element, while Hover retains `autoplay loop muted playsinline`.
- `checkplayback.py --selftest`: **PASS**; both looping-Fall and missing-Hover-loop red legs fire.
- `checkplayback.py website`: **PASS**; current generated HTML matches the contract.

## Remaining closure item

The live media is still the Pass-16 generation until Pass 17 finishes. The source pose gate, non-looping HTML contract and corrected live note are green (Upheaval `851be73`), but the finished Pass-17 publication must encode the new hold-last frames and pass freshness, full decode and production-byte verification before this fix is live.

## Acceptance sentence

Fall's animation/source/site correction is accepted structurally: production holds the complete grounded recovery pose and the page is controls-only with accurate copy. The session preview reproduced the rejected hard restart, but durable visual delivery remains open until the final clean Pass-17 bank regenerates the hold/control evidence, then passes encode, freshness, full decode and production-byte verification.
