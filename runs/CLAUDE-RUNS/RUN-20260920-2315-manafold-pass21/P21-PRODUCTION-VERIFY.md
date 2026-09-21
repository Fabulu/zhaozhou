# Manafold pass 21: production verification

| | |
|---|---|
| **Published** | 2026-09-21 02:09:30 +02:00 |
| **Deployment** | `https://3a1aca54.upheaval.pages.dev` |
| **Production alias** | `https://upheaval.pages.dev` |
| **Deployment record** | `Upheaval/website/deploy-records/deploy-20260921-020930.txt` (gates: all run) |
| **Upheaval main at deployment** | `76c6a3fbd2835fe062058ea66df24b123ad00c6e`, fast-forwarded from `343c4ed2` |
| **Zhaozhou main at deployment** | `806aecf7ec5e9a6d0824d75e9d04faefa974de1d`, fast-forwarded from `4bcf83db` |

**Verdict: PASS.** Both hosts serve the exact pass-21 index, all 44 live media
bytes, and twelve archive spot checks across four locked generations. Every
response returned HTTP 200 on the first request and the verifier exited 0.

## Publish

```powershell
website\deploy.ps1 -Project upheaval -Branch main
```

No skip flag was used, and the real exit code was **0** (`p21-deploy.log`, output
redirected to a file, not piped). Both mains were fast-forwarded first, and the
fast-forward was **proved rather than assumed** — `git merge-base --is-ancestor`
against both the local and the origin main, then `git push . manafold-pass21:main`,
which refuses anything that is not a fast-forward.

The deploy-time gates were:

| Gate | Result |
|---|---|
| Assemble | 2 creatures / **782** entries |
| Playback | Fall controls-only and non-looping; Hover autoplay and loop |
| Archive | v17 56/56; v18 44/44 equal to the published v18 receipt; pass-19 44/44 equal to the published pass-19 receipt; **pass-20 44/44 (44,980,318 B) equal to the published pass-20 receipt**; live 44/44 against the pass-21 receipt |
| Robots | exactly `noindex, nofollow` |
| Freshness | 22 fresh, 0 archived, 0 stale, 0 absent, 0 unknown |
| Full decode | **1,552 / 1,552**, no skip |

Wrangler 4.86.0 uploaded **45** files and reused 1,574. The 45 are the 44 changed
live media plus the index — all 44 changed this pass, where pass 20 changed 43
(one poster frame had genuinely not moved). The 22 `archive-p20-*` files were
**not** re-uploaded, because their bytes were already on the host as the pass-20
live files, which is exactly what being byte-for-byte copies means.

The deploy reassembles the page, so only the generated footer timestamp changed
against the pre-deploy gate: 455,500 bytes both times, SHA-256
`a3506db25c97a785b075ac6b9f4fab59d34ff19e7971e7c5c7c73c0a6dc4dfff` before and
**`d4bafc4c31d8c35390a567c83abc16af84f92f460065000b555b4b8ef0ecef35`** as
deployed. That exact file is committed with these records.

## Method

`P21-MEDIA-CLOSURE-RECEIPTS/p21_production_verify.py` is pass 20's verifier
extended by one locked generation. Every request carries a unique query string
and the headers `Cache-Control: no-cache, no-store, max-age=0` and
`Pragma: no-cache`. It checks three things.

**The index**, byte-for-byte against the deployed local `public/index.html`, plus
**thirteen** content checks (pass 20 had twelve):
- exactly one `noindex, nofollow` robots meta;
- the card reads "MANAFOLD, pass 21" and no longer "MANAFOLD, pass 20";
- renderer MD5 `fe1bab84…`, manifest `639c370c…`, source `zhaozhou bd29d4f9`,
  and "22 live subjects — 7,992 frames";
- all 44 live media declared, 22 `archive-v18` clips declared, 22 `archive-p19`
  clips declared, **22 `archive-p20` clips declared**, and
  **"Archive (16 generations)"**;
- Fall's tag has neither autoplay nor loop; Hover's has both.

**All 44 live media**, on exact SHA-256 and byte length against
`P21-LIVE-MEDIA-SHA256.txt`.

**Twelve archive spot checks, one set per locked generation** — so a generation
cannot be locked in the repo and silently absent from the host:
- version 17: `archive-v17-manafold-trick.webm`, `-hover.png`, `-mana-boil.webm`;
- version 18: `archive-v18-manafold-trick.webm`, `-hover.png`, `-inspect.webm`;
- pass 19: `archive-p19-manafold-trick.webm`, `-hover.png`, `-inspect.webm`;
- **pass 20**: `archive-p20-manafold-trick.webm`, `-hover.png`, `-inspect.webm`.

**Tested on deliberately broken copies before use** (`--selftest`,
`P21-MEDIA-CLOSURE-RECEIPTS/verify-selftest.log`, RC 0), because a checker's
silence is only evidence after you have watched it speak:
- the local page passes every check;
- each of these negatives fired its own check: Hover with its `loop` stripped,
  robots `index, follow`, an altered renderer MD5, the old "pass 20" card, a
  dropped `archive-p19` declaration, **a dropped `archive-p20` declaration** (the
  new lock's own negative, which had never been run before), and a stale
  "Archive (15 generations)";
- a one-byte-corrupted copy, a truncated copy and a non-200 response of a live
  WebM all failed the media check.

The pass-20 negative **drops** its declaration rather than renaming the clip, and
that is deliberate: pass 20's selftest caught itself renaming
`archive-p19-manafold-trick.webm` to `…-zzz.webm`, which still matches the
pattern and still counts 22, so the check did not fire. The same fixture fault
was available here and was not repeated.

## Results

| Host | Verified | Bytes | Mismatches | Retries |
|---|---:|---:|---:|---:|
| `https://3a1aca54.upheaval.pages.dev` | **57/57** | 73,222,298 | 0 | 0 |
| `https://upheaval.pages.dev` | **57/57** | 73,222,298 | 0 | 0 |

Each host's total is made up of:

| Part | Bytes |
|---|---:|
| Index | 455,500 |
| 44 live media | 43,766,439 |
| 12 archive spot checks | 29,000,359 |

All **thirteen** index checks passed on both hosts, every response returned HTTP
200 on the first request, and the verifier exit code was **0**. The per-file
machine receipt is in `p21-production-verify.json` and `p21-production-verify.log`.

## Disposition

Manafold pass 21 is published and production-verified on both the immutable
deployment host and `upheaval.pages.dev`. Pass 20 is preserved byte-for-byte in
the archive and served, alongside pass 19, version 18 and version 17 — four
locked generations, each cross-locked to the live receipt that production
actually served. The page remains `noindex, nofollow`.
