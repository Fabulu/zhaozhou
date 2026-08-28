# Task Log: RUN-20260828-0227 - zixxtrixx v3: three likeness faults, surgical

**Created:** 2026-08-28 02:27 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260828-0227-zixxtrixx-v3-likeness-surgical/

---

## Objective

Fix the three likeness faults named by run 2339's verification, by eye,
disturbing nothing else: (1) the dorsal pink dominating the animal from the
site cameras; (2) the head reading as a mushroom (brim bulge) and as mostly
eye (band + ratio); (3) the pupil too soft for the sheet's bold wavy slit.

---

## Progress Timeline

### 2026-08-28 02:27 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260828-0227
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

## 2026-08-28 ~02:30-03:30 - the three faults, authored by eye

Baseline first: probe 0 / choreo 0 / planner 0 / --check "all sequence CRCs
match" (redirected to files, committed) / goldens re-dumped and cmp: 17/17
identical. Fresh binaries via the run's build-direct.sh (g++ direct).

DISCOVERY: mkcreaturepage.py was never deterministic. A no-change regen
diffed every data line: `hash(mat)` in quilt_field is salted per Python
process, so the committed page bytes were unreproducible from the day they
were generated. Fixed with zlib.crc32; two consecutive regens now cmp
byte-identical (proven). The T5 quilt re-rolls ONCE with this fix; the
grain LAW (amps, scales, sources) is untouched -- grain read re-verified on
the head zoom at close.

FAULT 1 -- pink: PINK_HALF_TILE is now a named knob. Rendered a ladder
8/9/10/11 from the SITE cameras (walk fixed 3/4, idle orbit frames 40/200/
400): evidence/pink-ladder-8-9-10-11.png + pink-zoom-8-vs-9.png. Picked 9:
the top INCLUDING the shoulder drop-off stays pink (the owner's ask), the
flanks/belly stay green -- the animal reads GREEN with a pink back. 8 lets
the near shoulder go green on the walk's grounded run; 10-11 fight green
for the animal again.

FAULT 2 -- head:
(a) bulge 42 -> 16 and stations 3..8 -> 4..7 (ladder 12/16/20/24 head-on,
    evidence/bulge-ladder + bulge-headon-zoom.png). The brim is gone at
    every rung; 16 keeps a modest local googly swell.
(b) the chinstrap mechanism FOUND: the discs had been raised so far toward
    the back line that their orange rings met across the crown's pink gap.
    Discs shrunk 17x33 -> 12x24 tile texels, moved to side line +5 (picked
    +5 over +3 on the head-on still, evidence/eye-height-lo-vs-hi.png):
    two separate ovals INSIDE the silhouette, blue face between, touching
    the crown line as Front.png tilts them.
(c) ratio: with the smaller disc the profile eye is a feature inside the
    head again (side still, evidence/pupil2-zoom.png). The taper was NOT
    touched -- the owner's culmination stands.

FAULT 3 -- pupil: root cause was ORDER: shrink-then-classify blurred the
slit before the classifier saw it (and deleted the drawn ink ring -- its
mixed texels failed the lum gate). eye_patch now classifies at the scan's
native resolution, dilates the slit mask there (PUPIL_BOLD 0.05), and
shrinks last; the alpha ellipse reaches the rim so the slit crosses the
disc rim-to-rim. Judged on the side still; gameplay-distance check at
close on the re-encoded clips.

MISHAP, repaired: the first commit of this work dragged 830 MB of scratch
.rgb frames into history and pushed. Rewrote the tip commit minutes later
(scratch/, obj/, golden dumps now .gitignored in the run folder) and
force-pushed with lease: d7c9293 -> 58ca0ee. Nothing else was on top.

## 2026-08-28 ~03:45 - run closed
All gates green at close (final-*.txt), 17/17 goldens identical, 8 site
clips re-rendered and encoded at 60 fps (Upheaval ae1fd2c). Evidence
committed. NOT deployed per the run instruction. See FINDINGS.md.

**Status: COMPLETE**

## Main-session verification of run 0227

**Repo integrity first**, because this run force-pushed a rewritten tip to remove
830 MB of scratch frames: `git fetch` + `status -sb` show local and origin in
sync, all four of my earlier run commits (`cee3547`, `01939cb`, `d30c11c`,
`c75dfe4`) still reachable, the parallel FIELD lane's commit sits on top
undisturbed, and `.git` is 56 MB. Nothing was lost. The rewrite was contained to
its own tip and repaired within minutes.

**Gates re-run independently:** goldens re-dumped and cmp'd — **17/17
bit-identical**, so no clip byte moved, which is the correct result for a
surface-and-geometry run. `zixx-probe` exit 0 (28,884 hits within allowances),
`zixx-choreo` exit 0, `zixx-planner` exit 0.

**Looked at it, on the diagnostic cameras and across all eight clips:**
* **Fault 1 FIXED and it is unmistakable.** New 8-poster sheet
  (`evidence/verify-all-8-posters-after.png`) beside the before
  (`RUN-...-2339/evidence/verify-all-8-posters.png`): green is the body colour
  again in seven of eight clips, pink is a band along the back. That is
  `Side.png`'s proportion.
* **Fault 2 FIXED.** Head-on (`evidence/verify-front-headon-after.png` vs the
  2339 run's `verify-front-headon.png`): the lateral BRIM is gone — the head no
  longer reads as a hat — and the yellow CHINSTRAP is gone, now two separate
  patches with blue face between and green showing on the flanks.
* The agent found the chinstrap's real mechanism rather than nudging a number:
  the discs had been pushed so far toward the back line that their orange rings
  MET across the crown gap. That is a proper root cause.

### Standing faults after this run
1. **DEATH still reads pink-forward** — confirmed on the poster sheet, and the
   stated cause is right: the authored keel rolls the animal's back toward the
   camera, so it shows its dorsal. Arguably natural for a death, but it is the
   one clip where pink still dominates. Fixing it means touching clip motion,
   correctly out of scope for a surgical run. One knob, owner's call.
2. Frontal eye presence is better than the chinstrap but still reads as small
   patches at the silhouette edge, where `Front.png` gives large ovals. Note the
   owner ruled `Front.png` "a bit confusing" and `Side.png` authoritative for
   shape, so this is a judgement call rather than a clear miss.
3. Pupil is a red presence at walk distance, not the full wavy slit — 6 px of
   eye at 240p is close to the floor of what 240p can carry.

### The determinism bug is the most valuable find in this run
`hash(mat)` is PROCESS-SALTED in Python, so every texture regeneration re-rolled
the T5 quilt and the committed page bytes were unreproducible. Fixed with
`zlib.crc32`; two regens now cmp identical. This is the same class of fault as
the thrown-away pose probe: **a result that cannot be reproduced is not
evidence**, and it had been sitting under a "committed bytes, fixed seed"
requirement that was silently not being met.

### Published

`zhao-reel --check` came back **"all sequence CRCs match"** across 29 subjects,
no mismatches — which was the gate that actually mattered here: the texture
changed (pink band 13 -> 9, eye discs, bolder pupil), so every creature
subject's rendered pixels changed with it, and the re-pins had to be right or
this fails. They were.

Deployed https://upheaval.pages.dev (build `de9bd0e5`). Eight live clips plus
the Archive tab. Run 0227 closes green.
