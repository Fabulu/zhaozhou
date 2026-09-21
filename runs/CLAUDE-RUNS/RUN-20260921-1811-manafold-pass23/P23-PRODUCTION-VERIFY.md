# Manafold pass 23 — production verification

**Date:** 2026-09-22
**Deploy:** `deploy.ps1 -Project upheaval -Branch main`, RC **0**,
record `deploy-20260922-005226.txt`
**Live:** https://upheaval.pages.dev · deployment alias
https://dfecbade.upheaval.pages.dev

---

## Heads at deployment

| ref | commit |
|---|---|
| **Zhaozhou main** | `5b167ca90b5fe85af3b491b77484efae73b97b47`, fast-forwarded from `83002801` |
| **Upheaval main** | `0d805d752ea2c7a12d582b1f7771fc50b87814f2`, fast-forwarded from `d64fed91` |
| Zhaozhou `manafold-pass23` | `5b167ca9` — equal to main |
| Upheaval `manafold-pass23` | `0d805d75` — equal to main |

Both fast-forwards were run with the **checkout's exit code gating the merge**
and the branch-vs-main comparison taken **after** it, on two genuinely distinct
refs — the failure pass 22 recorded, where an aborted `checkout main` let a
script merge a branch into itself and then compare its head to its own head.
Both `--ff-only` merges returned 0 and both pushes returned 0.

---

## The bank behind this deploy

| receipt | value |
|---|---|
| accepted source | zhaozhou `d6532cc1` (`tools/reel` tree `35f0c13e`, working tree clean) |
| renderer MD5 | `3e9104bae2777a3160da9bbe37bd0b9c` |
| renderer SHA-256 | `fcae1a57e9cce0e357997b35fbe7657e002387e6f0a6fe646cd212164bbbaf77` |
| bank manifest SHA-256 | `2f3cfb8175f4e4cad80ced5351591ca1fdaa6fc5ee3fa768f4449059ecdffb18` |
| bank | 22 subjects, **7,992 frames**, ONE invocation, production ink, RC 0 |
| frame counts | every subject at its exact pass-22 count, **verified against the frames on disk** |
| scope | **4 of 22 subjects differ; 18 byte-identical across 7,992 frames** |
| encode | RC 0, **22/22** webm + posters |
| live media | 44 files, 43,681,362 bytes (`P23-LIVE-MEDIA-SHA256.txt`) |

The renderer was **built from scratch by the reviewer** and its gate output is
byte-identical to the implementation's committed receipt, so the shipped bank is
tied to the reviewed configuration rather than to a claim about it.

---

## Local gates before the deploy — real exit codes

| gate | RC | result |
|---|---|---|
| `assemble.py` | 0 | 2 creatures, 826 renders |
| `checkfresh.py` | 0 | 22 rendered subjects — **22 live fresh, 0 STALE**, 0 absent, 0 unknown |
| `checkarchive.py` | 0 | v17 56/56, **v18, pass 19, 20, 21 and 22 all 44/44 and equal to their published receipts**; live names match the **pass-23** receipt 44/44 |
| `checkarchive.py --selftest` | 0 | **31 red legs fire, 12 positive legs accept** |
| `checkplayback.py` | 0 | Fall controls-only/non-looping, Hover autoplay+loop |
| `checkmedia.py` | 0 | **1,640 declared files, 1,640 decoded** — full sweep, no skip |
| robots | — | exactly **one** `noindex, nofollow` meta |

`deploy.ps1` re-ran its own gate chain (including the full ~22 min decode sweep)
before uploading; all passed.

---

## Production verification — cache-bypassed, both hosts

**The verifier was selftested on deliberately broken copies FIRST**, before any
host was queried, so its silence means something. It returned RC 0 with:

> `SELFTEST OK: local page passes; 10 index negatives fire (a pass-19 archive
> clip dropped, a pass-20 archive clip dropped, a pass-21 archive clip dropped,
> a pass-22 archive clip dropped, generation count stale, hover loop stripped,
> old p22 card, renderer md5 altered, robots index,follow, scope sentence
> dropped); corrupted, truncated and non-200 media fire`

Three of those negatives are new this pass and had never been run before: the
pass-22 archive lock, the generation count at 18, and the **scope sentence** —
the card's actual claim, which would otherwise be the one thing on the page with
nothing watching it.

| host | rows verified | bytes | mismatches | retries |
|---|---|---|---|---|
| `https://upheaval.pages.dev` | **63 / 63** | 87,905,004 | **0** | 0 |
| `https://dfecbade.upheaval.pages.dev` | **63 / 63** | 87,905,004 | **0** | 0 |

63 rows = the index + **44 live media** + **18 archive spot checks**, three from
each of the six locked generations (version-17, version-18, pass 19, pass 20,
pass 21 and now pass 22), so a generation cannot be locked in the repo and
silently absent from the host.

**index.html**, identical on both hosts and byte-for-byte equal to the local
deployed file:

| | |
|---|---|
| SHA-256 | `d80f6256e413bc190359e70e9ef4a25c1b1858ab72b84a16c9ea5a24fb0caba3` |
| bytes | 477,562 |

All sixteen index content checks true on both hosts:
`robots_exactly_one_noindex_nofollow`, `card_pass_23`, **`scope_four_moved`**,
`renderer_md5`, `manifest_sha256`, `source_d6532cc1`,
`22_subjects_7992_frames`, `all_44_live_declared`, `v18_archive_22_declared`,
`p19_archive_22_declared`, `p20_archive_22_declared`,
`p21_archive_22_declared`, **`p22_archive_22_declared`**,
**`archive_18_generations`**, `fall_no_autoplay_no_loop`, `hover_autoplay_loop`.

The upload itself corroborates the scope: wrangler uploaded **25 new files**
against 1,682 already present — 22 videos + 2 posters + the index, exactly the
set the receipts predicted.

---

## Two instrument findings, recorded because both would otherwise be quoted

**Pass 22's bank-manifest CRC column does not describe what pass 22 shipped.**
Compared against it, all 22 pass-23 subjects "changed" — impossible when
eighteen are untouched. Pass 22's own identity receipt, a fresh exact-off render
and, decisively, the lossless poster stills coming out byte-identical to pass
22's published posters all agree against it. That manifest's SHA was published
as provenance in `P22-PRODUCTION-VERIFY.md`. **Pass 23 does not compare against
it**, and its own manifest was cross-checked against the frames on disk.

**The video encoder is not byte-reproducible.** The same 600 frames encoded
twice give different bitstreams of identical length, so a video hash can never
prove a clip did not change. All 22 videos carry new bytes this pass, including
the eighteen clips whose pictures are identical. The **frames** are the evidence;
the posters are reproducible and 20 of 22 match pass 22 exactly.

---

## Cleanup

Raw frame roots and full-resolution sheets removed after verification:
`p23-final-reel-22`, `p23-exactoff-reel-22`, `p23-sheets`,
`Upheaval/website/scratch-reel`. `.rgb` sweep run afterwards to catch anything
`.gitignore` hides.

**Status: Complete.**
