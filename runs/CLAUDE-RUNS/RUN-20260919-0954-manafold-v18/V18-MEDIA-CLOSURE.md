# Manafold version 18: media closure

**Date:** 2026-09-19
**Worker:** Claude (sole Opus worker)
**Verdict:** **READY-TO-PUBLISH.** The exact encode and the no-skip local gate are complete. The mains have not been fast-forwarded and nothing is deployed; both wait for the coordinator's go.

**Accepted source:** Zhaozhou `db2bcf0e`
**Accepted bank evidence:** `09f10562` (`V18-FINAL-BANK-INTEGRITY.md`)
**Renderer:** MD5 `0f082622d4ca0c58d012d1f0de555723`
**Bank manifest SHA-256:** `bdaac548e5dc956b7aa4afac57dae73e265861b25e3160788a26cbd8aa0628bd` (22 subjects, 7,992 frames, 2,209,692,096 bytes)

## 1. Exact encode source

- **Re-validated before the encode.** `V18-FINAL-RECEIPTS/v18-final-bank-validate.py` was re-run against `C:\programmieren\zencrifice\manafold-p16\v18-final-reel-22`: PASS, 22 subjects, 7,992 frames, 2,209,692,096 bytes. The manifest SHA-256 is again `bdaac548…28bd`, so the frame root is unchanged since acceptance.
- **Junction repointed, frames untouched.** `Upheaval/website/scratch-reel` was a Windows junction to the Pass-17 bank `pass17-final3-reel-28`. Only the junction was removed (`rmdir`) and recreated (`mklink /J`) to point at `v18-final-reel-22`. The old target still holds its 28 subject folders. No frame was copied or regenerated.

## 2. Encode

`python tools/tovideo.py . <the 22 live subjects>` ran from `Upheaval/website` in one invocation, with output redirected to a file and the real exit code read. There was no pipe.

- The log is `v18-final-encode.log`, which ends with **`ENCODE_RC=0`**.
- **22/22** WebMs (VP9 CRF16 4:4:4, 60 fps) and **22/22** posters were written. The posters use the existing `POSTER` table frames. Trick stays on f230, which is inside the planted hold.
- The independent probe (`ffprobe -count_frames`) passed on all 44 files. Every WebM is VP9, 384×240, `yuv444p`, 60/1, and its decoded frame count equals its source frame count. Every poster is a 1152×720 PNG. There were **0 errors**.
- `V18-LIVE-MEDIA-SHA256.txt` records SHA-256, bytes and path for exactly the **44** live files, **44,743,343 bytes** in total. A byte-identical copy sits beside the creature in `Upheaval/creature/Manafold/`, where the site gate consumes it (§4).

## 3. Site copy

- **Source.** The coordinator-checked Qwen draft `qwen/Q019-v18-findings-final.answer.md` (verdict "partial, use with corrections"), with both corrections applied.
  - Nothing says the bank "passed the every-frame review" flatly. The copy says every frame was reviewed on complete contact sheets, and that fine detail at sheet scale rests on the earlier byte-identical reviews. It adds that Trick was checked at native and 3x scale.
  - The mqa fix is described as checker hardening, not an art change.
- **Edits.** The copy was edited into plain owner language. Qwen's FINDINGS table and CONTINUATION are not reproduced.
- **Where it changed.**
  - `website/creatures.json`: the live Manafold blurb reads "MANAFOLD, version 18 — …" and carries the real PROVENANCE paragraph. The site-level note reads version 18.
  - Nine clip captions or notes that described changed behaviour were updated: Hover, Inspect, Channel, Trick, Hasty, Flight, Drift, Blown and Crackle.
  - The archive note, the archive generations and the archive count are unchanged.
- **Findings and plan.** `Upheaval/creature/Manafold/VERSION-18-FINDINGS.md` is written from the same copy. `VERSION-18-PLAN.md` gains the closure paragraph.

## 4. A gate that refused the encode it was built to allow

The first no-skip gate run failed with **RC 1** at `checkarchive`, reporting a "source/archive pair mismatch" on all 44 live names (`v18-media-gates.log`, first run, overwritten by the passing run below).

- **Cause.** Wave A's `checkarchive.py` required each surviving live name to equal its version-17 archive twin. That is right before the encode and wrong after it, and the tool had no second phase. As written, it could never pass a version-18 publication.
- **Repair (`website/tools/checkarchive.py`).** The check is now phase-aware.
  - While `creature/Manafold/V18-LIVE-MEDIA-SHA256.txt` is absent, the old pairing rule holds unchanged. This was verified: with the receipt moved away, the same tree fails on every live pair.
  - Once the receipt is present, every one of the 44 live names must match it exactly: no more, no fewer, same bytes.
  - The 56 locked archive digests, the declarations and the archive playback are checked identically in both phases.
