# QA — Manafold pass 4, the INDEPENDENT SECOND GATE

**Run:** RUN-20260905-0933-manafold-pass4-implementation · 2026-09-05
**Role:** independent QA after the reviewer. Brief: confirm, refute or re-rank —
a refutation is the most valuable output. Nothing was republished.
**Judged on:** QA's own worktrees and builds, never the reviewer's artefacts:

* `C:\zqaclean` — `git worktree add --detach HEAD` (2af79e85), built with
  `tools/reel/build-direct.sh --output C:\zqacleanbuild` (never `cmake --build`).
* `C:\zqap3` — `git worktree add --detach dd85b719`, the pass-3 baseline, built
  by QA into `C:\zqap3build` for calibration.
* `ZIXX_EXP=celmain`, native 384x240 first, magnified after.

**Evidence:** `evidence/qa/`.
**Note:** the implementer's own gate, which `REVIEW.md` cites as "QA.md", is
preserved verbatim at `QA-IMPLEMENTER-SELF-GATE.md`. This file is the second gate.

---

## The one-line verdict

**The reviewer is substantially right, and its single most consequential
diagnosis is wrong.** The build fault, the legibility cause, the knead coverage
gap, the dead knob and the mana covering the antenna all reproduce from QA's own
builds. But the eye-protrusion regression is **not** caused by `kEyeZMm`, the
constant the reviewer isolated — reverting that constant makes protrusion
*worse* — and the true cause reframes the fault from a regression into largely a
measurement artifact. One new fault the reviewer missed: a declared media file
that 404s on the live page.

---

## Calibration before anything (gate checklist 4)

* QA's HEAD build renders `manafold-hover` at `sequence_crc32c=0x5B44FCF2` — the
  same value the reviewer reported from a separate build. Two independent builds
  agree. (There is no stored shipped `meta.txt` to compare against;
  `website/scratch-reel` is render output and is not in the repo.)
* QA's **pass-3** build reproduces pass 3's published eye number exactly:
  `eye crown ellip 1365 pm — stands 164 mm proud`.
* The eye-crown probe block is **byte-identical** between
  `dd85b719:u02_probe.cpp` and `HEAD:manafold_probe.cpp`, so the cross-pass
  comparison is one instrument, not two.
* `U02_FOLD_LOCK` demonstrably changes the render (hover `0x5B44FCF2` vs
  `0xF975D2E1`), so the X-ray knob can fail.

---

## Verdict per acceptance item

