# Manafold pass 20: production verification

| | |
|---|---|
| **Published** | 2026-09-20 13:51:48 +02:00 |
| **Deployment** | `https://0cb48546.upheaval.pages.dev` |
| **Production alias** | `https://upheaval.pages.dev` |
| **Deployment record** | `Upheaval/website/deploy-records/deploy-20260920-135148.txt` (gates: all run) |
| **Upheaval main at deployment** | `d33f0698c6381a0d0a7630135dca06ea99a92dce`, fast-forwarded from `d7086d2e` |
| **Zhaozhou main at deployment** | `e0447b1ffaab0d05e77c81918c14a6518930867c`, fast-forwarded from `4b3d4576` |

**Verdict: PASS.** Both hosts serve the exact pass-20 index, all 44 live media
bytes, and nine archive spot checks across three generations.

## Publish

```powershell
website\deploy.ps1 -Project upheaval -Branch main
```

No skip flag was used, and the real exit code was **0** (`p20-deploy.log`,
output redirected to a file, not piped). Both mains were fast-forwarded first,
with the fast-forward proved rather than assumed (`git merge-base --is-ancestor`
against both the local and the origin main, then `git push . manafold-pass20:main`,
which refuses anything that is not a fast-forward).

The deploy-time gates were:

| Gate | Result |
|---|---|
| Assemble | 2 creatures / 760 entries |
| Playback | Fall controls-only and non-looping; Hover autoplay and loop |
| Archive | v17 56/56, v18 44/44 equal to the published v18 receipt, pass-19 44/44 equal to the published pass-19 receipt, live 44/44 against the pass-20 receipt |
| Robots | exactly `noindex, nofollow` |
| Freshness | 22 fresh, 0 stale, 0 absent, 0 unknown |
| Full decode | 1,508/1,508 |

Wrangler 4.86.0 uploaded **44** files and reused 1,531. The 44 are the 43
changed live media plus the index. The 22 `archive-p19-*` files were not
re-uploaded because their bytes were already on the host as the pass-19 live
files, which is consistent with them being exact copies — and
`manafold-taunt3.png` was not re-uploaded because it genuinely did not change
(see `P20-MEDIA-CLOSURE.md` §2).

The deploy reassembles the page, so only the generated footer timestamp changed
against the pre-deploy gate. The uploaded index is **446,851 bytes**, SHA-256
`a5396a86fa396714a9ce56c7b1f9b139905829621d6a8fc9ac8fab90ce682ed3`, and that
exact file is committed with these records.

## Method

`P20-MEDIA-CLOSURE-RECEIPTS/p20_production_verify.py` is pass 19's verifier
extended. Every request carries a unique query string and the headers
`Cache-Control: no-cache, no-store, max-age=0` and `Pragma: no-cache`. It checks
three things.

**The index**, byte-for-byte against the deployed local `public/index.html`,
plus twelve content checks:
- exactly one `noindex, nofollow` robots meta;
- the card reads "MANAFOLD, pass 20" and no longer "MANAFOLD, pass 19";
- renderer MD5 `e95faca9…`, manifest `a40b4154…`, source `zhaozhou 55767880`,
  and "22 live subjects — 7,992 frames";
- all 44 live media declared, 22 `archive-v18` clips declared, **22
  `archive-p19` clips declared**, and **"Archive (15 generations)"**;
- Fall's tag has neither autoplay nor loop; Hover's has both.

**All 44 live media**, on exact SHA-256 and byte length against
`P20-LIVE-MEDIA-SHA256.txt`.

**Nine archive spot checks, one set per locked generation:**
- version 17 (`V17-ARCHIVE-SHA256.txt`): `archive-v17-manafold-trick.webm`,
  `-hover.png`, `-mana-boil.webm`;
- version 18 (`V18-ARCHIVE-SHA256.txt`): `archive-v18-manafold-trick.webm`,
  `-hover.png`, `-inspect.webm`;
- **pass 19** (`P19-ARCHIVE-SHA256.txt`): `archive-p19-manafold-trick.webm`,
  `-hover.png`, `-inspect.webm`.

**Tested on deliberately broken copies before use** (`--selftest`,
`P20-MEDIA-CLOSURE-RECEIPTS/verify-selftest.log`, RC 0):
- The local page passes every check.
- Each of these negatives fired its own check: Hover with its `loop` stripped,
  robots `index, follow`, an altered renderer MD5, the old "pass 19" card, a
  dropped `archive-p19` declaration, and a stale "Archive (14 generations)".
- A one-byte-corrupted copy, a truncated copy and a non-200 response of a live
  WebM all failed the media check.

**The selftest caught a broken negative of its own, again.** The first version
of the "a pass-19 archive clip dropped" negative *renamed* the clip to
`archive-p19-manafold-zzz.webm` — which still matches the pattern and still
counts 22, so the check did not fire and the selftest failed loudly. It now
drops the declaration instead. That is the whole point of running the negatives
before any host is queried: a silent check is worthless, and this one announced
itself.

## Results

| Host | Verified | Bytes | Mismatches | Retries |
|---|---:|---:|---:|---:|
| `https://0cb48546.upheaval.pages.dev` | **54/54** | 66,906,938 | 0 | 0 |
| `https://upheaval.pages.dev` | **54/54** | 66,906,938 | 0 | 0 |

Each host's total is made up of:

| Part | Bytes |
|---|---:|
| Index | 446,851 |
| 44 live media | 44,980,318 |
| 9 archive spot checks | 21,479,769 |

Every response returned HTTP 200 on the first request, all twelve index checks
passed on both hosts, and the verifier exit code was **0**. The per-file machine
receipt is in `p20-production-verify.json` and `p20-production-verify.log`.

## Disposition

Manafold pass 20 is published and production-verified on both the immutable
deployment host and `upheaval.pages.dev`. Pass 19 is preserved byte-for-byte in
the archive and served, alongside version 18 and version 17. The page remains
`noindex, nofollow`.
