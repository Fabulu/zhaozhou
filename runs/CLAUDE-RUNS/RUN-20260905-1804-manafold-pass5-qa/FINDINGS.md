# Manafold pass 5 — QA findings

**Run:** RUN-20260905-1804-manafold-pass5-qa
**Subject:** pass 5 — `zhaozhou` f9bf26bf, `Upheaval` 78140d0
**Baseline:** `cc5ff8d9`, built by me in this run
**Lane:** own clones at `manafold-pass5-qa\{zhaozhou,Upheaval}` from
`github.com/Fabulu/{zhaozhou,untitled-game}`. Hardware lane and reviewer lane untouched.

Every number below came from a build I made and a render I ran. Where I could not
confirm something, it says so.

---

## Verdicts at a glance

| # | Claim | Verdict |
|---|---|---|
| — | Calibration: baseline reproduces `manafold-hover 0x5B44FCF2` | **CONFIRMED** — but "pristine worktree" is impossible as written |
| 1 | Build integrity, four parts | **CONFIRMED**, stronger than claimed |
| 2 | Seam 4.07 to 1.89, ratio 5.62 to 2.39 | **CONFIRMED**, survives honest windowing; instrument has real holes |
| 3 | `U02_FOLD_FREEZE` freezes the anchor input, discriminating | **CONFIRMED (mechanism)** / **REFUTED (the recorded A/B pair)** |
| 4 | Eye gate re-baselined on the lens; per-part numbers real | **CONFIRMED**, with a latent blind spot |
| 5 | Knead 0 to 36 / 0 to 48 / 18 to 51; clips that already fit untouched | **CONFIRMED (the three)** / **REFUTED (the blast radius)** |
| 6 | Zixxtrixx untouched, 1136/1136 | **CONFIRMED**, no caveat |
| 7 | Media and site | **CONFIRMED**; two live items unverifiable from this lane |
| — | Item 1 genuinely parked | **CONFIRMED** |
| NEW | Item 8 invalidated the star-containment derivation | **DEFECT FOUND** |

---

## Calibration — CONFIRMED, but the recipe as written cannot be followed

Gate checklist item 4. Pass 5's TASK_LOG says an "own baseline build of cc5ff8d9
(pristine worktree /c/mf5wt)" reproduced `manafold-hover 0x5B44FCF2`. That worktree
is gone, so I rebuilt.

A genuinely pristine `cc5ff8d9` worktree **cannot** produce that value, and I proved
it by accident:

    $ cd /c/mf5qa/base && git ls-files tools/reel/manafold_page.h
    (blank -- untracked at cc5ff8d9)
    $ ls tools/reel/manafold_page.h
    No such file or directory
    $ ZIXX_EXP=celmain .../zhao-reel-cel.exe /c/mf5qa/r-base manafold-hover
    manafold-hover: 600 frames, 3985 unique colours, sequence_crc32c=0x25909C45

That is the pass-4 bug reproduced independently: at `cc5ff8d9` the page was untracked
and `manafold.h` still carried the `__has_include` guard, so a clean worktree builds
happily and renders the pageless creature — 3,985 sequence colours instead of 11,062.

Regenerating the page from **cc5ff8d9's own generator** gives the real baseline:

    $ cd /c/mf5qa/base && python tools/pack/mkmanafoldpage.py
    $ sha256sum tools/reel/manafold_page.h
    e9d5b296...f8cb        # byte-identical to the page committed at HEAD
    $ ZIXX_EXP=celmain .../basebuild-page/bin/zhao-reel-cel.exe ... manafold-hover
    manafold-hover: 600 frames, 11062 unique colours, sequence_crc32c=0x5B44FCF2
    $ python evidence/seam.py r-base2/manafold-hover
    manafold-hover: seam 4.07  typical 0.72  ratio 5.62

**Every published digit reproduces.** The method was right; the word "pristine" is
wrong, and it is the word a future reader would try to repeat. Say instead:
"cc5ff8d9 with `mkmanafoldpage.py` run first, because the page is untracked there."

