# Manafold Pass 17 production verification

**Published:** 2026-09-19
**Deployment:** `https://e4b4a928.upheaval.pages.dev`
**Production alias:** `https://upheaval.pages.dev`
**Deployment record:** `Upheaval/website/deploy-records/deploy-20260919-091648.txt`
**Upheaval main at deployment:** `e772dc987e3392e56d3f1147dd824396b07d64ab`
**Verdict:** **PASS — both hosts serve the exact finished Pass-17 index and every live media byte**

## Publish gate

The finished pass was published with the mandatory production branch argument:

```powershell
website\deploy.ps1 -Project upheaval -Branch main -SkipDecodeSweep
```

The deploy invocation reassembled 2 creatures / 694 render entries, proved Fall controls-only/non-looping while Hover remains autoplay+loop, preserved exact `noindex, nofollow`, and returned freshness `28/28` with zero stale/absent media. The deployment-time decode sweep was skipped only because the same unchanged 1,376 declared files had immediately passed the complete no-skip decode gate twice at RC 0. Wrangler uploaded 57 changed files, reused 1,386 and returned the unique deployment URL above.

## Exact verification set

A cache-bypassed verifier used a unique query string plus `Cache-Control: no-cache, no-store, max-age=0` and `Pragma: no-cache` for every request. It compared:

- deployed `index.html` against the post-deploy locally assembled index;
- all **56** live `renders/manafold-*.webm/.png` files against `PASS17-LIVE-MEDIA-SHA256.txt` and local committed media.

The index verification additionally requires:

- exactly one `<meta name="robots" content="noindex, nofollow">`;
- Pass-17 source `18e1d993`, renderer MD5 `67DCAFF8ABC0AA2BA830C439A4CD98C7`, bank manifest `bc2d4d0bdce632815f42cca208763fb04bca62034e285ed1ef707c7f28d55d09`, and `28 live subjects / 11,592 frames`;
- every one of the 56 live media declarations;
- Fall without autoplay/loop and Hover with autoplay/loop.

The committed pre-deploy page was 415,608 bytes with SHA-256 `431c2643682bf52012862b050e9a05d3e73aab5040254d3d0eeeb910f148db96`. The deploy command deliberately reassembled it, changing only the generated footer timestamp from 07:09 UTC to 07:16 UTC. The exact page that was uploaded and verified is therefore still 415,608 bytes but has SHA-256 `39696c4ec636b82bbc5dd5ba6de24e5f894eda0668b0e0637629de10bdac51b9`.

## Cache-bypassed results

| Host | Index propagation attempt | Verified | Bytes | Mismatches |
|---|---:|---:|---:|---:|
| `https://e4b4a928.upheaval.pages.dev` | 1 | 57/57 | 70,992,253 | 0 |
| `https://upheaval.pages.dev` | 1 | 57/57 | 70,992,253 | 0 |

Per host, the total comprises the 415,608-byte index plus 56 media files / 70,576,645 bytes. Every response returned HTTP 200 and matched exact local SHA-256 and byte length. The unique deployment and production alias required no propagation retry beyond their first cache-bypassed request.

The complete per-file machine receipt is `pass17-production-verify.log`.

## Final disposition

**PASS.** Production serves the exact finished Manafold Pass 17 page and all 56 current live media bytes on both the immutable deployment host and `upheaval.pages.dev`. Pass 17 is published and production-verified.
