# Manafold Pass 17 media closure

**Date:** 2026-09-19
**Status:** **PASS — exact encode and no-skip local publication gate complete**
**Accepted source:** `18e1d993`
**Accepted evidence:** `a4d08762`
**Renderer MD5:** `67DCAFF8ABC0AA2BA830C439A4CD98C7`
**Renderer SHA-256:** `D60EBA80C2547032A603365837690FB14B551D90D96FB5973F1C976C185C3FE9`
**Bank manifest SHA-256:** `bc2d4d0bdce632815f42cca208763fb04bca62034e285ed1ef707c7f28d55d09`

## Exact encode source

`Upheaval/website/scratch-reel` was inspected as a Windows junction to the historical Pass-16 `final4-reel`. Only the junction itself was removed; its target was untouched. It now resolves to the sole accepted raw bank:

`C:\programmieren\zencrifice\manafold-p16\pass17-final3-reel-28`

The source contains exactly 28 canonical subjects / 11,592 contiguous RGB frames and is accepted 28/28 in `PASS17-FINAL-BANK-INTEGRITY-V3.md` plus `PASS17-FINAL3-TRICK-REVIEW.md`.

Taunt III's poster moved from historical shrug frame 100 to accepted held-punchline frame 324, where A80, the front-held face and `1450/750` eyes all read.

## Encode

`website/tools/tovideo.py` was invoked with exactly the 28 canonical subject names. Output remained VP9 CRF16 with `yuv444p` at 60 fps; posters remain nearest-neighbour 3x PNGs.

The first invocation returned RC 1 before completing a subject because this noninteractive shell did not contain the installed FFmpeg directory on `PATH`. The exact FFmpeg 9.0.1 full-build bin was then prepended and the **entire 28-subject command restarted from the beginning**. No partial output from the failed launch was accepted.

Final encode log: `pass17-final3-encode.log`

- final encoder RC: **0**;
- 28/28 WebMs written from their complete declared source-frame counts;
- 28/28 posters written;
- Taunt III poster frame: `324`;
- all WebMs probe as VP9, 384x240, `yuv444p`, 60/1 fps with duration matching `frames / 60`;
- all posters probe as PNG, 1152x720.

`PASS17-LIVE-MEDIA-SHA256.txt` records SHA-256 and byte length for exactly the 56 current live Manafold files.

- files: **56**;
- total bytes: **70,576,645**;
- missing/empty/stream-format errors: **0**.

## Manifest and card

`Upheaval/creature/Manafold/PASS-17-FINDINGS.md` records the completed pass and both exact-bank correction loops. The live Manafold card now describes Pass 17 rather than carrying Pass-16 claims about broad transparency, open work, stale rear rho, Fall restart or Trick identity loss. The Pass-16 archive group and bytes remain unchanged.

Exact provenance shown in the generated page:

- source `18e1d993`;
- renderer MD5 `67DCAFF8ABC0AA2BA830C439A4CD98C7`;
- bank manifest `bc2d4d0bdce632815f42cca208763fb04bca62034e285ed1ef707c7f28d55d09`;
- 28 subjects / 11,592 frames.

## Canonical no-skip local gate

Invocation:

```powershell
website\deploy.ps1 -Project upheaval -Branch main -AssembleOnly
```

No skip flag was used. Initial log: `pass17-media-gates.log`; final post-copy-correction rerun: `pass17-media-gates-final.log`.

- assemble: 2 creatures / **694 render entries**;
- playback: Fall is controls-only/non-looping; Hover remains autoplay+loop;
- robots: exact `noindex, nofollow`;
- freshness: **28 fresh / 0 stale / 0 absent**;
- full decode: **1,376 declared / 1,376 decoded**;
- overall assemble-gate RC: **0**;
- deployment performed: **no** (`-AssembleOnly`).

Generated `website/public/index.html` after the remaining site-wide Pass-16 summary sentence was corrected and the complete no-skip gate was repeated with the installed FFmpeg directory on `PATH`:

- bytes: **415,608**;
- SHA-256: `431c2643682bf52012862b050e9a05d3e73aab5040254d3d0eeeb910f148db96`;
- contains the exact Pass-17 provenance and current card copy;
- final rerun: playback, robots, freshness and **1,376/1,376** decode all passed at RC 0.

## Disposition

Local media closure is complete. The exact media/card/findings packet is ready for commit/push and mainline fast-forward. The finished-pass deployment is authorized by project policy, but publication and cache-bypassed production-byte verification remain separate outward-facing steps for the coordinator.