Bonus, unclaimed: the page generated at `cc5ff8d9` is sha256-identical to the page
committed at HEAD, so the generator is stable across the whole pass — an independent
corroboration of the determinism behind the `-text` pin.

---

## Claim 1 — build integrity — CONFIRMED (all four parts), stronger than claimed

From a fresh `git clone` of `github.com/Fabulu/zhaozhou` at `f9bf26bf`, nothing copied in:

| part | evidence |
|---|---|
| page committed, 819 KB, `-text` | `git ls-files -s` tracked; `stat` = **819,853 B**; `.gitattributes:92 tools/reel/manafold_page.h -text` |
| `__has_include` guard removed | only two explanatory comments mention it; `manafold.h:40` includes unconditionally |
| `build-direct.sh` checks reel+cel, names generator | both `build_reel` and `build_cel` print `(generate: python tools/pack/mkmanafoldpage.py)` |
| `.gitignore:91` stale line removed | `209ae66a` drops `tools/reel/unnamed02_page.h` |
| two false comments replaced | the "grey fallback" promise and its self-contradiction are gone; replacement is accurate |

**Clean-clone build and render:**

    $ bash tools/reel/build-direct.sh --output /c/mf5qa/head cel
    LD zhao-reel-cel / build-direct: done        BUILD_RC=0
    $ ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross .../zhao-reel-cel.exe ... manafold-hover
    manafold-hover: 600 frames, 9700 unique colours, sequence_crc32c=0x6A6DAEE0

Per-frame unique colours 1064 / 1410 / 1155 / 1082 / 1522 across the clip, with **zero
near-black pixels**. The creature is textured from a clean checkout. The bug is dead.

**The negative test is stronger than pass 5 claimed** — deleting the page fails *three*
build paths:

    cel        -> missing .../manafold_page.h (generate: python tools/pack/mkmanafoldpage.py)   RC=1
    mprobe     -> manafold.h:40:10: fatal error: manafold_page.h: No such file                  RC=1
    mmeshcheck -> manafold.h:40:10: fatal error: manafold_page.h: No such file                  RC=1

`mprobe`/`mmeshcheck`/`meshcheck`/`probe` have **no** script-level check — the unguarded
include is what saves them. Belt and braces both work; worth knowing which is load-bearing
for which target.

Regeneration at HEAD is byte-exact: `git diff` empty, sha256 unchanged.

**Not reproduced: "830 colours."** The figure is in no committed document. I measure
9,700 sequence-unique and 1,064 to 1,522 per frame for `manafold-hover`. The *substance* —
a clean clone renders a textured creature, not a black one — is confirmed; the number
is unsourced and should not be quoted forward.

---

## Claim 2 — the seam instrument — number CONFIRMED, instrument not trustworthy in general

### The headline survives honest windowing

Both endpoints reproduce exactly on my own builds, and the improvement holds under
every windowing I tried. This is not an eighteen-points-that-was-really-1.4:

| window | baseline (cc5ff8d9+page) | head (f9bf26bf) |
|---|---|---|
| seam (last to first) | **4.074** | **1.890** |
| ratio, stride-10 (`seam.py`) | **5.622** | **2.385** |
| ratio, **all 599 pairs** | 5.260 | 2.319 |
| ratio, median step | 5.503 | 2.414 |

`seam.py` printed `seam 4.07 typical 0.72 ratio 5.62` and `seam 1.89 typical 0.79
ratio 2.39` — the published values to the digit. **Claim confirmed.**

### The house class is softer than it sounds, but it is real

`2.13` appears nowhere in `Upheaval/`. I measured rather than inherited:

    $ python evidence/seam.py z-base/zixxtrixx-idle
    zixxtrixx-idle: seam 3.16  typical 1.39  ratio 2.28

The band is **real and reproducible in spirit** — Zixxtrixx idle at 2.28, Manafold hover
at 2.39, just inside it. But the cited **2.13 does not reproduce**, and the norm rests on
a single comparison clip: `zixxtrixx-walk` through the same instrument is **7.36**.
House class is one idle loop, not a house.

