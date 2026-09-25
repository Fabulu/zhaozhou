# Manafold pass 26 — production verification

**Deployment:** https://7dec0772.upheaval.pages.dev
**Production alias:** https://upheaval.pages.dev
**Deployed:** 2026-09-25, `deploy.ps1 -Project upheaval -Branch main`, RC 0
**Deploy record:** `Upheaval/website/deploy-records/deploy-20260925-200134.txt`

---

## VERDICT: **VERIFIED ON BOTH HOSTS**

| host | rows | verified | bytes | mismatches | retries | index checks |
|---|---|---|---|---|---|---|
| `https://upheaval.pages.dev` | 72 | **72** | 111,795,529 | **0** | 0 | 42 run, **0 failed** |
| `https://7dec0772.upheaval.pages.dev` | 72 | **72** | 111,795,529 | **0** | 0 | 42 run, **0 failed** |

72 rows = `index.html` + **44 live media** + **27 archive spot checks**, three
from each of the nine locked generations (v17, v18, p19, p20, p21, p22, p23,
p24 and the new p25). Every fetch is cache-bypassed — a fresh `?p26verify=<uuid>`
query plus no-cache headers — and compared by **exact SHA-256 and byte length**,
never by size alone. `index.html` is compared **byte-for-byte** against the
local deployed `public/index.html`.

## The verifier was proved able to fail BEFORE its silence was quoted

`P26-MEDIA-CLOSURE-RECEIPTS/p26_production_verify.py --selftest` → **RC 0**:

> local page passes; **33 index negatives fire**; corrupted, truncated and
> non-200 media fire.

Each negative breaks the page in one specific way and asserts that the matching
check goes red. A check that has only ever been seen green is a claim, and this
project has shipped several.

**Four of the 33 are this pass's most important**, because they guard the
correction the review made rather than the work the pass did:

* `the corrected seam figures dropped` — removes "163 pixels down to 111".
* `the relative seam figure dropped` — removes "238 TIMES anything else on
  screen".
* `the admission dropped` — turns "The review threw that out" into "The review
  agreed".
* **`the vanished claim put BACK`** — re-inserts "The seam effectively
  vanished", which `no_standalone_vanished_claim` must reject. The implementation
  was about to publish that sentence; a page that quietly reverts to it would be
  **worse** than one that never carried the correction, because it would look
  reviewed. That negative fires.

### Three faults in the verifier itself, all found by its own selftest

Worth recording, because each one would have produced a **green check that could
not fail**:

1. **`\b` became a literal backspace (`\x08`) in six regexes**, because a
   generator script wrote them through a shell heredoc into a non-raw Python
   string. `re.search(r'\x08autoplay\x08', …)` matches nothing, ever — so
   `hover_autoplay_loop` and `fall_no_autoplay_no_loop` were dead.
2. **Three check strings carried raw apostrophes.** The assembler escapes `'` as
   `&#x27;`, so `"Startle's payoff …"` could never match the served page. The
   pass-25 docstring warns about exactly this; the checks now accept `'`,
   `&#x27;`, `&#39;` and `’`, and the matching negatives mutate the **escaped**
   form, or they could not fire either.
3. **The quoted-claim stripper targeted `&ldquo;`** where the assembler writes
   `&quot;`, so `no_standalone_vanished_claim` was reading an unstripped page.

All three were caught by the selftest refusing to pass, which is the whole
argument for having one.

## Index content checks — 42, all green on both hosts

Beyond byte-identity, the page is checked to still *say* the things that matter:

* **Privacy:** exactly one `<meta name="robots" content="noindex, nofollow">`.
* **The diagnosis** (5 checks): the travel was DELETED, the camera was never
  told, a pan moves the ground with the creature, the two speed-cue numbers, and
  the 45°-into-the-lens finding.
* **Speed** (3): the 5→13 cadence, the lean is no longer a pose, the margins.
* **The face** (4): brow tops TOGETHER, the explicit contrast with Startle's
  tops-APART, the three checks, and why the camera had to move first.
* **The seam correction** (5), listed above.
* **Containment** (2): the owner's two closed items named as identical, and
  "exactly 240 of them differ / All 240 are Hasty".
* **The red gate** (2): that two floors had been RED since before pass 25
  shipped, and that pass 25's published clips were never in doubt.
* **Provenance** (6): renderer MD5 `b822bb9bd7ceb8f0b921623566b26169`, bank
  manifest SHA-256
  `eb6d4e9a8191d8e958711eb09d955842bec5782ef53c2b236d206b81126c5e13`, source
  `zhaozhou bfb22a65`, 22 subjects / 7,992 frames, the reviewer-built renderer,
  and "No bound anywhere was relaxed".
