# Task Log: RUN-20260906-0705 - [Describe objective here]

**Created:** 2026-09-06 07:05 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0705-manafold-pass8-impl/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 07:05 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0705
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

## Pass 8 log

### Lane
`C:\programmieren\zencrifice\manafold-p8-impl\{zhaozhou,Upheaval}`, cloned fresh
from `origin/main`, branch `manafold-pass8` in both. `origin/manafold-p7-review`
merged into the zhaozhou branch first: its three instruments (`webm2rgb.py`,
`eyesheet.py`, the `plates.py` crop mode) and `PASS-7-REVIEW.md` had never
landed on main, and the review is a `Read first` input.

### Item 1 -- the antenna. DONE (constants), by eye + a new committed probe.
The pass-7 review's read ("a uniform strap with mitred corners, no taper, no
knuckles") and the shipped source (five named `kKnuckleSwell*` constants that
arithmetic said nearly doubled the band) could not both be true. Wrote
`tools/reel/manafold_bandprobe.cpp` -- reads the COMPILED MESH, groups vertices
by exact bind y, prints halfX/halfZ per ring; `--selftest` plants a bulge and
also proves a flat stack reports flat.

**Both were true.** halfX ran 63..123 mm (ratio 195%) in pass 7. The knuckles
were there. The fault was that a 170 mm half-width swell on stations 340 mm
apart fills the whole gap -- the band never gets to BE a band -- and every
widening lands exactly on a fold station, where it reads as a fat mitre.

Fixed by making the knuckles LOCAL and the run between them THIN:
`kKnuckleSwellHalfMm` 170->120, `kLoopRings` 48->64 (so a narrower knuckle and
a wider fold blend both still round), `blend` 145->165 (the mitre itself; 168 is
the hard ceiling = half the shortest station spacing), the blade taper thinned
~15%, the swell profile u^2 -> smoothstep u^2(3-2u) (84% of full height at half
width instead of 56%, still zero-slope at the rim so pass 6's no-crease property
survives). New profile: halfX 50..117, ratio 234%.

Sheet ratio used as a BIAS check only, like for like (both side projection, both
as a fraction of body width): sheet band ~7.8% / knuckle ~15.5%; pass 7 shipped
14% / 24%. Deliberately not taken all the way to 7.8% -- that is under 4 px on
the house zoom.

Gates after: meshcheck CLEAN (28 meshlets, 2448 tris), clearance contract holds,
closure worst rim 1053 pm vs gate 1120, slot-13 declared contact -28 mm
(declared -25, band -60..-5), 5d gate A 22 mm, gate B 801 pm.