One thing I expected to be a finding and is not: Manafold's wrap is the largest single
step in its clip (100th percentile). **So is Zixxtrixx idle's** (3.161 against a largest
real step of 3.126). Being the biggest jump in the loop is house-normal. Manafold is
merely more prominent about it — 1.33x its next-largest step, where it used to be 2.63x.
That is a large genuine improvement and I will not dress it up as a residual fault.

### The instrument, fed known-bad input (`evidence/seam_canfail_fixture.py`)

| fixture | expected | `seam.py` said | exit |
|---|---|---|---|
| perfect loop | ratio about 1 | ratio 1.67 | 0 |
| **broken loop** (never closes) | huge | **ratio 40.00** | 0 |
| dead clip, all frames identical | should complain | `ratio nan` | **0** |
| static body, huge seam | huge | `ratio inf` | **0** |
| **motion period exactly 10 frames** | huge | **ratio 1.00** | 0 |

It **does** respond to the fault it tests (ratio 40 on a broken loop), so it is not an
`inkmask.py`. But:

1. **It is a meter, not a gate.** No threshold, always exit 0. Nothing can fail.
2. **Degenerate input prints `nan`/`inf` and returns success.** Checklist item 6 is
   explicit that a tool finding zero of the thing it counts must say so loudly.
3. **The stride-10 denominator aliases against clip periodicity.** A clip whose motion
   period is 10 frames reports **ratio 1.00 on a seam of 41.67**. And pass 5's own fix
   was quantising the orbit/wander frequencies to whole cycles per clip — it changes
   exactly the periodicity the denominator samples against. The denominator is not
   independent of the intervention.

That last one **did not bite here** — all-pairs and stride-10 agree within 3% on both
builds, which is why the claim stands. But the instrument is one clip-length change away
from lying, and it is now the committed measure for this property.

---

## Claim 3 — `U02_FOLD_FREEZE` — mechanism CONFIRMED, the recorded A/B pair REFUTED

**The mechanism is exactly what is claimed, and it is precise, not coarse.**
`manafold_fx.h:692` substitutes `A.body[k] + fxu(kFoldAnchorRestMm[i][k])` for the six
posed anchors — the field's anchor input only, body-relative, reusing the rest hexagon
that already existed for the stencil UV basis. Bones, mesh and camera keep animating. It
isolates what the retired `U02_ABLATE_KNEAD` could not.

**It can fail.** My own A/B, same binary, same env:

    live    ZIXX_EXP=celmain                     -> sequence_crc32c=0x30CFD6C7
    frozen  ZIXX_EXP=celmain U02_FOLD_FREEZE=1   -> sequence_crc32c=0xD03110F6

**But the pair in the run log is not a same-build pair.** The frozen half reproduces
exactly. The live half, `0x59ADB544`, reproduces nowhere I could find:

| build | env | live `manafold-taunt2` |
|---|---|---|
| HEAD f9bf26bf | `ZIXX_EXP=celmain` | `0x30CFD6C7` |
| HEAD f9bf26bf | `celmain` + `ZIXX_LIGHT=diagonal-cool-cross` | `0x30CFD6C7` |
| HEAD f9bf26bf | none | `0xF45CD105` |
| e4a4b89e (the freeze commit) | `celmain` | `0xE5E4035C` |
| **claimed** | — | **`0x59ADB544`** |

For completeness, at `e4a4b89e` the frozen render is `0xA6F7A986` — so the gate has been
discriminating throughout, but neither half of the recorded pair comes from that commit
either. The frozen half is a HEAD measurement; the live half belongs to some intermediate
working tree that is not any commit I tested.

**The conclusion is right and I independently confirmed it; the evidence as recorded is a
before/after pair from two different builds** — the mismatched-comparison law wearing a new
costume. Re-record both halves from one build, and name the commit beside them.

---

## Claim 4 — the eye gate — CONFIRMED, with a latent blind spot