* **Honesty:** the card does not overclaim the frame review.
* **Structure** (10): all 44 live paths declared; 22 distinct clips declared in
  each of the v18/p19/p20/p21/p22/p23/p24/**p25** archive collections;
  "Archive (21 generations)"; Fall still controls-only and non-looping; Hover
  still autoplay+loop.

## Local gates before the deploy — real exit codes, never through a pipe

| gate | RC | result |
|---|---|---|
| `assemble.py` | 0 | 2 creatures, 892 renders (**refused once first — see below**) |
| `checkfresh.py` | 0 | 22 rendered subjects, **22 live fresh**, 0 stale, 0 absent |
| `checkarchive.py` | 0 | **every generation locked**, incl. pass-25 44/44; live names match the pass-26 receipt 44/44 |
| `checkarchive.py --selftest` | 0 | **49 red legs** fire (43 before), 18 positive legs |
| `checkplayback.py` | 0 | Fall controls-only/non-looping, Hover autoplay+loop |
| `checkmedia.py` | 0 | **1,772 declared files, 1,772 decoded** |
| `decodecheck.sh` | 0 | **44 files, 0 failures** (22 live + 22 new archive webms) |
| noindex | — | exactly one meta tag, `noindex, nofollow` |

`deploy.ps1` re-ran assemble, checkplayback, checkarchive, the robots gate,
checkfresh and checkmedia itself before uploading; all green inside the deploy.

### Two gates earned their keep, and one could not fail

* **`assemble.py` REFUSED THE BUILD.** `style.css` wired 20 archive-generation
  slots and Manafold now has 21, so the build stopped rather than shipping
  generation buttons that select nothing — precisely the failure that guard
  exists for. Both `.archive-generations` nth families were extended to 22 in
  the *same edit* as the constant, as the guard's own comment demands.
* **`decodecheck.sh` exited 0 on an EMPTY argument list** — `checked 0 files, 0
  failures`, a green line about nothing at all. Found by calling it wrong, which
  is how it would be found in anger. It now refuses an empty list, and the repair
  was proved **both ways**: empty → RC 2, a truncated webm → RC 1.

## The bank

| | |
|---|---|
| subjects | **22**, one invocation, production ink, **no override of any kind** |
| frames | **7,992** |
| renderer MD5 | `b822bb9bd7ceb8f0b921623566b26169` |
| renderer SHA-256 | `98459381a2ef0b3eb10a9f46be46d146a3f87cce8848c1086d4a1b5add8d6a6a` |
| bank manifest SHA-256 | `eb6d4e9a8191d8e958711eb09d955842bec5782ef53c2b236d206b81126c5e13` |
| accepted source | `zhaozhou bfb22a65` |

The renderer was **built from scratch by the independent reviewer**, not by the
author of the change, and reproduces the implementation's own 22-subject bank
**byte for byte on all 22**.

**Containment proved three independent ways:**

1. **Rendered frames:** 240 of 7,992 differ, and all 240 are `manafold-hasty`.
   Crackle 600/0 and Hover 600/0 — the owner's closed items.
2. **Scope proof vs pass 25:** exactly one subject's CRC moved; zero frame
   counts changed.
3. **Encoded media:** of the 22 WebMs, **exactly one changed size**, and it is
   Hasty. (The encoder is not byte-reproducible, so this is a cross-check and
   never the proof — containment is always argued on the rendered frames.)

## The archive

Pass 25 preserved **before** the encode, which is the whole safety property: the
encode overwrites the live names in place, and after it the pass-25 bytes exist
nowhere but production. All 44 live files were verified against the published,
production-verified `P25-LIVE-MEDIA-SHA256.txt` **first**, then copied, then the
**copies** re-hashed: 44/44, 44,526,868 bytes, receipted in
`P25-ARCHIVE-SHA256.txt`. One new generation, each clip declared exactly once,
archive count 20 → **21**. `checkarchive.py` extended to lock it.

## Open issues

Carried from `PASS-26-FINDINGS.md` §7 — nothing new was found in production:

1. **Drift (slot 1) has the same 45° traverse fault** that was repaired on Hasty.
   Untouched, byte-identical. Worth a pass of its own.
2. **The pass-25 pink-ink seam mask is background-dominated**; the 38.8 px figure
   in the pass-25 records is not a creature displacement. Use
   `tools/reel/seamdisp.py`.
3. **The f238→f239 jump is the clip's one hitch**, accepted by Direction 27.
4. **`mqa` Q3's wrap column is printed and not gated**, so nothing bounds a loop
   discontinuity.
5. **The pass-24 ambient eye SIZE layer still cannot reach 17 of 22 clips** — a
   question for the owner, not a defect to fix silently.
6. **`screenmotion.py`'s band correlator is unreliable on this staging.**
7. `kU02HastyBiasX` is unused on the hurry path, retained for the exact-off path.
