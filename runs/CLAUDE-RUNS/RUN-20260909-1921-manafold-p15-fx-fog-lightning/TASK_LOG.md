# Task Log: RUN-20260909-1921 - [Describe objective here]

**Created:** 2026-09-09 19:21 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-1921-manafold-p15-fx-fog-lightning/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 19:21 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-1921
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## 2026-09-09 19:21 — LANE-FX opened

**Lane:** `manafold-p15-fx/{zhaozhou,Upheaval}`. Upheaval `4c50975`, zhaozhou `12961f9e`.
**Brief:** OWNER-DIRECTION-11 §2.3 (the fog) and §4 (the lightning). PASS-15-PLAN §2 B.3, §4, §6.
**My files, exclusively:** `manafold_fx.h`, `zhao_reel.cpp` (shell call region + env knobs), `rungsweep.py`.
**Not mine:** `manafold_clips.h`, `manafold_art.h` eye block, `manafold_rig.h`, `manafold_model.h` (LANE-EYE).

### What I read and what it settles

* **The fog has never been fog because it is an ANNULUS THAT ISN'T ONE.** `shell_paint`
  is a screen-space distance-from-silhouette band: 3 px out, 6 px in, peak alpha 440,
  effective ~27% one ring out. At the eye/body intersection (tens of px interior) it is
  **identically zero**. No alpha fixes that. The architect proved it on pixels
  (`B-shell-ladder-3x.png`): 440 = edge anti-aliasing, 900 = a white rim glow.
* **The lightning surround is black by arithmetic**, not by tuning:
  `kManaStormMid={8,12,46}` x `kFoldStrandDarkGainPm=300` = ~(2,4,14). The pass-14
  ladder swept radius and gain and never swept HUE.
* `kFoldStrandOn` (`manafold_fx.h:364`) is **false**. The connected figure exists behind it.

### Plan

1. **FOG:** re-author `shell_paint`'s geometry as a real annulus scaled to the body's
   own apparent radius (uncapped inward distance transform, R = max depth, annulus
   depth a per-mille fraction of R). Density rises inward to a peak at the annulus's
   inner edge, then holds at a core floor -- which is what the column integral through
   a real fog shell actually does, and it is also the profile that makes an eye sinking
   into the body FADE. Ladder `U02_SHELL_REACH` on both backdrops with a too-far rung.
2. **LIGHTNING:** three layers -- white core (keep), **NEW additive blue shimmer**
   (the missing thing), thin per-backdrop navy backing with its RAMP raised off black.
   More motes. Flip `kFoldStrandOn`. Keep the green/false fold separate and intact.
3. Everything through `rungsweep.py`, ONE binary, both backdrops, provenance beside
   each plate.

### Discipline
`--clean` builds always; real exit code, never a pipeline's; `rgbframe.py` only;
contact sheet finds, 2x crop confirms; calibrate on a known-negative; PIDs never
`taskkill /IM`; one build, one renderer, NO encodes.

19:21 baseline `--clean cel` build started (`build-base.log`), output `build-fx/`.

## 19:45 — the code is written; waiting on the clean build

**FOG — `shell_paint` re-authored (manafold_fx.h).** The band is gone; the shell
is now an annulus scaled to the creature's own apparent radius:

1. an **uncapped inward distance transform** of the renderer's cover mask;
2. **R = the deepest interior pixel** = the body's apparent radius THIS FRAME
   (so the fog breathes with the bounce);
3. **one continuous profile from the gas's outer edge inward** -- rise to a peak
   at the annulus's inner boundary, decay to a plateau, hold.

