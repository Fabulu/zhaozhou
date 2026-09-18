# Manafold Pass 17 complete O-outline implementation

**Date:** 2026-09-18
**Direction:** Owner Direction 17
**Audit:** `PASS17-OUTLINE-AUDIT.md`

## Production repair

The fix changes coverage ownership, not form or an art value.

- Added `manafold_outline.h`, a pure mask helper shared by production and the committed gate.
- The Manafold body-only auxiliary now follows the cel-outline path rather than the shell switch, so shell-off diagnostics retain the accepted upper/head-top line.
- Viewport-connected exterior is still flood-filled from the border. Manafold now also selects **visible full-mask creature pixels** bordering a non-mask, non-exterior component. Ink lands on the stick side only; O pixels remain untouched negative space.
- The existing body-only depth equality remains intact. A nearer antenna surface still suppresses hidden body-derived ink.
- Removed the unconditional post-energy internal-ink RGB restore. Initial ink remains protected from shell/mist; later effects that pass their depth test remain in front, while effects behind the creature still fail their existing depth test.
- The enclosed-hole rule is explicitly species-scoped through the Manafold body auxiliary. A protected Zixxtrixx Idle render remains 577/577 files byte-identical.

No geometry, skinning, camera, animation, ink width/colour, shell, mist, lightning or texture value changed.

## Committed gate

`manafold_outlinegate.cpp` calls the production helpers and checks:

1. a 3x3 enclosed O is classified as negative space;
2. all twelve creature-side inner-boundary witnesses are ink-owned;
3. zero O pixels are selected for ink;
4. the visible body/head top line remains and a depth-mismatched nearer creature surface suppresses body ink;
5. an effect behind the ink remains occluded and a nearer effect remains the final RGB owner;
6. every ink pixel is in mist cover while all O pixels remain outside it.

Fired controls:

- `--selftest-no-opening-owner`: RC 1, only complete inner-boundary ownership goes red.
- `--selftest-force-post-repaint`: RC 1, nearer-effect ownership goes red.

Normal gate: RC 0.

The gate is registered as direct target `moutline` and CMake target `manafold-outlinegate`.

## Build and picture evidence

- Direct clean `moutline` build: PASS.
- Direct clean cel renderer build: PASS.
- Final renderer MD5: `3957094CF4084DD510B409D1CE9D701F`.
- Environment: `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`, `zhao-env.ps1` sourced.
- `manafold-antenna-fixed`: 420 frames, sequence CRC32C `0xC60CEF58`.
- `manafold-channel`: 420 frames, sequence CRC32C `0x971060D9`.
- Protected `zixxtrixx-idle`: 577/577 files byte-identical to the pre-outline binary.

Every-frame fixed/quarter/channel sheets and exact 4x crops were inspected in this isolated agent context. The lower and side stick edges now form one coherent black boundary around the whole open O across the motion. The existing upper/head-top line remains. In effects-on Channel witnesses, cyan lightning visibly retains foreground ownership where it crosses the inner line; no stale black stripe repaints through it.

## Final integrated 23-bone evidence closure

Rebuilt clean from the settled 23-bone posed-ring-checked tree.

- Renderer MD5: `F7C2DAA085661C1E00CADF09C65B5F8A`.
- Environment: `zhao-env.ps1`, `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`.
- Gate RCs: `moutline` normal `0`; no-opening-owner `1`; force-post-repaint `1`.
- Normal sequence CRC32C: fixed `0x8F08D358`, quarter `0x2CC13DC4`, Channel `0xB89B63D1`.
- Explicit `ZHAO_U02_OUTLINE_CONTROL=none` is 577/577 Zixxtrixx Idle files byte-identical to unset, proving the diagnostic is species-scoped and identity-default.

Durable final evidence:

- `PASS17-OUTLINE-FINAL23-FIXED-ALLFRAMES.png`
- `PASS17-OUTLINE-FINAL23-QUARTER-ALLFRAMES.png`
- `PASS17-OUTLINE-FINAL23-CHANNEL-ALLFRAMES.png`
- `PASS17-OUTLINE-FINAL23-OPENING-AB-NATIVE.png` / `...-4X.png`
- `PASS17-OUTLINE-FINAL23-EFFECT-AB-NATIVE.png` / `...-4X.png`

All 1,260 normal fixed/quarter/channel presentation frames retain the open O and coherent full inner boundary. Native and exact 4x normal/control plates reproduce both original defects: removing the opening owner deletes the lower/inside stick ink without filling the O or reducing the top line; forced stale repaint draws black through nearer cyan lightning (clearest at Channel f0345), while normal leaves lightning in front. This evidence supersedes the 22-bone `PASS17-OUTLINE-INTEGRATED-*` checkpoint. Direction 17 is closed on the final 23-bone binary.

## Files

- `tools/reel/manafold_outline.h` — shared pure ownership helpers.
- `tools/reel/manafold_outlinegate.cpp` — normal assertions and two fired controls.
- `tools/reel/zhao_reel.cpp` — body auxiliary scope, enclosed-O owner, removal of stale restore.
- `tools/reel/build-direct.sh` — `moutline` target.
- `tools/CMakeLists.txt` — `manafold-outlinegate` target.