- **Selftest.** It now fires **five** red legs, up from three. The two new ones are a live file that differs from its receipt and a short receipt.

## 5. Canonical no-skip local gate

The invocation was `deploy.ps1 -Project upheaval -Branch main -AssembleOnly`, with no skip flag and FFmpeg 9.0.1 on `PATH`. It was preceded by the `checkarchive` and `checkfresh` selftests. The final log, taken after the copy was final, is `v18-media-gates-final.log`.

| Gate | Result |
|---|---|
| selftests | checkarchive RC 0 (5 red legs), checkfresh RC 0 |
| assemble | 2 creatures / 716 render entries |
| playback | Fall controls-only/non-looping; Hover autoplay+loop |
| archive | 56/56 locked files, 70,576,645 bytes; live names match the version-18 receipt 44/44; 22+6+10 declared once; archive playback controls+loop/no-autoplay/preload-none |
| robots | exactly `noindex, nofollow` |
| freshness | **22 live fresh / 0 stale / 0 absent / 0 unknown** (the source junction now holds only the 22 version-18 subjects) |
| decode | **1,420 declared / 1,420 decoded**, no skip |
| overall | **RC 0**; nothing deployed |

The generated `website/public/index.html` is **426,337 bytes**, SHA-256 `f7ba41752e39ae6d7701e070ccf370c7e35122784f2d519e745e5023255c4bfd`. It contains the provenance (source `db2bcf0e`, renderer MD5, manifest SHA-256) and no "pass 18" anywhere.

## 6. Checker hardening (coordinator item, render-neutral)

**Commit.** Zhaozhou `0e84d60f`, `tools/reel/manafold_qa_p12.cpp` only.

**Change.** The Wave-F `attributed()` legs now also require the pre-existing Q1–Q5 categories to stay green, where before they counted only the other Wave-F counters.

**Found on the way.** The strict rule immediately failed `--fail-trick-spin-gain`. Two revolutions in the same window double the turn speed, so Q3's Trick root-step ceiling also fires (309.2 mm vs 240). This is a genuine co-failure that the old rule hid.
- Two alternatives were tried and rejected: controls of −1000 and 0 pm keep Q3 green but break Q6b's join detection.
- **Resolution:** the Q6a leg declares exactly that one consequence, Q3's slot-13 row, with the reason in the source. Any other pre-Wave-F failure still fails the leg.

**Receipts** (`V18-MEDIA-CLOSURE-RECEIPTS/`): the direct clean build with winlibs g++ 16.1.0 (`mqa-attr-build.log`, RC 0) and the binary MD5 `8fb3f25262ac4ee23718e28374eec43b` (`mqa-attr-binary.txt`). The results are in `mqa-attr-matrix.txt`:
- normal: RC 0, PASS 0 failures;
- Q6a, Q6b, Q6c, Q6d and Q7 legs: each RC 0 with "fired only its own category". RC 0 is this matrix's convention for an attributed leg (`gatematrix_int.sh`, `run f-mqa-* 0`); RC 1 would mean *not* attributed;
- the six legacy mqa legs: RC 0, unchanged;
- positive control for the stricter rule: `mqa-attr-POSCTL-undeclared-trick-spin-gain.log`, the same leg before the declaration, RC 1, "pre-Wave-F categories 1".

No rendered byte depends on this file, so nothing was re-rendered or re-encoded.

## 7. Scratch (deletable)

- `zhaozhou/.tmp/v18-mqa-attr/` (the mqa build).
- The session scratchpad copy-edit scripts. The media probe/SHA tool and the pre-encode validation log are committed in `V18-MEDIA-CLOSURE-RECEIPTS/` (`v18_media_sha.py`, `prencode-validate.log`).
- `C:\programmieren\zencrifice\manafold-p16\pass17-final3-reel-28`, 3.1 GB. It is the old junction target and no longer referenced. It is archive-only history, so delete it only on the coordinator's say.
- Keep `v18-final-reel-22` until production verification is done: the junction points at it and `checkfresh` reads it.

## 8. Next (on the coordinator's go)

1. Fast-forward Zhaozhou and Upheaval `main` to `manafold-v18`.
2. Run `deploy.ps1 -Project upheaval -Branch main`.
3. Verify production with the cache bypassed: the index SHA plus all 44 live media files against `V18-LIVE-MEDIA-SHA256.txt`, on the unique and alias hosts.
4. Commit and push the production records.