The profile is not a guess. The column integral through a real fog shell is zero
at the silhouette, peaks where the ray goes tangent to the clear core (screen
depth R - R_core, i.e. the annulus's inner edge), then holds at about half that
across the interior. That is ALSO the owner's sentence verbatim, and it is ALSO
what makes a sinking eye fade instead of cut. Three readings, one curve.

Knobs, all env-overridable from one binary: `kShellFogDepthPm` (380),
`kShellOutReachPm` (55), `kShellCoreFloorPm` (340), `kShellRiseGamma` (1600),
`kShellAlphaMaxPm` (440, re-laddered from scratch -- the pass-12 pick was
"the most you can push a fringe" and does not transfer to a volume).

**LIGHTNING (manafold_fx.h + zhao_reel.cpp env).**
* `kFoldStrandOn` **flipped to true** -- D11 calls this the STANDARD, not an
  experiment. Declared loudly; `channel` is protected content and this changes it.
* `kManaStormMid/Hi` **{8,12,46}/{18,26,78} -> {12,20,64}/{26,44,112}** and
  `kFoldStrandDarkGainPm` **300 -> 1000**. The navy is now IN the ramp where the
  opaque+soft blend actually reads it, and the "LOWER IS DARKER" knob nobody
  could read the direction of is gone.
* **NEW `kRampShimmer` + a third draw pass** -- additive blue, depth off, between
  the navy backing (9 px, was 12) and the white core (3 px). Per-stamp per-FRAME
  flicker at +/-380 pm is what makes it shimmer rather than sit.
* **NEW `kRampBoltCore`** and the SHAPE motes join the lightning vocabulary when
  the strand is on; wanderers do not (they are not folded into anything).
* Mote garnish 300 -> **720 pm** while the bolt folds ("we want more of them").
* The green/aqua fold keeps every number it ships with. One rebalance rung
  (`ZHAO_U02_AQUA_BAL=1`, B >= G) as an experiment row, default off.

**GATE.** `manafold_shellgate.cpp` re-aimed: checks 2, 3 and 4 would have passed
TAUTOLOGICALLY on the new mechanism (2 read a pixel the annulus also paints; 3
asserted the opposite of the owner's sentence; 4 compared the ink against a
neighbour that is denser anyway under a rising profile, so kShellOverInkPm=1000
would have passed). New check 6 is the mechanism itself -- the fog must reach
deeper into a bigger body, which a fixed-pixel band cannot do.
**And it had NO build-direct.sh target**, so nobody had run it since it was
written: 10-GATE item 42. Added `mshell`.

**Trap paid for, in the log because it is generic:** a `\n` written into a C
string through a Python heredoc arrived as a REAL NEWLINE and split two string
literals. 09-ENGINE-GOTCHAS s21's family. `BUILD_RC=1` while the harness
reported the task "exit code 0" -- the outer subshell's status. Reading the real
exit code caught it in one look.

## 20:05 — rendered, and LOOKED. Two verdicts, one of them a fault I made.

Binary `690a8c91` (baseline was `dae2fa57` -- the md5 moved, so the header edits
are in). Rendered `hover`/`hit`/`channel` under `ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross`. Plates in `diag/plates/`.

**FOG: it works, first time in five directions.** `fog-hover-BA.png` and
`channel-BA-f250.png`, native 3x crops, P14 beside P15 from two binaries
(labelled as a BEFORE/AFTER pair, not as a rung -- a ladder comes from one
binary and this is not one). The P14 ball is a hard-edged solid with a black
line round it. The P15 ball wears a soft gas layer thicker than any outline,
with a visible inward gradient, and the ink is still solid black. On the violet
night the mood survives. **The shell is finally fog rather than anti-aliasing.**
Open question for the ladder: the interior plateau lifts the body's pigment a
little. That is the D8 §4 knob (`kShellCoreFloorPm`) and the owner picks it.

**LIGHTNING: the shapes are BEING MADE -- and they are a white rope.**
`p15a-hover-sheet.png`, every 12th frame of `hover`: a closed figure is traced
in the antenna window on essentially every frame, and it CHANGES -- a ring at
f036, a wide loop at f120, a triangle at f384, an arrowhead at f420. That is
D11's "they need to make the shapes", and it is the first time this creature
has done it. But the line is a uniform saturated white tube. No blue, no
filament, no bolt.

⚠ **And I can name the cause without a ladder, which means I must PROVE it with
one rather than act on it.** `kFoldStrandPerSeg = 6` subdivides each
sub-segment into 6 stamps -- and pass 14's own note records that a sub-segment
is about THREE MILLIMETRES, which is one to two pixels. So the six stamps land
on the same pixel. **perSeg is not a connectedness knob at this scale; it is a
6x additive overlap knob**, and 09-ENGINE-GOTCHAS' own strand lesson is that
the white is set by OVERLAP and not by gain. The line was already continuous at
1. Next: a perSeg ladder on `channel` from one binary, with a mote-count rung,
so the claim is proved on pixels instead of asserted from a comment.

Also visible on `channel` f250: the navy backing IS reading, as a dark rim
around the white mass. The layer works; it is being swamped.

## 20:25 — the shell's render cost, removed; and where the lightning stands

**PERF, and it is a real regression I created.** The first `shell_paint` cost
about **1.8x the reel's whole render throughput** (~200 frames/min -> ~112,
measured on the wall clock across two renders of the same subjects). Across a
28-clip bank that is half an hour added to every publish wave. Cause: it
allocated and zeroed FOUR frame-sized vectors per frame and materialised every
EXTERIOR pixel as a BFS seed -- ~85,000 of them, each fanning out to eight
neighbours -- when level 1 is just "a cover pixel touching a non-cover pixel or
the frame edge", which is one scan. `depth` now also carries the outward skirt
as NEGATIVE distances, so two of the four buffers stop existing, and it is
reused across frames.