| # | Direction 4 acceptance item | Reviewer | **QA** |
|---|---|---|---|
| 1 | Named Manafold, folder renamed | PASS | **PASS** (inherited + site check) |
| 2 | Folds mana into shapes, kneads them, continuously, without touching | PARTIAL | **PARTIAL — mechanism PROVEN, read FAILS** |
| 3 | Tumour balls gone; front+back junction balls; a bone at every ball | PASS | **PASS** (inherited; spot-looked) |
| 4 | Antenna thin, thickening only at junctions | PASS | **PASS** (inherited; spot-looked) |
| 5 | More/bigger particles, smoother rotation, fewer lines, glitchier smear, odd drifters | PARTIAL | **PARTIAL — and the "bigger" knob is the item-2 blocker** |
| 6 | Smear is depth-correct | PASS | **PASS** (inherited from code + reviewer's look) |
| 7 | Eyes: not touching, outward, teardrop, whites track pupils | PASS + undeclared regression | **PASS, with the regression RE-DIAGNOSED (below)** |
| 8 | Interior glow gone; outer layer more see-through | PASS / owner eye | **PASS on the glow; owner call stands** |
| 9 | Outline-thickness question answered | PASS | **PASS** (inherited) |
| 10 | Directional hit animations | PASS + dead knob | **PASS; dead knob CONFIRMED** |

---

## 1. The untracked build bug — CONFIRMED, and the blast radius is now bounded

Reproduced end to end from QA's own worktree
(`evidence/qa/03-page-untracked-proof.txt`, plates `01`/`02`):

* Clean checkout of HEAD: `manafold_page.h` **absent**; all three Zixxtrixx pages
  present.
* `build-direct.sh cel` **succeeds with no error and no warning**.
* Render: `u02-s4-front: 4 frames, 77 unique colours` — **a solid black creature**.
* Regenerate with the committed generator → **sha256-identical** to the lane's
  untracked file. Nothing is lost.
* Rebuild with the page, **same commit**: `803 unique colours`, correct.

Both of the reviewer's numbers (77 / 803) reproduced exactly. The instrument is
honest: `subject_u02_s4(1)` sets no `creature_shade` and `u02_common()` pins the
shipping sun, so this is the lit shipping path — an unlit diagnostic could not
have shown it (`09-ENGINE-GOTCHAS.md` §7).

**Blast radius, established by QA — narrower than feared, which sizes the fix:**

* `__has_include` appears **exactly once** in the whole `tools/` tree.
* `manafold_page.h` is the **only untracked source/header in the repository**.
* All three Zixxtrixx page headers are **tracked**.
* `build-direct.sh` `build_cel()` **already hard-errors** on a missing
  `zixxtrixx_page_cel.h`. Manafold has no equivalent guard.

So: one file, one guard, and the hard-error precedent already exists ten lines
away in the same script. This is not a systemic pattern.

**Two false statements in the source** (gate checklist 8 — a comment is untested):

* `manafold.h` calls the page *"gitignored, never tracked"*. `.gitignore:91` still
  names the **pre-rename** `unnamed02_page.h`; `git check-ignore` says the file is
  **not ignored**, merely uncommitted. Committing it needs no `.gitignore` change.
* The same comment promises a fresh clone will *"build grey rather than
  breaking"*. It builds **black**, and the same comment block twelve lines later
  says untextured parts render black under celmain. The file contradicts itself.

**QA's recommendation:** take the reviewer's hard-error route *and* commit the
page. They are not alternatives — the guard protects every future creature, and
committing an 800 KB deterministic header costs nothing against a silent black
creature. Fix the two comments and the stale `.gitignore` line in the same edit.

## 2. The eye protrusion — number CONFIRMED, cause REFUTED, severity RE-RANKED

Full working: `evidence/qa/04-eye-protrusion-REFUTATION.txt`.

The number is real and reproduces exactly: **1365 pm / 164 mm (pass 3) → 1275 pm /
123 mm (HEAD)**, on an instrument QA proved unchanged between the two passes.

**The reviewer's cause does not survive testing.** It concluded `kEyeZMm` 190→215
— the separation that fixed "the eyes touch" — slid the crowns off the forward
pole, and framed it as *"fixing 'they touch' cost 'they stand out'"*. QA reverted
that one constant on HEAD and rebuilt:

| build | eye crown |
|---|---|
| HEAD, `kEyeZMm = 215` | 1275 pm / **123 mm** |
| `kEyeZMm = 190` (pass-3 value), nothing else changed | 1247 pm / **111 mm** |

Reverting the separation makes protrusion **12 mm worse**. `kEyeZMm` 190→215 was
an *improvement*. (Also, it was not the only changed eye constant —
`kEyeVAngleA16` moved −4400 → −3600.)

**What the probe actually measures.** It takes the **max over every posed vertex
forward of +300 mm** — not the lens. QA instrumented a scratch copy to report the
per-part maximum, identifying parts by material colour:

| part | pass 3 (dd85b719) | HEAD |
|---|---|---|
| cyan pupil star `(64,220,240)` | **1365 pm / 164 mm** | 1269 pm / 121 mm |
| purple lens `(116,58,178)` | 1218 pm / 98 mm | **1228 pm / 102 mm** |
| white ring `(246,242,250)` | *did not exist* | **1275 pm / 123 mm** |

**The lens never regressed** — it is marginally better. The entire loss is in the
cyan star, and the proudest part at HEAD is a component that did not exist in
pass 3.

**The real cause, established causally by sweep on HEAD:**

| variant | result |
|---|---|
| `kPupilStarArmLongMm` 150→185 | no change (tangential, not radial) |
| `kEyeZMm` 215→190 | **worse** |
| **`kPupilStarArmShortMm` 88→185** | **1397 pm / 178 mm — recovered past the protected read** |

The crossed star blade was shortened 185 → 88 in Stage E. `manafold_model.h` says
why: *"the crossed (short) arm must FIT the lens half-width"*. And `dd85b719`'s
own commit subject is *"the star still leaves the lens"*.

**So pass 3's 164 mm was produced by a star arm poking outside the lens — the
exact fault pass 4 was directed to remove.** The probe was partly measuring the
fault. Re-lengthening the arm to recover 166 mm would re-break star containment.

The reviewer's *prescription* (raise `kEyeDeepMm`/`kEyeXMm`) is right. Its cause,
its severity, and its "one acceptance item broke another" pairing are wrong — the
pairing is **star containment vs protrusion**, not eye separation vs protrusion.
The gate should be re-baselined against **lens** geometry rather than a max over
every face vertex, or it will keep rewarding a star that escapes its lens.

## 3. The shape-legibility cause — CONFIRMED; one wording refuted

`evidence/qa/09`, plates `05` (native) and `06` (4x).

Bright-core connected components on the HOLD frames give **73–99% of the core in
two components** on every frame, in **both** normal and `U02_FOLD_LOCK=1`
variants. (The reviewer measured 75–96% on the X-ray; different threshold, same
conclusion, independently reached.) Locking coherence to maximum and deleting the
cloud does **not** un-fuse the core, so the cause is the stencil fusing, not the
cloud swallowing it. The brush arithmetic checks out from the constants: 18
stations ~11 px apart under a **14–20 px** stroke — neighbours overlap, and the
polyline closes into a slab before it is drawn.

**At native 384x240 QA cannot name RING and cannot name STAR** in either variant.
The acceptance read fails at the shipping camera.

**Refuted wording:** *"the X-ray looks the same"* is too strong. At 4–6x the
locked render is visibly crisper, the RING's hole cleaner, and component counts
drop materially (61→43, 82→53, 33→9). The cloud does add diffuse satellite
specks; what it does not do is cause the fusion. The conclusion stands, the
sentence does not — and it matters, because "the cloud is irrelevant" would send
the next pass past a real secondary contributor.

This remains **an owner-level trade**: "make the particles bigger" (§2, twice)
and "fold into recognisable shapes" (§0) are the same knob pulled opposite ways.

## 4. The mana burying the antenna — CONFIRMED, and re-ranked per clip

Plates `07` (taunt2 f160–170 at 3x) and `08` (buried vs good).

* **`taunt2` f160–170 — the loop is submerged in solid cyan and unreadable.** The
  exhibit holds. Small precision note: the black ink silhouette and a few pink
  slivers do survive, so "completely invisible" is marginally strong; the read is
  nonetheless a cloud sitting on the hand.
* `rest` f335 — loop traceable, mana coats the upper region. Moderate.
* **`pirouette` f239 — the loop READS.** Mana fills the hole but does not bury it.
  The reviewer's *"buries the loop entirely"* **overstates this clip**.
* `drift` f179 and `hasty` f131 are the wins. On `hasty` there is a **visible gap**
  between the antenna and the bulk of a long lagging trail — the iron-filings read
  the direction asked for. Protect the DRAG term.

**Discarded instrument, recorded not quoted (checklist 6):** QA first ranked this
by peak cyan pixels as a fraction of creature pixels. It cannot detect the fault —
`hasty` (good) scored 6.2% against `taunt2`'s 6.5%. It counts mana *quantity*, not
coverage of the loop. Discarded; the ranking above is by looking.

## 5. Is the centrepiece proven, and by what? — YES, BY THE CODE. Not by the ablation.

Stated plainly, because the brief asks for it:

**The coupling is proven by construction, in source, and the proof is strong.**
`mana_fold` builds `anchors[6]` from the **posed** bone origins
(`A.junction_f, A.neck, A.hinge_a, A.hinge_b, A.hinge_c, A.junction_b`), and each
stencil point is `q[k] = (Σ w[i] · anchors[i][k]) >> 12` with `w` fixed integer
mean-value weights computed once from the **rest** anchors. There is no other
position law. Grepping the mana path for proximity/collision/distance terms finds
only the comments declaring their absence. If the bones move, the shape moves;
nothing can respond to nearness because nothing measures it. That is exactly the
right kind of answer to "kneads at a distance".

**The ablation gate is weaker than the code and cannot isolate the mechanism.**
QA confirms the reviewer's structural objection from the source:
`antenna_knead(Rig& g, ...)` writes rotations onto the **bones**, so
`U02_ABLATE_KNEAD=1` moves the antenna too. The A/B therefore cannot separate "the
mana follows the rig" from "the projection moved because the rig moved". The
source comment — *"the mana MUST go limp. If it does not, the coupling is
decorative and the feature has failed"* — promises a test the ablation cannot
deliver, and the reviewer's measured −20.5% mana px / −1.2% spread is not "limp vs
gathered".

**Recommendation:** stop resting the claim on the ablation render. Either delete
that gate or replace it with one that holds the bones fixed and perturbs only the
field. The code is the proof; say so there, and keep the grep as the regression
guard.

## 6. Coverage and the dead knob — CONFIRMED EXACTLY

From QA's own `U02_FOLD_DEBUG=1` renders:

| clip | frames | GATHER | HOLD | KNEAD | RELEASE |
|---|---|---|---|---|---|
| `manafold-hit` | 140 | 62 | 50 | **0** | 28 |
| `manafold-startle` | 160 | 90 | 42 | **0** | 28 |
| `manafold-curious` | 180 | 70 | 64 | **18** | 28 |
| `manafold-taunt2` | 240 | 74 | 76 | 62 | 28 |
| `manafold-hover` | 600 | 214 | 186 | 172 | 28 |
| `manafold-damage` | 464 | 146 | 186 | 104 | 28 |
| `manafold-rest` | 400 | 166 | 120 | 86 | 28 |

`hit` and `startle` **never knead**; `curious` kneads 18 frames of 180. Every span
matches the reviewer's table frame for frame. "Then knead it into new shapes… a
loop of that going on all the time" is not true of the whole bank.

**The dead knob confirmed:** `kKneadClipPm[15]` authors `250` at index 14, the
reader is `slot < 14 ? kKneadClipPm[slot] : 700`, and slot 14 is `build_damage()`.
Index 14 is never read; the damage clip runs at **2.8x** the authored value.
One-character fix; CLAUDE.md law 6 says an unturnable knob is the fault.

## 7. NEW — a declared media file that 404s (the reviewer reported 0 missing)

`website/public/index.html:98`:

    <video src="renders/manafold-inspect.webm"
           poster="renders/manafold-inspect.png" autoplay ...>

`manafold-inspect.webm` exists; **`manafold-inspect.png` does not.** Of 445
`<video>` tags every source is present and 444 of 445 posters are present — this
one is the exception. Damage is low (the video autoplays, so the poster barely
shows) and it is **not** grounds to republish, but gate checklist 18 requires
every declared media file to be present, and the reviewer's "453 declared media
references, 0 missing" is refuted on a broader scan (897 refs including posters
and hrefs).

