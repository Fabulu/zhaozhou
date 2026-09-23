# Manafold pass 25 — PRODUCTION VERIFICATION — **the creature's final pass, live**

**Date:** 2026-09-23
**Deployed:** `website/deploy.ps1 -Project upheaval -Branch main`, exit **0**
**Deployment:** https://034764be.upheaval.pages.dev
**Production:** https://upheaval.pages.dev
**Verdict:** **69 / 69 on BOTH hosts, 0 mismatches, 0 retries.**

---

## 1. THE RESULT

| host | verified | bytes | mismatches | retries | index checks |
|---|---:|---:|---:|---:|---|
| `https://upheaval.pages.dev` | **69 / 69** | 103,465,143 | **0** | 0 | 31 run, **0 failed** |
| `https://034764be.upheaval.pages.dev` | **69 / 69** | 103,465,143 | **0** | 0 | 31 run, **0 failed** |

69 rows per host = **1 index** + **44 live media** + **24 archive spot checks**
(three from each of the eight locked generations: version-17, version-18,
pass-19, pass-20, pass-21, pass-22, pass-23 and **pass-24**). Both hosts returned
the identical byte total, which is the cheapest possible cross-check that they
are serving the same build.

Every media row is checked on **status, exact byte length and SHA-256** against
`P25-LIVE-MEDIA-SHA256.txt` and the archive receipts. The index is compared
**byte for byte** against the deployed `public/index.html` *and* run through 31
content checks. Every request carries a unique `?p25verify=<uuid>` and
`Cache-Control: no-cache, no-store, max-age=0`, so nothing was answered from a
cache.

Receipt: `P25-MEDIA-CLOSURE-RECEIPTS/production-verify.log`,
`p25-production-verify.json`.

## 2. THE INSTRUMENT WAS FIRED FIRST

`p25_production_verify.py --selftest` was run **before any host was queried**,
and again after the deploy against the page that actually shipped. Both **rc 0**:

* **22 index negatives fire.** Each breaks the page one way and asserts that a
  named check goes red. Fourteen are inherited; the new ones are this pass's
  claims, and they exist because *a card that keeps the fix and loses the reason
  invites the same four passes again*:
  * `final-pass sentence dropped` — the one fact on the page that changes what
    the owner does next.
  * `rollout size dropped`, `nearest-miss numbers dropped`,
    `non-monotone warning dropped` — item 1's size, the number that **located**
    the complaint after a gate reading zero had failed to, and the trap the next
    person would otherwise walk into.
  * `back-ball finding dropped`, `carrier C identification dropped`,
    `cancellation finding dropped`, `filter/knob distinction dropped` — the
    diagnosis, in four pieces. The fix is one function; the reason is four
    passes of work.
  * `front-spin disclosure dropped`, `crackle offer dropped` — the two things
    the owner must not be surprised by. A page that quietly loses either is
    reassuring and incomplete, which is worse than either fault.
  * `manifest sha altered`, `reviewer provenance dropped`,
    `a pass-24 archive clip dropped`, `generation count stale`.
* **Corrupted, truncated and non-200 media all fire.** A one-byte flip in the
  middle of a real file, a file one byte short, and a 404 carrying correct bytes
  are each rejected.

**A detector reading zero is a claim.** These fired before their silence was
quoted.

## 3. ONE FAULT IN MY OWN INSTRUMENT, FOUND AND FIXED MID-RUN

The first verification run named `https://main.upheaval.pages.dev` as the second
host. **That alias does not exist for this project — it 404s.** The verifier's
retry policy is five attempts with 5/10/15/20-second backoffs, so every one of
its 68 URLs on that host cost **50 seconds of sleeping** before failing. The run
sat for over half an hour with an empty log, **and the shape of that is worth
recording: a job with no progress output is indistinguishable from a job that is
working.** Exactly the fault `P25-BACKBALL.md` §5 records three times over.

What settled it was not waiting longer. It was **probing the four URL shapes
directly**, which took eight seconds and printed `HTTPError 404` against
`main.upheaval.pages.dev` while the other three returned 200 in under a third of
a second. Pass 24's own receipt then said what the second host should be: the
**per-deploy alias**, `b7e1fe29...` then, `034764be...` now.

The stuck process was killed **by PID, after confirming `p25_production_verify`
in its own command line** — CLAUDE.md's identify-before-you-kill rule, in a tree
where several lanes run the same executable names.

## 4. THE PAGE IN THE REPOSITORY IS NOW THE PAGE ON THE HOSTS

**A durable provenance gap was found and closed this pass.** The committed
`index.html` had **never** been the bytes production serves:

* `core.autocrlf` is `true` on this machine and `assemble.py` writes the page
  with CRLF (`creatures.json` is CRLF and the assembler carries it through), so
  git stored an **LF-normalised** copy — about **16 KB** of difference, on every
  pass, silently.
* It matters on this page specifically because the verifier compares production
  **byte for byte against the working file**. The verification was always honest
  about production; it was the **repository** that could not be used to
  reconstruct what had been served.