The probe reproduces all three per-part numbers exactly on my clean-clone build:

    u02-probe: eye crown [purple lens] ellip 1228 pm — stands 102 mm proud of the body
    u02-probe: eye crown [cyan star]   ellip 1269 pm — stands 121 mm proud of the body
    u02-probe: eye crown [white ring]  ellip 1302 pm — stands 135 mm proud of the body
    u02-probe: eye-protrusion gate (LENS >= 1215 pm): OK

`kPupilStarArmShortMm` is untouched at 88 — it does not appear in the `cc5ff8d9..f9bf26bf`
constant diff. Reporting per named part is the right answer to checklist 16, and the gated
part **fails safe**: if no vertex matches the lens material, `max_e` stays 0 and
`0 >= 1215` is false.

**The blind spot.** Parts are matched by exact RGB. Lens and star use named constants
(`kLensR/G/B`, `kStarR/G/B`). The white ring uses a hardcoded `{246, 242, 250}` in the
probe, duplicating an equally hardcoded `p.r = 246;` at `manafold_model.h:308`. Neither is
a named constant, and **the probe never prints how many vertices it matched**. Change
either literal and the ring silently reports 0 pm with no complaint — the `inkmask.py`
pattern, reintroduced in the pass that cites `inkmask.py` as its motivation. Not
load-bearing today (the ring is reported, not gated), but one edit from being wrong and quiet.

---

## NEW DEFECT — item 8 invalidated the star-containment derivation, and nothing guards it

The most consequential thing I found, and it is in no claim.

`manafold_art.h:285-291` carries pass 4's containment derivation, which exists because
Direction 3 section 2 ordered that the star plus its white ring must never cross the lens
ink at any authored gaze extreme:

> the short arm (88) + the white ring tube (**15**) must stay inside the lens half-width
> (125): allowed z travel = 125 - 88 - 15 = 22 mm -> sin = 22/88 = 0.25 -> ~14.5 deg full
> angle = **2640** angle16. Held under it with margin.

`kGazeMaxA16 = 2400` was chosen to sit below 2640.

**Pass 5 changed `kWhiteRingTubeMm` from 15 to 22 and did not re-derive.** Re-running the
comment's own arithmetic — my recomputation reproduces its 2640 for tube 15, which validates
the reading:

    tube  15 mm -> allowed z  22 mm, sin 0.250, full angle 14.48 deg, = 2636 angle16
    tube  22 mm -> allowed z  15 mm, sin 0.170, full angle  9.81 deg, = 1787 angle16
    shipped kGazeMaxA16 = 2400

The shipped clamp is now **34% above** the limit its own stated rule produces.

Three things make this worse than an arithmetic slip:

1. **The comment is now factually false** — it still reads "the white ring tube (15)".
   Pass 5 fixed two false comments and created a third, in the load-bearing derivation.
2. **There is no gate.** I read the probe's complete output: clearance, closure rim, eye
   protrusion, travel, surface crossing, rest anchors. **No star-in-lens containment check
   exists.** Pass 4's own comment says containment is ARITHMETIC then PROVEN by rendering
   the authored extremes — a hand derivation plus a look. Nothing could have caught this.
3. The probe *does* now report the white ring standing **prouder than the lens**
   (1302 pm vs 1228 pm). The number that should have prompted the question was printed and
   read past.

I have **not** confirmed a visible artefact — that belongs to the by-eye reviewer, and the
ring being prouder of the dome is deliberate (`kWhiteRingOffXMm` 52 to 60, authored). What I
confirm is that an owner-ordered property lost its margin, silently, and has no instrument.

---

## Claim 5 — knead coverage — the three numbers CONFIRMED, the blast radius REFUTED