Everything else on the site checks out: exactly one `<meta name="robots">`,
Manafold first, the mana picker and eye-experiment/gaze rows serving, archive
generations NINE..NINETEEN plus the first-pass archive intact, zero `unnamed02`
current-generation media.

## 8. Zixxtrixx untouched — spot-checked, not repeated

From QA's own pass-3 worktree build and QA's own HEAD build, identical env:

| subject | frames | CRC (both) | byte compare |
|---|---|---|---|
| `zixxtrixx-walk` | 160 | `0x81155EDB` | **all identical** |
| `zixxtrixx-idle` | 576 | `0x118660EF` | **all identical** |

**736 frames across 2 subjects verified by QA.** The CRCs differ from the
reviewer's because it also set `ZIXX_LIGHT`; both sides of QA's comparison share
one env, which is what the identity claim requires.
**Inherited:** `zixxtrixx-damage`, the other 19 sequence CRCs, and the
`ZIXX_SUNS=off` gate-off byte-identity.

---

## Ranked — what the next pass must fix

1. **The shapes do not name at native.** The pass's headline feature reads as a
   cyan puff at the shipping camera. The cause is confirmed as the brush-size /
   shape-size collision, not the cloud. **This needs an owner decision** between
   "bigger particles" and "nameable shapes", not another tuning pass. Everything
   else on this list is smaller.