**The optimised version reproduces the gate's profile DIGIT FOR DIGIT**
(`13 33 57 88 121 161 205 254 227 227 197 170 141 141 114 85 85 ...`), which is
the corroboration s16 wants before believing a rewrite.

**LIGHTNING at 5x on the night (`interim2.png`, channel f363):**
* STRAND OFF (the shipped look): a cloud of aqua blobs. No shape. This is the
  owner's complaint, reproduced from my own binary as the control.
* STRAND ON: **a spiral drawn in white filaments with a blue shimmer hugging
  them and a navy backing outside that.** That is D11's sentence, on screen.

**And at 2x/native (`native-check-2x.png`), the honest test (item 9):**
* NIGHT reads. The spiral and its blue survive.
* **DAY does not.** On the pale sunset the blue washes out and the figure is a
  white loop. That is 08-LIGHTING's backdrop law arriving exactly where it said
  it would -- an additive effect cannot win against a bright field, and the two
  clips whose job is the mana are the ones that carry it.

So the next axis is the NAVY: deepen and widen the backing so the shimmer has
something dark to be bright against on the day too. The pass-14 review's guess
was that a better surround may collapse the per-backdrop split; that is now a
testable rung rather than a hope.

**`manafold-hit` is the right ladder subject** and I had been about to use
`hover`: 140 frames instead of 600, it carries the fold figure (spirals at
f010-f025, a triangle at f110), it is on the DAY backdrop where the fault is,
and it is also the eye-clipping clip. Three questions, one 35-second render.

**Fog, by eye on the hit sheet:** the body reads noticeably WASHED across the
clip -- a dusty mauve where P14 is hot pink. `kShellCoreFloorPm` (340) is the
knob and my own eye says it is too high. Into the ladder, with the owner's pick
above mine.

## 20:50 — the perSeg ladder, and a hypothesis it killed

`diag/ladder-white/rungs-f0363.png`, six rungs, ONE binary (md5 690a8c91,
provenance file beside the plate), `channel` f363/f250, shipping env.

| rung | what it draws |
|---|---|
| STRAND OFF (the control) | a cloud of aqua blobs, NO shape -- the owner's complaint, reproduced from my own binary |
| PERSEG6 (as built) | a continuous white spiral, blue shimmer hugging it, navy outside that |
| PERSEG3 | a readable BEADED filament; more jagged, less blown |
| PERSEG1 | **the figure is gone** -- unconnected dots |
| PERSEG1 CORE2 | dots |
| PERSEG1 CORE2 FEWMOTES | dots |

⚠ **I was wrong, and the ladder is the only reason I know.** I had reasoned from
pass 14's own comment (sub-segments ~3 mm, i.e. 1-2 px) that perSeg=6 put six
stamps inside one pixel -- pure additive overlap, no connectedness -- and was
one edit from cutting it to 1. **perSeg IS the connectedness knob and cutting
it would have thrown the feature away.** A comment is not evidence
(09-ENGINE-GOTCHAS §8), and this is the second time this pass that reading has
disagreed with rendering.

**Native check (item 9), 2x not 5x:** the night reads, the DAY does not -- the
blue washes out on the pale sunset and the figure is a white loop. Exactly
08-LIGHTING's backdrop law. So the next axis is the NAVY BACKING: give the
shimmer something dark to be bright against on the day too.