I committed `evidence/kneadcount.cpp`, which calls `fold_phase` directly for **every** slot
rather than rendering three clips.

                             baseline(cc5ff8d9)   head(f9bf26bf)
    slot  3 curious    90k          18                 51     <- claimed
    slot  4 startle    80k           0                 48     <- claimed
    slot 10 hit        70k           0                 36     <- claimed
    slot  1 drift     150k          78                 86     <- NOT reported
    slot  6 pirouette 120k          36                 76     <- NOT reported  (+111%)
    slot  8 hasty     120k          44                 67     <- NOT reported  (+52%)
    slot 12 taunt2    120k          62                 76     <- NOT reported  (+23%)
    slots 0,2,5,7,9,11,13,14                     unchanged

**All three headline numbers reproduce exactly.** The fix works and the guard is correct:
`if (n == 0 && release_at > 3*16 && gather+hold+knead > release_at)` touches only the first
cycle of clips that overflow, and slot 7 (the 2-key still) is skipped rather than crashing.

But "clips whose first cycle already fit are untouched" is defensible only on its literal
wording. **Seven of fifteen clips were retimed; pass 5 reported three.** Four clips had their
fold schedule moved with no measurement and no look — `pirouette` more than doubled its knead
duration.

The sharp edge: **`hasty` is the clip pass 5 used as its control for item 2** (hasty f131
lagging-gap read INTACT, looked at against the pass-4 shipped frame). Its knead duration
moved 44 to 67 in the same pass. That comparison has two variables in it, so it is not the
clean control it is presented as.

---

## Claim 6 — Zixxtrixx untouched — CONFIRMED

From a baseline I built (`cc5ff8d9` + regenerated page) against my clean-clone HEAD build,
`ZIXX_EXP=celmain` on both:

| clip | baseline | head | published |
|---|---|---|---|
| walk | `0x81155EDB` | `0x81155EDB` | `0x81155EDB` |
| idle | `0x118660EF` | `0x118660EF` | `0x118660EF` |
| damage | `0x1EA126EE` | `0x1EA126EE` | `0x1EA126EE` |

Frame-by-frame sha256 over all three sequences:

    frames compared: 1139
    sha256-identical: 1139/1139
    differing: NONE

1,136 `.rgb` frames plus 3 `meta.txt` — pass 5's 1136/1136 is the frame count exactly.
**Fully confirmed, no caveat.** This is the cleanest claim in the pass.

For the record: these CRCs need `ZIXX_EXP=celmain` **without** `ZIXX_LIGHT`. Adding
`ZIXX_LIGHT=diagonal-cool-cross` gives a different, equally stable set —
`0xF06EF66B / 0x1408F885 / 0x6C224D56`. The run log records the env; keep doing that.

---

## Claim 7 — media and site — CONFIRMED, two live items unverifiable from here

* **22 subjects.** `78140d0` touches 45 files = 44 render files over exactly 22 unique
  subject names + 1 html line.
* **Inspect assets exist and are real.** Poster **35,918 B**, webm **2,258,752 B** — both
  exact — and the webm decodes: `ffprobe` reports `vp9, 384x240, duration=10.000000`. Not
  another 0-byte file.
* **The 0-byte failure mode is not systemic.** I ran `ffprobe` over **every** webm in the
  site: `checked=446 bad=0` — all decodable, none under half a second. No file under 1 KB
  anywhere in the 905 files of `public/renders`.
* **Exactly one robots tag, and it is right:** `<meta name="robots" content="noindex, nofollow">`.
* **Archives referenced and present:** `archive-2026-08-28-idle.{webm,png}`,
  `archive-2026-09-04-u02-trio.{webm,png}`.
* **Ordering, live:** fetched `upheaval.pages.dev` — Manafold first, Zixxtrixx second.

**Count nit:** `mediacheck.py` reports `901 refs (897 unique)`. Pass 5 quoted 897 declared
media refs; that is the unique count, not the declared count.

**UNVERIFIED from this lane:** the live HTTP 200s and byte sizes, and the live `noindex`.
The shell has no outbound network (`curl` exit 43), and the only fetcher available converts
to markdown, which strips the meta tags — so its "no robots tag" is a conversion artefact and
**not** evidence of absence. The deployed source is correct; I could not re-check the wire.