2. **The mana covers the antenna on `taunt2` (worst), `rest` (moderate),
   `damage`.** A cloud sitting on the hand refutes "shaped across a gap" more
   directly than any legibility caveat. `pirouette` is milder than reported and
   can wait.
3. **`hit` and `startle` never knead; `curious` kneads 18 frames of 180.** The
   second half of the owner's sentence does not happen on part of the bank.
4. **Commit `manafold_page.h` AND add the missing-page hard error**, and fix the
   two false comments plus the stale `.gitignore:91` in the same edit. Zero damage
   to what shipped; the repository currently cannot build its own creature,
   silently. Cheap, bounded, and it protects every future creature.
5. **Eye protrusion — re-baseline the gate, do not chase the number.** Raise the
   **lens** stand-off (`kEyeDeepMm`/`kEyeXMm`) and re-baseline the probe against
   lens geometry. Do **not** re-lengthen `kPupilStarArmShortMm`: that is what the
   old 164 mm was made of, and it re-breaks star containment. Lower than the
   reviewer ranked it, because most of the "regression" is the removal of a fault.
6. **`kKneadClipPm[14]` is dead** — the damage clip runs at 2.8x the authored
   value. One character.
7. **Retire or replace the ablation gate.** It cannot isolate what its comment
   claims it proves. The code already proves the coupling.
8. **The white reads as a crescent, not a ring** around the star (inherited).
9. **The hover loop seam** on the always-playing loop (inherited).
10. **`renders/manafold-inspect.png` is missing** — one broken poster on an
    autoplaying video. Lowest damage on this list.

---

## What is RIGHT and must be protected

