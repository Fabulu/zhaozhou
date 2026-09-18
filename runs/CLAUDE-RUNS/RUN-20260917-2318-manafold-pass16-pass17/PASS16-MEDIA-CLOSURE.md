# Manafold Pass 16 media closure

**Accepted renderer:** MD5 `1255A8F8DEE778F7E76DCC7759D678B0`
**Raw bank:** 28 subjects / 11,592 frames
**Encode log:** `final4-encode.log`
**Exact live-media hashes:** `PASS16-LIVE-MEDIA-SHA256.txt`
**Gate log:** `pass16-media-gates.log`

## Encode

`website/tools/tovideo.py` consumed all 28 exact final4 subject directories through a junction at `website/scratch-reel`, avoiding a second raw-frame copy. It produced 28 VP9 CRF16 4:4:4 WebMs and 28 posters. Every subject's encoded frame count matches its exact render receipt. Encoder RC: **0**.

Live Pass-16 media manifest: **56 files, 54,435,090 bytes**. The 28 live WebM names exactly equal the 28 non-archive Manafold declarations; every companion PNG exists.

## Archive

Pass 15's 28 prior live WebMs were copied before overwrite and verified SHA-256-identical pairwise. Archive commit `830c46c` is on `origin/manafold-pass16`.

## Canonical pre-deploy gates

Invoked:

```powershell
website\deploy.ps1 -Project upheaval -Branch main -AssembleOnly
```

No skip flags.

- assembly: 2 creatures / 666 render entries;
- robots: exactly `noindex, nofollow`;
- freshness: 28 subjects, **28 fresh, 0 stale, 0 absent**;
- full decode: **1,320 declared, 1,320 decoded**;
- overall RC: **0**.

Direction 14 then added card wording only; media bytes did not change. Reassembly and the cheap freshness gate were repeated: still 28/28 fresh. Final local `public/index.html`: **406,835 bytes**, SHA-256 `dd8b2ea82b4a930a51e7136147a86478fe08de58ce6d766ed9f7167401a18baa`, exact robots directives `noindex`, `nofollow`.

The production deploy may skip only the expensive decode sweep because the same unchanged 1,320 files just passed it; freshness must run again and the deploy record must state the skip. Production verification compares the final index and all 56 Pass-16 live media files against the local hashes.