### `mediacheck.py` cannot fail on the fault it was written for

Load-bearing — it is the cited proof for checklist item 19. Given a fixture with a **0-byte**
`.webm` referenced from `index.html` and a robots tag saying `content="index,follow"`:

    declared render refs: 2 (2 unique); missing: 0
    robots meta tags: 1
    RC=0   <-- PASSED

`os.path.exists` is True for a 0-byte file, and the robots check counts tags without ever
reading `content`. **Pass 4's exact failure — a 0-byte `manafold-inspect.webm` — would sail
through this gate**, and so would a page that had silently become indexable. The live site
happens to be clean (I verified independently with ffprobe, above), so it hid nothing this
time. It still manufactures confidence.

---

## Item 1 — PARKED — CONFIRMED untouched

The complete constant diff `cc5ff8d9..f9bf26bf` in `manafold_art.h` is five items, no more:

    kKneadClipPm[5]   700 -> 500   (rest)
    kKneadClipPm[12]  500 -> 380   (taunt2)
    kWhiteRingOffXMm   52 -> 60
    kWhiteRingTubeMm   15 -> 22
    kDragMaxMm         (new) 380

**No mote size, no stencil scale, no pocket, no camera knob changed.** The single
radius-touching line in the whole diff is `zhao_reel.cpp:3274`, inside `smear_feed`: it
shrinks the *trail feed* footprint using the pre-existing `kMoteCoreOfHaloPm` (580,
unchanged). The drawn mote radius `ms.r_px` is untouched. The parked axis was respected.

---

## Gates that cannot fail

| gate | can it fail? | load-bearing? | did it hide anything? |
|---|---|---|---|
| `mediacheck.py` — 0-byte media | **No** | **Yes** (item-19 proof) | No — site verified clean by ffprobe |
| `mediacheck.py` — robots *content* | **No** | Yes | No — tag is correct |
| `seam.py` — no threshold, always exit 0 | **No** (it is a meter) | Yes (item-9 headline) | No — number reproduces |
| `seam.py` — nan/inf on degenerate input | **No** | Yes | No |
| `seam.py` — stride-10 aliasing | responds, but denominator moves with the fix | Yes | No — agrees with all-pairs here |
| eye probe — zero-vertex match, no count printed | ring: **no**; lens: fails safe | ring no, lens yes | No |
| **star-in-lens containment** | **no gate exists at all** | **Yes** (owner-ordered) | **Yes — the margin is gone** |
| `kKneadClipPm` guard `slot < 15` | hand-written literal beside a `[15]` array | yes | Not yet — will recur |

---

## What is RIGHT — protect these

1. **The build bug is genuinely dead.** A fresh clone from GitHub builds and renders a
   textured creature, and three separate build targets hard-fail if the page goes missing.
   The most important thing in the pass, done properly, with belt *and* braces.
2. **The page generator is deterministic across the whole pass** — the page generated at
   `cc5ff8d9` is sha256-identical to the one committed at HEAD. The `-text` pin does real work.
3. **Zixxtrixx is untouched, provably.** 1136/1136 byte-identical from a baseline I built.
4. **The seam fix is real and survives honest windowing** — 5.26 to 2.32 on all pairs, not
   just on the sampled window. The opposite of the eighteen-points-that-was-1.4 failure.
5. **The eye gate re-baseline is the correct response to checklist 16** — per-named-part
   reporting, gating the stable identity, refusing to chase the number the defect produced.
   The reasoning recorded at the gate site is good and should survive rebuilds.
6. **`U02_FOLD_FREEZE` is a better instrument than what it replaced**, and it freezes
   precisely the anchor input, reusing an existing rest constant rather than inventing one.
7. **Item 1 was left alone properly** — verified at constant level, not taken on trust.
8. **The knead fix is well-guarded** — first cycle only, overflow only, degenerate 2-key
   clip handled without a crash.
9. **The media set is genuinely healthy** — 446/446 webms decode. Pass 4's failure mode is
   repaired in fact, even though the gate that should catch it cannot.

