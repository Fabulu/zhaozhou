# Manafold version 18: production verification

| | |
|---|---|
| **Published** | 2026-09-19 20:14 +02:00 |
| **Deployment** | `https://142475d3.upheaval.pages.dev` |
| **Production alias** | `https://upheaval.pages.dev` |
| **Deployment record** | `Upheaval/website/deploy-records/deploy-20260919-201401.txt` (gates: all run) |
| **Upheaval main at deployment** | `a9ce5c83958421c8f50ad2088f975ace67ddd230`, fast-forwarded from `664f415` |
| **Zhaozhou main** | `438e42f23cb0a5ec2c96b7fda962c78813f7ade9`, fast-forwarded from `bd7de937` |

**Verdict: PASS.** Both hosts serve the exact version-18 index and every live media byte.

## Publish

```powershell
website\deploy.ps1 -Project upheaval -Branch main
```

No skip flag was used, and the real exit code was **0** (`v18-deploy.log`). The deploy-time gates were:

- assemble: 716 entries;
- playback: Fall is controls-only and does not loop; Hover autoplays and loops;
- archive: 56/56 locked files, and the live names match the version-18 receipt 44/44;
- robots: exactly `noindex, nofollow`;
- freshness: 22 fresh, 0 stale;
- full decode: 1,420/1,420.

Wrangler 4.86.0 uploaded 45 changed files, reused 1,442, and returned the unique deployment URL above.

The deploy reassembles the page, so only the generated footer timestamp changed. The uploaded index is **426,337 bytes**, SHA-256 `835e9aab7fc2d171b13b000e4e8e8f2319e87e24b3ee1ae352787b791dd9e337`, and that exact file is committed with these records.

## Method

`V18-MEDIA-CLOSURE-RECEIPTS/v18_production_verify.py` sent every request with a unique query string and the headers `Cache-Control: no-cache, no-store, max-age=0` and `Pragma: no-cache`. It compared three things:

- **the index**, byte-for-byte against the deployed local `public/index.html`. It also checked that the index has exactly one `noindex, nofollow` robots meta and carries source `db2bcf0e`, renderer MD5 `0f082622d4ca0c58d012d1f0de555723`, manifest `bdaac548…28bd` and "22 live subjects — 7,992 frames". It checked that all 44 live media are declared, that Fall's video tag has neither autoplay nor loop, that Hover's has both, and that no "pass 18" appears anywhere;
- **all 44 live media**, on exact SHA-256 and byte length against `V18-LIVE-MEDIA-SHA256.txt`;
- **three archive spot checks** against the locked `V17-ARCHIVE-SHA256.txt`: `archive-v17-manafold-trick.webm`, `archive-v17-manafold-hover.png` and `archive-v17-manafold-mana-boil.webm`.

Before use, the index checks were run on the local page, where they all pass. They were also run on two known-negatives, a Hover with its loop stripped and a robots tag of `index, follow`, and both fired.

## Results

| Host | Verified | Bytes | Mismatches | Retries |
|---|---:|---:|---:|---:|
| `https://142475d3.upheaval.pages.dev` | **48/48** | 51,732,019 | 0 | 0 |
| `https://upheaval.pages.dev` | **48/48** | 51,732,019 | 0 | 0 |

Each host's total is the 426,337-byte index, plus 44 live media files totalling 44,743,343 bytes, plus 6,562,339 bytes of archive spot checks. Every response returned HTTP 200 on the first request, and all index checks passed on both hosts. The verifier's exit code was **0**.

The per-file machine receipt is in `v18-production-verify.json` and `v18-production-verify.log`.

## Disposition

Manafold version 18 is published and production-verified on both the immutable deployment host and `upheaval.pages.dev`.