* Pass 24 saw the symptom and committed *"the index.html production actually
  serves"* — and still committed an LF-normalised blob, because that note was
  about the card text, not the bytes.

`.gitattributes` now marks `website/public/index.html` as `-text`. The blob, the
working file and both hosts are the same bytes, and the hashes were compared to
prove it rather than assumed.

**And the deploy restamps the page.** `deploy.ps1` re-runs `assemble` as its
first gate, and `assemble` writes its own build time — so the file that went to
Cloudflare says `Built 2026-09-23 10:31 UTC` while the one committed minutes
earlier said `10:23`. One line. The **served** file was committed afterwards and
is what the verification above compares against.

## 5. RECEIPTS

**Renderer:** `.tmp/p25rev/bin/zhao-reel-cel.exe`
MD5 `a0c0c80a2e9b02dc852c25c1ff1aa530`,
SHA-256 `85c3c7e3c7e1d2390793ffb161771710807ed973f217ca21e837d917bb067237`.
Built directly with `tools/reel/build-direct.sh` and the `zhao-env.ps1`
toolchain (g++ 16.1.0 MinGW-W64 ucrt) **by the independent reviewer, not by the
author of the change**, from a clean tree. It reproduces the implementation's
own 22-subject bank **byte for byte on all 22**. No CMake was used.

**Bank manifest SHA-256**
`23cd615d25ceb27f29231c769f3e1a507e2d9cc2dcc74612da247b99fc3cef16`
— 22 subjects, 7,992 frames, **ONE invocation**, production ink
(`ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`), **no override of any
kind**, every subject keeping its exact pass-24 frame count.

**Accepted source:** zhaozhou `b3c5760e`.

**Live media receipt:** `Upheaval/creature/Manafold/P25-LIVE-MEDIA-SHA256.txt`,
44 files, **44,526,868 bytes**. Every `.webm` ffprobed for `vp9` / 384×240 /
`yuv444p` / `60/1` **and its exact frame count**; every poster for `png` /
1152×720.

**Archive:** `P24-ARCHIVE-SHA256.txt`, 44 files, **43,768,883 bytes**, copies
re-hashed against the published pass-24 receipt before anything was overwritten.
The archive is now **twenty generations**.

**Local gates, real exit codes, none read through a pipe:**

| gate | rc | result |
|---|---:|---|
| `assemble.py` | 0 | 2 creatures, 870 renders (it correctly **refused twice** until both style.css archive-generation families reached 20) |
| `checkfresh.py` | 0 | 22 rendered subjects, **22 live fresh, 0 STALE, 0 absent** |
| `checkarchive.py --selftest` | 0 | **43 red legs fire, 16 positive legs** accept a valid contract |
| `checkarchive.py` | 0 | every generation v17→pass-24 equal to its own published receipt; live names match the pass-25 receipt 44/44 |
| `checkplayback.py` | 0 | Fall controls-only/non-looping, Hover autoplay+loop |
| `checkmedia.py` | 0 | **1728 declared files, 1728 decoded** |
| `decodecheck.sh` | 0 | **44 files, 0 failures** |
| noindex | — | **exactly one** robots tag, `content="noindex, nofollow"` |
| `deploy.ps1 -Project upheaval -Branch main` | **0** | 45 files uploaded (1750 already uploaded) |

## 6. THE FOUR HEADS

| ref | commit |
|---|---|
| Zhaozhou `manafold-pass25` | see §7 |
| Zhaozhou `main` | fast-forwarded, FF-only, ancestry checked before the merge |
| Upheaval `manafold-pass25` | see §7 |
| Upheaval `main` | fast-forwarded, FF-only, ancestry checked before the merge |

Both fast-forwards were checked with `git merge-base --is-ancestor main <branch>`
**before** merging, and both were `--ff-only`.

## 7. WHAT IS LEFT OPEN ON THE CREATURE

Pass 25 closes Manafold. `PASS-25-FINDINGS.md` §7 is the standing list; the three
an owner would act on:

1. **The front ball's own SPIN is 35 % slower.** Its position, and the lift he
   approved in pass 24, are on `kBJunctionF` — a bone the smoothing cannot reach.
   Separating the swell's own spin from station A needs a second rotation
   channel. **The one most worth his eye.**
2. **Crackle carries the same rear fault** and is the worse case — a long idle
   under a fixed close camera. **One table entry:**
   `kBackBallDampClipPm[23] = 700`.
3. **`manafold-hasty` wraps with a 38.8 px jump.** Its ink centroid drifts from
   x = 211.0 to x = 172.2 across the clip while no frame-to-frame step exceeds
   1.47 px; every other live subject closes its loop under 1.6 px. **Measured on
   the pass-24 bank from the same tree it is 39.52 px**, so it is pre-existing
   and nothing in this pass caused it — but nobody had measured it before, and
   it is the kind of thing that reads as a stutter on a loop.
   Receipt: `P25-BANK-RECEIPTS/loop-seam-scan.txt`.
