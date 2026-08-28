# FINDINGS — RUN-20260828-0227 zixxtrixx v3: three likeness faults, surgical

## Delivered (all three faults, authored by eye, judged on renders)

### FAULT 1 — the dorsal pink no longer dominates
PINK_HALF_TILE is now a NAMED KNOB in mkcreaturepage.py, and it is 9
(was 13), picked off a rendered ladder (8/9/10/11) shot from the SITE
cameras — walk fixed 3/4 and idle orbit frames 40/200/400 — never from a
UV fraction. Evidence: pink-ladder-8-9-10-11.png, pink-zoom-8-vs-9.png.
At 9 the animal reads GREEN with a pink back and the owner's instruction
still stands: the pink covers the entire top INCLUDING the shoulder
drop-off you see looking down at it. 8 under-delivers that ask (the near
shoulder goes green on the walk's grounded run); 10-11 fight green for
the animal again. The head crown and the atlas painter derive from the
same knob, so the band is one width everywhere.

### FAULT 2 — the head is a head again
* (a) The brim is gone. kEyeBulgeNum 42 -> 16, and the swell localises
  to stations 4..7 (was 3..8) so it is two swellings at the eyes, not a
  continuous lateral ridge. Picked off a head-on ladder (12/16/20/24):
  the brim was gone at every rung; 16 keeps a modest local googly swell.
  Evidence: bulge-ladder-12-16-20-24.png, bulge-headon-zoom.png.
* (b) The chinstrap mechanism was FOUND and removed. The discs had been
  raised so far toward the back line that their orange rings met across
  the crown's pink gap — one yellow band wrapping the face. The painted
  discs shrink 17x33 -> 12x24 tile texels and sit at the side lines +5
  (picked over +3 on the head-on still, eye-height-lo-vs-hi.png): two
  separate ovals INSIDE the silhouette, blue face between them, touching
  the crown line the way Front.png tilts them.
* (c) The ratio is fixed by the EYE, not the head. With the smaller disc
  the profile eye is a feature inside the head (~40% of head height, the
  sheet's read). The owner's culminating taper was NOT touched — no
  graft, no junction.

### FAULT 3 — the pupil is bold
Root cause was ORDER, not amplitude: eye_patch shrank the crop to ~20
texels and classified afterwards, so the slit's red-vs-yellow contrast
was blurred away before the classifier saw it — only the mid swell
survived (the soft blob), and the drawn ink ring failed the luminance
gate outright. Now it classifies at the SCAN'S NATIVE resolution,
dilates the slit mask there (PUPIL_BOLD = 0.05 of crop height, ~1.5x the
drawn width), saturates toward intent, and shrinks LAST; the alpha
ellipse reaches the rim (1.02 -> 1.08) so the slit crosses the disc
rim-to-rim as drawn. At the site cameras the pupil is a distinct red
slit on the idle/look posters and a clear red presence on the walk.
Evidence: pupil2-zoom.png, pupil-gameplay-distance.png,
look-poster-after.png.

### DISCOVERY — the converter was never deterministic
hash(mat) in quilt_field is salted per Python process: every regen
re-rolled the T5 stroke quilt, so the committed page bytes were
unreproducible from the day they were generated ("fixed seed" was a lie
by one stdlib call). Fixed with zlib.crc32; two consecutive regens now
cmp byte-identical (proven in-run). The quilt re-rolled ONCE with the
fix; the grain LAW (amps, scales, scan sources) is untouched and the
grain read was re-verified by eye at head-zoom and walk distance
(verify-idle-head-zoom-AFTER.png).

## Not disturbed (verified, not assumed)
* 17/17 golden artifacts bit-identical (16 clips + pose CRCs) after all
  changes — no clip byte moved, no re-pin needed.
* zixx-probe exit 0, zixx-choreo exit 0, zixx-planner exit 0,
  zhao-reel --check "all sequence CRCs match" (redirected to
  final-*.txt in the run folder). Baseline captures of the same gates
  are committed alongside.
* The crayon converter's T5 grain law, the head attitude (-6000), the
  head-aim rig, the planner, the phase clips, the atlas structure, and
  archive-2026-08-27-* — all untouched.

## Honest remainders
* The death clip still reads pink-forward. Its authored flank keel rolls
  the corpse so the BACK faces the site camera — the pose, not the band
  (before/after in death-before-after-zoom.png; both are pink-dominant,
  after is slimmer). Changing the roll direction would move clip bytes,
  which this run was forbidden to do. One-knob candidate for the owner:
  keel the other way, or bias the camera.
* Head-on the crown reads slightly flat-topped (a pillbox hint) — that
  is the culminating taper's dome geometry, which the owner approved and
  this run did not touch.
* At walk distance the pupil is a red presence, not the full wavy
  shape — the disc is ~6 px there; the shape reads at idle/look poster
  distance and at zoom. Physics of 240p, declared.
* The first commit of this work dragged 830 MB of scratch frames into
  history and pushed; rewritten minutes later (d7c9293 -> 58ca0ee,
  force-with-lease, nothing was on top) and the run folder now ignores
  scratch/, obj/ and golden dumps.
* Site renders are ON DISK and committed (Upheaval ae1fd2c) but NOT
  deployed — the owner verifies and publishes this pass himself (his
  explicit instruction for this run overrides the bestiary's standing
  publish authorisation).
