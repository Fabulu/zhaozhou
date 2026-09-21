# Manafold pass 22 — production verification

**Date:** 2026-09-21
**Deploy:** `deploy.ps1 -Project upheaval -Branch main`, RC **0**,
record `deploy-20260921-074508.txt`
**Live:** https://upheaval.pages.dev · deployment alias
https://1664776d.upheaval.pages.dev

---

## Heads at deployment

| | |
|---|---|
| **Upheaval main** | `ca51e26d023c113b37ec0625622dba4b5467eb06`, fast-forwarded from `2afec1a7` |
| **Zhaozhou main** | `0d796c4a582a9396ff8bce5b2a0486d97138d3cb`, fast-forwarded from `b9de3059` |

Both fast-forward only. All four heads agree in both repos — local `main`,
`origin/main`, `manafold-pass22` and `origin/manafold-pass22` are the same
commit in each.

> **One process note worth recording.** The first fast-forward attempt on
> Zhaozhou **failed** — `git checkout main` aborted on an uncommitted log file —
> and the script then merged `manafold-pass22` into itself, printed
> *"Already up to date"* and a `branch == main? YES` that was comparing the
> branch to itself. **A check wired to one operand twice reported success on a
> merge that never happened.** It was caught by reading the abort message rather
> than the verdict. The FF was redone with the checkout's own exit code gating
> it, and the comparison now reads `git rev-parse HEAD` after the merge against
> the branch head captured before it.

## Bank receipts

| | |
|---|---|
| source | zhaozhou `0a743562` (content of `0d796c4a`) |
| renderer MD5 | `8a0aa4da7f35a24c8415848268a5cf98` |
| renderer SHA-256 | `c27a29d44c417f984b5bf7447078eb625c0a28b59a0f4b82d388638a6659ee54` |
| bank manifest SHA-256 | `b188f2efa85649821bf5578bbfb1cea25511c214ea48040195d5a579b1777d6e` |
| bank | 22 subjects, **7,992 frames**, ONE invocation, production ink, RC 0 |
| scope | **22/22 subjects changed**, 6,568/7,992 frames, every frame count preserved |
| live media | 44 files, 43,665,082 bytes, ffprobe-verified |

## Local gates, each read from its own exit code

| gate | RC | result |
|---|---|---|
| assemble | 0 | 2 creatures, 804 renders |
| checkfresh --selftest | 0 | classification + orphan red leg fire |
| checkfresh | 0 | 22 live fresh, **0 stale, 0 absent, 0 unknown** |
| checkarchive --selftest | 0 | **twenty-seven red legs fire** |
| checkarchive | 0 | v17 56/56, v18 44/44, pass-19 44/44, pass-20 44/44, **pass-21 44/44**, all equal to their published receipts; live names match the **pass-22** receipt 44/44; **4×22** locked-generation clips |
| checkplayback | 0 | Fall controls-only/non-looping, Hover autoplay+loop |
| robots | — | exactly one `noindex, nofollow` |
| checkmedia (full sweep, no skip) | 0 | **1,596 declared files, 1,596 decoded** |
| encode | 0 | 22 WebMs + 22 posters |

## Production verification — cache-bypassed, both hosts

The verifier was **selftested first**, on deliberately broken copies, so its
silence could be quoted: the local page passes its own checks; the hover-loop,
robots, renderer-MD5, old-card, dropped-pass-19-clip, **dropped-pass-21-clip**
and stale-generation-count negatives all fire; and a one-byte-corrupted, a
truncated and a non-200 media response all fail. `verify-selftest.log`, RC 0.

Every request carries `Cache-Control: no-cache, no-store, max-age=0`, `Pragma:
no-cache` and a unique query string.

| host | rows verified | bytes | mismatches | retries |
|---|---|---|---|---|
| `https://upheaval.pages.dev` | **60 / 60** | 80,511,279 | **0** | 0 |
| `https://1664776d.upheaval.pages.dev` | **60 / 60** | 80,511,279 | **0** | 0 |

The 60 rows are the index, the **44 live media** files against
`P22-LIVE-MEDIA-SHA256.txt`, and **15 archive spot checks — three from each of
the five locked generations** (version-17, version-18, pass-19, pass-20 and the
new pass-21), so a generation cannot be locked in the repo and silently absent
from the host.

**index.html**, identical on both hosts and byte-for-byte equal to the local
deployed file:

```
sha256 a2d20175e37468f34db5948f6979e1d7b3255205adcea21ef60a12599d1951d5
bytes  466104
```

All fourteen index content checks are true on both hosts:

`robots_exactly_one_noindex_nofollow`, `card_pass_22`, `renderer_md5`,
`manifest_sha256`, `source_0a743562`, `22_subjects_7992_frames`,
`all_44_live_declared`, `v18_archive_22_declared`, `p19_archive_22_declared`,
`p20_archive_22_declared`, **`p21_archive_22_declared`**,
**`archive_17_generations`**, `fall_no_autoplay_no_loop`, `hover_autoplay_loop`.

The page remains **noindex, nofollow** — unlisted, for the owner.

## Open issues carried forward

1. **The knead reaction is uneven by design, and five clips barely show it**
   (slots 18, 20, 14, 17, 1 at 1.7–7.1°). Both clips the direction names for
   acceptance — Inspect and Hover — reach the reference and take the full
   gesture. If the beat is wanted on the shallow five, the lever is those clips'
   own press depth, **not** the rotation constant.
2. **`dip_pm` runs at about a fifth of its declared range bank-wide.** Pass 22
   works around it with a declared, saturating reference. Whether
   `kFoldDipRefMm = 420 mm` is the right reference for the pose at all is the
   underlying question.
3. **R6's LINE far-leg is tautological** — it compares an expression against
   itself and cannot fire. The lightning claim does not rest on it (the
   other-splat population and the near leg are real checks, and the routing
   argument is structural), but it should not be quoted as evidence. One-line
   repair next pass.
4. **R7 floors on the bank maximum, not per clip**, so it asserts the reaction
   exists somewhere rather than everywhere. Given issue 1, a pass that tunes
   per-clip press depth should give it a per-clip floor.
5. **Hover is not a byte-exact near clip** and must not be quoted as one.
   Inspect is.

None blocks. The pass is complete and served.