* **The fold's construction** — fixed mean-value weights over posed anchors, and
  no proximity term anywhere. QA re-verified this from source. Whatever happens to
  legibility, never "help" the shapes by snapping motes to the antenna.
* **`hasty` f131 and `drift` f179** — QA looked at both and agrees they are the
  best evidence in the pass. `hasty` shows a real *gap* between antenna and
  trailing mass. Protect the DRAG term and the wander motes.
* **Mana on every clip**, with zero per-clip mana authoring, because the coupling
  reads the rig.
* **The X1 teardrop lens** — and note QA's finding that the lens's own protrusion
  did **not** regress (1218 → 1228 pm). It is in better shape than the headline
  number suggested.
* **`kBJunctionF` carrying the old `kBNeck` bind verbatim.**
* **The committed probe** — it printed the protrusion number honestly and
  unprompted. The failure was that nobody read the line, not that it stayed quiet.
  Extend it into a gate; do not trim it.
* **The reviewer's own rigour.** Three of QA's confirmations started from its
  flags, and its refutations-of-self section is the reason QA looked for the same
  trap in its own first reads.

---

## Verified vs inherited

**Verified first-hand by QA (own worktrees, own builds, own renders):**

* The clean-checkout black render, 77 vs 803 colours, same commit, both sides.
* Deterministic byte-identical page regeneration (sha256).
* The blast radius: one `__has_include`, one untracked header, Zixxtrixx tracked
  and hard-guarded.
* The stale `.gitignore` entry and both false comments.
* Pass-3 calibration `1365 pm / 164 mm` reproduced; the probe block proven
  byte-identical across passes.
* HEAD `1275 pm / 123 mm` reproduced; the `kEyeZMm` revert experiment; the
  per-part protrusion breakdown; the four-variant causal sweep isolating
  `kPupilStarArmShortMm`.
* Bright-core component analysis on four HOLD frames, both variants; the native
  and 4x look; the brush/station arithmetic from the constants.
* The `taunt2` contact sheet and the buried-vs-good plate, by looking.
* The fold-phase coverage table for seven clips from own `U02_FOLD_DEBUG` renders.
* The dead-knob reader, its only two references, and slot 14's identity.
* The MVC position law and the absence of proximity terms, from source.
* `antenna_knead` mutating the rig — the ablation's isolation failure.
* Zixxtrixx walk + idle, 736 frames byte-identical, both builds made by QA.
* The full site audit including the missing poster.

**Inherited, not re-derived:** acceptance items 3, 4, 6, 9 and the smear depth
mechanism; `zixxtrixx-damage` and the other 19 CRCs; the gate-off byte-identity;
the ablation's 600-frame pixel deltas; the instrument selftests; the multi-conduit
cost arithmetic; the ladder picks; the live-site deploy verification.

**Could not confirm:** whether the closure rim at 1087 pm is visibly acceptable —
the mana occludes the region under test, so the reviewer's "unfalsifiable look"
objection stands and QA could not resolve it either. A mana-off diagnostic lane
would fix this and is worth the small cost.

## Refutations of QA's own first reads

* QA twice read a **stale binary** as a result: once reading 1247 pm from an exe
  built before a constant was restored, once reading the old probe after a failed
  compile. Both were caught by re-checking that the build actually succeeded
  before trusting output. This is CLAUDE.md's stale-binary law biting inside the
  gate that exists to catch it — every number in this file comes from a build
  whose success was confirmed in the same command.
* QA's first ranking instrument for "the mana buries the antenna" could not detect
  the fault (it scored a good clip as badly as the worst one). Discarded and
  recorded rather than quoted.
* QA initially expected the protrusion loss to be the teardrop lens rebuild. It is
  not — the lens improved slightly. Only the per-part breakdown showed that the
  proudest part had changed identity between passes, which is what made the
  reviewer's single-constant elimination look sound when it was not.

## Background work

Every build and render was foreground and awaited. `Get-Process` (**not** `ps`,
which lies on this machine) shows zero QA processes remaining. The `cmake`,
`ninja` and `g++` processes seen during this session resolve to
`C:\programmieren\zencrifice\zhaozhou` — the hardware agent's lane, which this
gate is forbidden to touch. **Left alone, deliberately.**

QA's worktrees `C:\zqaclean` and `C:\zqap3` were removed with
`git worktree remove`; build trees `C:\zqacleanbuild`, `C:\zqap3build` and the
render outputs are scratch outside both repos. The scratch probe instrumentation
lived only inside a disposable worktree and was **never committed**.

**Nothing was republished. No authored art value was changed by this gate.** The
deployment is correct — the black-render fault is a repository reproducibility
fault, not a fault in what shipped — so there is nothing that must not stay live.