Running now, both in parallel (one build, one renderer -- the load rule):
* `ladder-navy` on **`manafold-hit`** (140 frames, DAY backdrop, carries the
  figure AND the eye clip): core radius x navy depth x navy width x shimmer
  gain, six rungs with a deliberately-too-far one.
* the **baseline binary for `bitident.py`**, built from a `git worktree` at
  zhaozhou `12961f9e` -- the commit before this lane touched anything. The
  first `--clean` deleted my original baseline exe, which is a small lesson of
  its own: `--clean` empties `$BIN`, so a binary you want to keep goes in a
  different `--output` directory, not a different filename.

## 21:10 — the navy ladder, the pick, and the axis it exposed

`diag/ladder-navy/`, six rungs, ONE binary (md5 ce389929), `manafold-hit`
f15/f115, DAY backdrop, shipping env. Plate:
`pass15-fx-plates/D-navy-ladder-hit-f115-4x.png`.

| rung | read |
|---|---|
| core3 / navy9 (as built) | a white blob; the shimmer is buried under it |
| **core2** | the white separates into FILAMENTS; blue appears beside them |
| **core2 / navy14** | **THE PICK.** The figure sits on a visibly darkened ground and the blue reads -- on BOTH backdrops |
| navy14 / gain600 (deeper) | NOT better: whiter, no more blue. Depth was the wrong axis, WIDTH was the right one |
| shimmer gain 900 | nearly identical to 620 -- a clamped additive sum, gain is a no-op, for the third recorded time on this creature |
| TOO FAR (core1 navy22 shim r10 g1300) | a solid white cloud. The ceiling, demonstrated (item 4) |

**Picked and shipped:** `kFoldStrandCoreRPx` 3 -> **2**,
`kFoldStrandDarkRPx` 9 -> **14**, `kFoldStrandDarkGainPm` stays **1000**,
`kFoldStrandPerSeg` stays **6**.

**And the plate changed my mind about a colour.** The shimmer renders CYAN, not
blue: additive over the pink body lifts R to clipping, and a green of 116 lifts
G before B can lead, so the mid-tones land on cyan however blue the constant
looks in an editor. **Hue is the one axis pass 14 never swept and I had just
guessed a value on it.** So it ships as a THREE-VARIANT AXIS
(`ZHAO_U02_SHIMMER_HUE`: 0 cyan-lean control, 1 true blue, 2 violet-blue) with
1 as the by-eye default -- the owner packet's "three blue-shimmer variants x
both backdrops" is now a thing that exists rather than a thing to hand-roll.

Baseline binary for `bitident` built from the `12961f9e` worktree, BUILD_RC=0.

## 21:40 — the fog ladder, and the pick that contradicted my own diagnosis

`pass15-fx-plates/B-fog-ladder-hit-f0028-3x.png` + `-eyezoom-6x.png`. Six rungs,
ONE binary (md5 ec2bd37d), `manafold-hit` f28 -- the frame where the eye sinks
into the body -- and f100. Provenance beside them.

| rung (reach/alpha/floor) | read |
|---|---|
| OFF | hard-edged ball, crisp cel bands, the lens a hard blade with a severed sliver |
| LIGHT 240/320/200 | a gentle haze; the lens still fairly hard |
| AS BUILT 380/440/340 | plainly gaseous; the lens absorbed; the body a touch pale |
| **PICK 520/560/180** | **the most gas at the rim AND the pigment intact** |
| THICK 520/620/450 | heaviest wash; the pink desaturates |
| TOO FAR 900/950/850 | a milky drowned animal, pigment gone, ink softened -- the ceiling |

⚠ **The pick contradicts what my own eye had concluded an hour earlier.** I had
written "the body reads washed, the floor is too high" and the obvious next move
was LESS FOG. The ladder says the obvious move was wrong: **depth and floor are
two things the old band had conflated**, and pulling them apart gives exactly
what the owner described --

    annulus DEEPER (380 -> 520)   more gas where a thing clips in
    floor   LOWER  (340 -> 180)   less veil over the body's clean middle

The rung that raised BOTH is the one that kills the pigment. **"Too much fog"
was never the diagnosis; "fog in the wrong place" was.** Shipped 520/560/180.

Remaining: final confirm render on the shipped values, the shimmer-hue plate,
`bitident` against the `12961f9e` baseline, and the findings doc.

