# Manafold pass 19: production verification

| | |
|---|---|
| **Published** | 2026-09-19 22:58 +02:00 |
| **Deployment** | `https://452e401c.upheaval.pages.dev` |
| **Production alias** | `https://upheaval.pages.dev` |
| **Deployment record** | `Upheaval/website/deploy-records/deploy-20260919-225805.txt` (gates: all run) |
| **Upheaval main at deployment** | `f9e98baedce2e216738ce022f29347329b44a7ba`, fast-forwarded from `2132bb87` |
| **Zhaozhou main at deployment** | `e547a5ec21242d5d21ef59f7539d08ebd27a1f50`, fast-forwarded from `f3f061f7` |

**Verdict: PASS.** Both hosts serve the exact pass-19 index, all 44 live media bytes, and the archive spot checks.

## Publish

```powershell
website\deploy.ps1 -Project upheaval -Branch main
```

No skip flag was used, and the real exit code was **0** (`p19-deploy.log`, output redirected to a file, not piped). The deploy-time gates were:

| Gate | Result |
|---|---|
| Assemble | 738 entries |
| Playback | Fall controls-only and non-looping; Hover autoplay and loop |
| Archive | v17 56/56, v18 44/44 equal to the published v18 receipt, live 44/44 against the pass-19 receipt |
| Robots | exactly `noindex, nofollow` |
| Freshness | 22 fresh, 0 stale |
| Full decode | 1,464/1,464 |

Wrangler 4.86.0 uploaded **45** changed files (the 44 live media and the index) and reused 1,486. The 44 `archive-v18-*` files were not re-uploaded because their bytes were already on the host as the version-18 live files, which is consistent with them being exact copies.

The deploy reassembles the page, so only the generated footer timestamp changed against the pre-deploy gate. The uploaded index is **435,313 bytes**, SHA-256 `1c77f0b63530b22ff6ca1fb7ee3f80f1ded7442763dba04c407a2c227acda2b5`, and that exact file is committed with these records.

## Method

`P19-MEDIA-CLOSURE-RECEIPTS/p19_production_verify.py` is v18's verifier extended. Every request carries a unique query string and the headers `Cache-Control: no-cache, no-store, max-age=0` and `Pragma: no-cache`. It checks three things.

**The index**, byte-for-byte against the deployed local `public/index.html`. Content checks:
- exactly one `noindex, nofollow` robots meta;
- the card reads "MANAFOLD, pass 19" and no longer "MANAFOLD, version 18";
- renderer MD5 `776d5575…`, manifest `f7edc1fb…`, source `zhaozhou 10877707`, and "22 live subjects — 7,992 frames";
- all 44 live media declared, and 22 `archive-v18` clips declared;
- Fall's tag has neither autoplay nor loop; Hover's has both.

**All 44 live media**, on exact SHA-256 and byte length against `P19-LIVE-MEDIA-SHA256.txt`.

**Six archive spot checks:**
- version 17 (`V17-ARCHIVE-SHA256.txt`): `archive-v17-manafold-trick.webm`, `-hover.png`, `-mana-boil.webm`;
- version 18 (`V18-ARCHIVE-SHA256.txt`): `archive-v18-manafold-trick.webm`, `-hover.png`, `-inspect.webm`.

**Tested on deliberately broken copies before use** (`--selftest`, `P19-MEDIA-CLOSURE-RECEIPTS/verify-selftest.log`, RC 0):
- The local page passes every check.
- Each of these negatives fired its own check: Hover with its `loop` stripped, robots `index, follow`, an altered renderer MD5, and the old "version 18" card.
- A one-byte-corrupted copy, a truncated copy and a non-200 response of a live WebM all failed the media check.
- The selftest's first run also caught a broken negative of its own. The Hover-loop regex assumed `loop` came before `src`, so it did not change the page. That was fixed before any host was queried.

## Results

| Host | Verified | Bytes | Mismatches | Retries |
|---|---:|---:|---:|---:|
| `https://452e401c.upheaval.pages.dev` | **51/51** | 58,810,073 | 0 | 0 |
| `https://upheaval.pages.dev` | **51/51** | 58,810,073 | 0 | 0 |

Each host's total is made up of:

| Part | Bytes |
|---|---:|
| Index | 435,313 |
| 44 live media | 44,320,731 |
| 6 archive spot checks | 14,054,029 |

Every response returned HTTP 200 on the first request, all index checks passed on both hosts, and the verifier exit code was **0**. The per-file machine receipt is in `p19-production-verify.json` and `p19-production-verify.log`.

## Disposition

Manafold pass 19 is published and production-verified on both the immutable deployment host and `upheaval.pages.dev`. Version 18 is preserved byte-for-byte in the archive and served.
