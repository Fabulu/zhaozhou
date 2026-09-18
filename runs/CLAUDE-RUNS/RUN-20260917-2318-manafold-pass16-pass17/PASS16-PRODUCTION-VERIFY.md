# Manafold Pass 16 production verification

**Deployment:** `https://6d83b033.upheaval.pages.dev`
**Production alias:** `https://upheaval.pages.dev`
**Published:** 2026-09-18

## Deploy invocation and gates

```powershell
website\deploy.ps1 -Project upheaval -Branch main -SkipDecodeSweep
```

- exact production branch argument present;
- assembly succeeded: 2 creatures / 666 render entries;
- robots gate: exactly `noindex, nofollow`;
- freshness re-ran: 28/28 fresh, 0 stale, 0 absent;
- decode sweep explicitly skipped only at deploy time because the unchanged committed bytes had immediately beforehand passed the canonical full gate: 1,320 declared / 1,320 decoded, RC 0;
- Wrangler uploaded 57 changed files, reused 1,330, and returned deployment success;
- deployment record: `website/deploy-records/deploy-20260918-064700.txt`.

## Cache-bypassed production bytes

A cache-bypassed verifier fetched `index.html` plus all 56 live Pass-16 Manafold media files from both the unique deployment host and production alias, then compared SHA-256 and byte length against local committed media / the post-deploy generated index.

| Host | Verified | Bytes | Mismatches |
|---|---:|---:|---:|
| `https://6d83b033.upheaval.pages.dev` | 57/57 | 54,841,925 | 0 |
| `https://upheaval.pages.dev` | 57/57 | 54,841,925 | 0 |

Deployed `index.html`: 406,835 bytes, SHA-256 `f069e2413d91b3f4afcb3d4492afdfdc193b273965eabeea2eaa608b4d005e5b`.

**Verdict: PASS.** Production serves the exact finished Pass-16 index and every exact Pass-16 live WebM/poster byte on both hosts.