## 22:15 — the hue ladder, and the axis the shell has never had

`pass15-fx-plates/D-shimmer-hue-DAY-hit-f115-4x.png`. Four rungs, ONE binary
(md5 ec2bd37d), `manafold-hit`, the DAY backdrop.

The first rung is **pass 14's own look reconstructed from this binary** -- black
surround, shimmer off -- and it shows the complaint exactly: white lines with a
**hard black outline** round them. Not lightning; rope with ink on it. The
review's "the shipped halo reads BLACK" is now sitting next to its fix.

* HUE0 cyan-lean (the first authored value): electric, but cyan.
* **HUE1 true blue (SHIPPED): white core, blue shimmer, navy beyond.** The most
  "actual lightning bolt" of the four.
* HUE2 violet-blue: a lilac edge; harmonises with `channel`'s violet night, but
  on the sunset it drifts toward the body's own magenta.

**And one more axis added, because a frame made me look.** `wash-check.png`
(`hover` f240, the creature's DARK side, P14 / first build / shipped): the fog
lifts a deep magenta a long way. On bright frames the shift is subtle; in shadow
it is the whole read. **Every argument this project has had about "too much fog"
has been an argument about the AMOUNT** -- `kShellAlphaMaxPm` -- and the amount
is only half of it. `kShellTint` is a pale rose, its distance from the pigment
is what the blend multiplies, and **it has never been swept once.** A gas the
colour of the animal fogs; a gas far from it BLEACHES, at any alpha.

It stays rose (that is the v1 shell D9 §7 said to go and look up) but it is now
a knob with an env override -- `U02_SHELL_TINT=r,g,b` -- so the next person can
ask that question in one render instead of concluding "less fog" for a fifth
time. 09-ENGINE-GOTCHAS §18, pre-empted rather than paid for.

⚠ **The `\n`-through-a-Python-heredoc trap fired for the THIRD time** in this
session, again splitting a C string literal across two lines. The fix that
works is a script that writes `chr(92) + 'n'`, committed in the run's scratch as
`fix_nl.py`. It is 09-ENGINE-GOTCHAS §21's family and it is worth knowing that
it recurs every single time an agent edits C from a heredoc.

## 22:50 — the night hue plate, and two identity proofs

`pass15-fx-plates/D-shimmer-hue-NIGHT-channel-f363-4x.png`. Same four rungs as
the day plate, one binary (md5 53e5b184), the violet night. **The P14 control is
even more damning here**: a white spiral outlined in pure BLACK against a violet
sky -- a cartoon rope. HUE0 reads as electric cyan plasma, HUE1 as blue lightning
with a white core, HUE2 as a lilac bolt sitting inside the sky's own hue. HUE1
and HUE2 are both defensible on the night; HUE1 is what his words point at and
is the shipped default. Both plates exist, so the pick is one constant.

**Identity proof 1 -- the tint knob is inert.** `manafold-hit`, 140 frames,
rendered by P15D (before the knob) and P15E (after): sha256 over every frame's
bytes is `f5d610e0…` from BOTH. So the plates made with P15D remain valid
evidence for the shipped tree, which is the thing a new knob quietly invalidates
if nobody checks.

**Identity proof 2 -- Zixxtrixx.** `bitident.py` running now, self-built baseline
from the `12961f9e` worktree against P15E, 71 enumerated subjects, both metrics.
⚠ **Stated deviation:** `--jobs 2`, not one renderer. 09-ENGINE-GOTCHAS §22
measured the profile worth gating on -- concurrent COMPILERS and ENCODERS -- and
a renderer is 1 thread / 17 MB. Two of them is 2 of 8 cores, and no build or
encode runs alongside. It halves a job that would otherwise outlast the session.

## 23:15 — housekeeping, and the load census before pushing anything

**Purged 3,672 `.rgb` intermediates** from this lane's `diag/`: 1006 MB -> 76 MB
(and the 76 is bitident's own in-flight frames). Every PNG plate and every
`RUNG-PROVENANCE.txt` kept. CLAUDE.md's own law -- `.gitignore` made these
invisible to git and nothing was deleting them; the machine reached zero bytes
free once already.

**Load census (09-ENGINE-GOTCHAS §22, the two-line version) before deciding
whether to push bitident harder:**

    quartus fits    NONE
    ffmpeg          NONE
    cc1plus         1  (another lane's build, not mine)
    renderers       2  (mine, bitident)
    cores 8, free RAM 10.7 GB of 23.8

So the dangerous profile -- concurrent compilers and encoders -- is not present,
and there is headroom. I left bitident at `--jobs 2` anyway: raising it would
have meant killing a run that already has ~20 minutes of completed subjects in
it, next to another lane's compiler, to save my own wall clock. Not a trade I
get to make with someone else's machine.

⚠ **And the process check itself is the discipline, not a formality.** The one
renderer visible at that moment was identified by COMMAND LINE first --
`.../manafold-p15-fx/build-base/bin/zhao-reel-cel.exe ... zixxtrixx-death2-normviz`
-- which is what says it is mine. Every lane runs this executable name.

**Plate generations labelled.** `pass15-fx-plates/README.md` names the binary and
md5 behind each plate and flags the two that are deliberately an older
generation. D11 §1 applied to my own evidence. Writing that table caught a wrong
md5 I had just typed into it (P15D is 9fba1de4, not ec2bd37d) -- the "wrong
number with a reassuring provenance line" failure, caught by writing the number
down next to the thing it names.

## 23:55 — the best proof of the lane, and the control error that nearly hid it

**D11 §4 says KEEP the green fold and keep it SEPARATE.** That is now a hash.

`manafold-hit`, 140 frames, baseline (`12961f9e`) vs the shipped binary with
`ZHAO_U02_STRAND=0 U02_SHELL_ALPHA=0`:

    baseline, shell off                sha256 f99c9722...
    pass 15, shell off, strand off     sha256 f99c9722...

**Byte for byte identical.** Every pixel this lane changes is behind one of two
switches; the green fold is not one of them.

⚠ **The first run of this check said NOT IDENTICAL** (c25fa732 vs f99c9722,
crc 0xCF62ED54 vs 0x7CB31B31) and I nearly went looking for a bug in my own
code. **The fault was my control**: I had zeroed the shell on the pass-15 side
and left the BASELINE's own shell running, so I was comparing a creature with fog
to one without and calling it a fold test. **A comparison that changes two things
measures neither** -- the mismatched-poses lesson in CLAUDE.md, arriving through
an environment variable instead of through a camera.

It also handed the check the **known-negative** item 40 asks for, by accident:
the same method returned DIFFERENT on the unfair comparison and IDENTICAL on the
fair one, so the instrument demonstrably distinguishes the two states rather than
being a hash that always agrees.

`bitident` (Zixxtrixx, 71 subjects, both binaries) still running at ~subject 24.

## 00:20 — lane closed

Both repos in sync with `origin/main`, working trees clean, 23 plates shipped.

**Wave-1 disjointness held exactly.** LANE-EYE landed
`manafold_art.h`, `manafold_clips.h`, `manafold_model.h`, `manafold_rig.h`,
`manafold_eyecam.cpp`, `eyesweep.py`; I landed `manafold_fx.h`,
`zhao_reel.cpp`, `manafold_shellgate.cpp`, `build-direct.sh`. **Intersection:
empty.** One Upheaval push needed a rebase onto their findings; it was clean.

⚠ **One heads-up for the reviewer, not a complaint:** LANE-EYE's
`manafold_eyecam.cpp` does not appear to have a `build-direct.sh` target either
-- which is the item-42 fault I just fixed for `manafold_shellgate.cpp` (a
committed gate nobody could build, so nobody had run it). Their call, but the
two edits to that file will want merging together, and mine is additive: one
function, one case label, one usage line.

**Still running, deliberately, and it is the only thing:** `bitident.py`
(Zixxtrixx identity, 71 subjects x 2 binaries), at subject ~35. One python
driver plus one short-lived renderer per side, ~50 MB across three processes.
The findings carry the log path, the exit-code meaning, the re-run command, what
the answer should be and why, and the kill recipe (find the driver by COMMAND
LINE, stop it by PID) if a fit needs the machine.

**Everything else has exited** and this lane's 3,672 `.rgb` intermediates are
purged. The lane is deletable once the publish wave re-renders the bank --
`git worktree remove` for `base-tree`, not an `rm`.
