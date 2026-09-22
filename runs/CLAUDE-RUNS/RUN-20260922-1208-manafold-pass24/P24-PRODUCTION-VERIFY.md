# Manafold pass 24 — production verification

**Date:** 2026-09-22
**Deployed:** `website/deploy.ps1 -Project upheaval -Branch main`, exit **0**
**Deployment:** https://b7e1fe29.upheaval.pages.dev
**Live:** https://upheaval.pages.dev (noindex, unlisted — for the owner)
**Heads:** Zhaozhou `86eb703f`, Upheaval `30119c9` — both branches and both
mains, all four agreeing.

---

## VERDICT: **66 / 66 verified on BOTH hosts, 0 mismatches, 0 retries.**

```
https://upheaval.pages.dev          66/66, 95,368,335 bytes, mismatches 0, retries 0
https://b7e1fe29.upheaval.pages.dev 66/66, 95,368,335 bytes, mismatches 0, retries 0
```

66 = the index + **44 live media** + **21 archive spot checks**, three each from
all seven locked generations (version-17, version-18, pass-19, pass-20, pass-21,
pass-22 and the new pass-23). Every fetch is cache-bypassed — `no-cache`,
`no-store`, `max-age=0`, plus a unique query string per request — so a cached
copy cannot stand in for the served one.

Each media row is checked on **status, exact byte length and SHA-256** against
`P24-LIVE-MEDIA-SHA256.txt` and the archive receipts. The index is compared
**byte-for-byte** against the locally deployed `public/index.html`, and then on
19 content checks, all True on both hosts:

| check | what it protects |
|---|---|
| `robots_exactly_one_noindex_nofollow` | the page stays unlisted |
| `card_pass_24` | and it is not still showing pass 23 |
| `scope_nineteen_moved` | the pass's own scope claim survives on the page |
| `mechanisms_named` | **BOLT AVOIDANCE and DEPTH SPLITTING are both named**, so the owner's comparison is legible |
| `measurement_stated` | ONE IN NINE and 503,304 — the number that decides the rollout |
| `renderer_md5`, `manifest_sha256`, `source_8e5a3ee3` | the provenance is the shipped one |
| `22_subjects_7992_frames` | the bank's size |
| `all_44_live_declared` | every live file is actually on the card |
| `v18`/`p19`/`p20`/`p21`/`p22`/**`p23`**`_archive_22_declared` | each generation declares 22 distinct clips |
| `archive_19_generations` | the count is not stale |
| `fall_no_autoplay_no_loop`, `hover_autoplay_loop` | playback contract |

## The verifier was selftested on broken copies BEFORE its silence was quoted

`verify-selftest.log`, exit **0**:

> local page passes; **14 index negatives fire** (a pass-19 archive clip dropped,
> a pass-20 archive clip dropped, a pass-21 archive clip dropped, a pass-22
> archive clip dropped, **a pass-23 archive clip dropped**, **avoidance name
> dropped**, generation count stale, hover loop stripped, **measurement dropped**,
> old p23 card, renderer md5 altered, robots index,follow, scope sentence
> dropped, **splitting name dropped**); corrupted, truncated and non-200 media fire

Four of those negatives are new this pass and had never been run before. The
media legs prove a one-byte-corrupted copy, a truncated copy and a non-200
response each fail — so "66/66" is a reading from an instrument known to be able
to say no.

## Local gates before the deploy, real exit codes, none read through a pipe

| gate | RC | result |
|---|---|---|
| `assemble.py` | 0 | 2 creatures, 848 renders |
| `checkfresh.py` | 0 | 22 rendered subjects, **22 live fresh, 0 STALE**, 0 absent, 0 unknown |
| `checkarchive.py` | 0 | every generation locked; **pass-23 44/44, 43,681,362 bytes, equal to its published receipt**; live names match the **pass-24** receipt 44/44 |
| `checkarchive.py --selftest` | 0 | **33 red legs fire**, 12 positive legs accept (was 31; pass 23's lock added two) |
| `checkplayback.py` | 0 | Fall controls-only/non-looping, Hover autoplay+loop |
| `checkmedia.py` | 0 | **1,684 declared files, 1,684 decoded** — a full sweep, no skips |
| noindex | — | exactly one robots meta, `noindex, nofollow` |

`deploy.ps1` re-ran playback, archive, noindex, freshness and the full decode
sweep itself immediately before uploading, and all passed again.

## The bank these bytes came from

* **One invocation**, 22 live subjects, **7,992 frames**, production ink
  (`ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`), **no override of any
  kind**. Every subject kept its exact pass-23 frame count.
* **Renderer MD5** `e90af7c043cd9670054eeb256abd5e95`
  (SHA-256 `28e96220f8c75bae6f9a061d5fb1ab07204c6abe6a291c07338bfc7a1b00d732`),
  built from scratch by the reviewer with `build-direct.sh --clean`, g++ 16.1.0
  MinGW-W64 ucrt. No CMake was used and no CMake result is claimed.
* **Bank manifest SHA-256**
  `f9128e0821004dc49b19a280651db864e4556947b20d9267fbb84d5b9e58703b`.
* **Source** zhaozhou `8e5a3ee3` (the reviewed tree).
* **Live media receipt** `Upheaval/creature/Manafold/P24-LIVE-MEDIA-SHA256.txt`,
  44 files, 43,768,883 bytes, every WebM probed VP9 / 384×240 / yuv444p / 60 fps
  with its frame count counted, every poster PNG 1152×720.

## Scope, proved two independent ways

`P24-BANK-RECEIPTS/scope-vs-pass23.txt` — the shipping bank against the same
renderer with every pass-24 knob at its pass-23 value, compared **frame by frame**
rather than by CRC alone: **19 subjects moved, 3 byte-identical** — Curious,
Startle and Taunt III, the authored expression beats.

And corroborated from a different artefact entirely: the **lossless poster
stills** are byte-identical to pass 23 for exactly those same three clips, and
changed for the other nineteen. Two artefacts, one answer.

With every knob off the bank is byte-identical to pass 23 on **22 of 22**.

## Archive

Pass 23 was archived **before** the encode that overwrote it — the ordering is
the whole safety property. 44/44 live files verified against the published,
production-verified pass-23 receipt, copied to `archive-p23-manafold-*`, and the
**copies re-hashed**: 43,681,362 bytes, all matching. `P23-ARCHIVE-SHA256.txt`
is beside the creature. The manifest gained one generation, "Pass 23 —
2026-09-21", declaring its 22 clips exactly once each; the archive note went
EIGHTEEN → NINETEEN generations.

## Housekeeping

Raw frame roots and full-resolution sheets deleted after verification
(`.tmp/p24rev-ship`, `.tmp/p24rev-scope23`, the isolated single-layer renders,
`website/scratch-reel`, `p24-sheets`). Checked for stray `.rgb` that `.gitignore`
hides: **none committed, none left behind**.

## Open issues carried to pass 25

1. **Hover's back ball is still finicky.** `kRearCarrierCalmPm` measures ~52× the
   lever Direction 25 named (9,352 changed pixels against 178) and has never been
   laddered by eye. Try it first. The card says this plainly rather than claiming
   the item was delivered.
2. **mbolt's per-subject selector table is an unbound mirror** of the renderer's
   assignment. They agree today — I read both — but nothing makes them. One
   shared table with a single accessor closes it, the way
   `rear_ambient_clip_gain_pm()` already works.
3. **The rollout.** Avoidance won the comparison. The two worst clips by rate are
   **not** in the experiment — death-drop 16.6 % and drift 15.3 % — and 39,500
   intersections remain on the 19 untouched subjects.