---

## Ranked list for the next pass

1. **Re-derive `kGazeMaxA16` for the 22 mm ring tube, and fix the false comment.** The
   stated rule gives 1787; the shipped value is 2400. Either the clamp comes down, or the
   derivation is wrong and must be rewritten to say what actually constrains the star.
   Owner-ordered property, currently unmargined. *(Highest — it damages a protected read.)*
2. **Give star-in-lens containment an actual gate in `manafold_probe.cpp`.** The only
   owner-ordered eye property with no instrument, and it just broke silently. Walk the star
   and ring vertices against the lens ink at the authored gaze extremes.
3. **Look at the four unreported retimed clips** — `pirouette` (+111%), `hasty` (+52%),
   `taunt2` (+23%), `drift` (+10%). Motion is judged by looking and these were not looked at.
   Redo the `hasty` item-2 control now that its schedule is known to have moved.
4. **Make `mediacheck.py` able to fail.** Non-zero size at minimum; better, `ffprobe` and
   require a decodable stream with a plausible duration. Read the robots `content`, do not
   count tags.
5. **Make `seam.py` a gate, not a meter.** All consecutive pairs (the stride is a false
   economy at these clip lengths); refuse nan/inf loudly; take a threshold and exit
   non-zero. Record the comparison clip beside any house-class claim, and stop quoting
   `2.13` — it does not reproduce; idle measures 2.28.
6. **Re-record the freeze A/B from one build**, naming the commit beside both halves.
7. **Print match counts in the eye probe**, and give the white ring colour a named constant
   shared with `manafold_model.h` instead of two copies of `246,242,250`.
8. **Replace `slot < 15` with `slot < std::size(kKneadClipPm)`.** The dead-knob bug just
   fixed is one new clip away from returning.
9. **Fix the calibration recipe wording** — pristine worktree cannot reproduce
   `0x5B44FCF2`; the page must be regenerated first.
10. **Stop quoting "830 colours."** In no committed document, does not reproduce. Quote the
    sequence CRC and the per-frame count instead.

---

## Separating verified from inherited

**Verified from my own builds and renders:** the calibration CRC and seam; the clean-clone
build, render and colour counts; the three-target delete-page failure; page regeneration
determinism at both commits; all three eye per-part numbers and the gate verdict; knead
counts for all fifteen slots at both commits; all three Zixxtrixx CRCs at both commits and
1139/1139 sha256; the freeze A/B at two commits; the seam instrument five can-fail
fixtures; the two `mediacheck.py` blind spots; inspect asset sizes and decodability; 446/446
webm decode; the 22-subject count; the constant diff for the parked axis; the containment
recomputation.

**Inherited, not verified:** the live HTTP 200s and byte sizes on the wire, and the live
`noindex`; and everything visual — whether the mana still buries the antenna, whether the
white ring reads as a ring, whether the loop reads at native. Those are the by-eye reviewer
lane and nothing here should be read as a verdict on them.

**Could not confirm:** the 830 colours figure; the house idle class 2.13 figure; the live
`manafold-taunt2` CRC `0x59ADB544` under any environment at either HEAD or the freeze commit.

---

## Instruments committed with this run

* `evidence/kneadcount.cpp` — knead/gather/hold/release frame counts per clip slot, straight
  out of `fold_phase`, for every slot. Build with the reel include set:
  `g++ -O2 -std=c++17 -I reference/include -I runtime/include -I tests/render -I compiler/tests/generated -I reference/src -I tools/reel evidence/kneadcount.cpp`
* `evidence/seamhonest.py` — loop seam over **all** consecutive pairs, with the seam
  percentile inside that distribution and the stride-10 figure alongside for comparison.
* `evidence/seam_canfail_fixture.py` — five synthetic clips with known seam behaviour,
  including the period-10 aliasing trap. Any future seam tool should be run against these
  before it is believed.

## Background jobs

All background jobs started by this run were polled to completion and the process table
verified empty of this run children before the run was closed.
