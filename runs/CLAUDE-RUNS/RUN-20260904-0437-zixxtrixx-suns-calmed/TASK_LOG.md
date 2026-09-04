# Task Log: RUN-20260904-0437 - [Describe objective here]

**Created:** 2026-09-04 04:37 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260904-0437-zixxtrixx-suns-calmed/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-04 04:37 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260904-0437
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

## 2026-09-04 04:40 — reading complete; lane state noted

Read OD-30 (binding), OD-29, OD-28, 08-LIGHTING.md, RUN-20260903-2309 TASK_LOG,
zhaozhou/CLAUDE.md. Lanes fetched: zhaozhou at afe9aa9b, Upheaval at f602655.

FOUND: Upheaval working tree carries the previous pass's Direction-29
website/public/index.html regeneration UNCOMMITTED (143 insertions; last
index.html commit is 709c345, Direction 28). The live site serves it but git
does not have it. Plan: commit it as-is first (recovering the prior pass's
site copy), then build Direction 30 on top.

## 2026-09-04 04:55 — COURSE CORRECTION from the owner via coordinator

Owner: "It's one super bright normal sun on everything, no color." The per-clip
colour table did not survive the owner's eye. Working hypothesis (coordinator's,
matching 08-LIGHTING "channel ceiling destroys hue"): the suns push channels
over the 1.0 shade clamp across large areas, clamping hue away -> hue-neutral
floodlight. "Too neon" and "no colour" are ONE fault. Revised acceptance:
1) each clip's sun colour nameable and distinct at native 384x240 (primary);
2) calmer than current, closer to Gen 18, pigment/form dominant;
3) measure per-clip clamping before/after; no sun clamps all three channels
   over a large area. Also consider real falloff instead of attenuation 1
   everywhere (my call, by eye; must not read lit-from-below at apexes).
Plan: build at HEAD, render idle/walk/death current + ZIXX_SUNS=off, measure
saturation with rgbframe.py, look at native, then tune.

## 2026-09-04 05:15 — DIAGNOSIS: measured, and the owner is right

Reused the previous run's intact frame sets (render/v5 = current bank,
render/pristine = pre-suns) with a new committed diag (diag/sunmeter.py, uses
rgbframe.load). Changed-pixel delta between suns-on and suns-off, 6 frames per
subject:
* NOT a literal all-three-channels white-out (all-sat = 0.0% everywhere).
* But the lift is huge: mean dominant delta +60..+85 counts, top-decile
  +120..+165, on EVERY clip, same geometry everywhere -> reads as one very
  bright sun.
* HUE IS DILUTED: idle "golden morning" authored add R:G 1.88:1 measures
  delta 1.1:1 (yellow-white) because the mult gain rides the green-rich
  pigment and pumps G. Balance "teal" measures green-dominant. Death's delta
  IS red but sits on the documented gold-olive physics.
* Per-channel sat over the sun's influence region: R up to 48% (idle f460),
  B 26-37% on cool clips -- large pegged areas per channel, the neon source.
Fix: cut ADDs ~2.5x (the amplitude lever), cut mults ~2x (they flatten form
into the quant_shade ceiling and dilute hue with the pigment's own green),
keep complements near zero for nameable hue. Keep sun geometry (attenuation 1
across a 3 m animal IS what a sun does; lambert shapes the form; the flat-wash
read comes from amplitude, not from missing falloff) -- decision logged.

## 2026-09-04 05:50 — v1 constants authored; Direction 30 rewiring in place

zhao_reel.cpp: sun table calmed (adds ~40%, mults ~50% of Direction 29, hue
purified, complements near zero; positions/radii untouched); additive is
NORMAL (g_zixx_additive_normal, default true, reel-scoped; library gate
zc::g_creature_additive_light stays default-OFF for the silicon revert);
subject_zixx_moving_light_additive REMOVED -- the moving-light subject now
renders with the additive term and the red source; ZIXX_SUNS=off is the ONE
revert switch (suns off + additive-normal off = the pre-suns bank).
Calibration: fresh build reproduced v5 death CRC 0xCDE34FD5 exactly before
any edit. v1 subset rendering (idle/walk/death/attack/balance/salto-nine).

Upheaval: recovered the previous pass's uncommitted generated index.html
(b37b546, pushed). Generation Nineteen archive files staged: 46 copies
(23 webm + 23 png, includes the retiring moving-light-additive entry --
Gen 18 did not archive it, and Direction 30 removes it from the live page,
so the archive is its only surviving home).

## 2026-09-04 06:55 — v2 accepted by eye; code pushed; site prep pushed

v1 (adds 40%, mults 50%): measured better but idle still yellow-white
(delta R:G 1.2) and the stripe still glowing -- closer to v5 than to 18.
v2 (adds ~24%, mults ~30% of D29, secondaries suppressed; balance trimmed
less as the subtlest clip): triptychs (gen18/new/v5) judged by eye at native
-- idle warm gold cast over intact green, walk nameable blue day, death a
warm red-rose mood over the pigment, attack's coil green again with crimson
rim, salto-nine's wheel green with hot-pink inner. All between 18 and v5,
closer to 18. new-sat (newly pegged) drops to 3-20% of changed px (was up
to 48% of a larger area). No all-three-channel pegging anywhere.
Probe: PASS (committed zixx-probe, all gates). Ink-mask (committed
inkmask.py, ink 26/24/22): identical on rendered subjects.
zhaozhou d4127687 pushed. Upheaval b37b546 (recovered page), 8749a5a
(gen19 archive + one moving-light clip + calmed notes + 08-LIGHTING D30).
Gate-off render: 7 subjects already CRC-equal to the pristine table incl
moving-light 0x65A8D1E5. Full table when render completes.

## 2026-09-04 05:20 — full bank rendered, verified, measured; encoding

(Note: two earlier entries carry drifted clock times; order is correct.)
All 22 subjects rendered with the v2 table; frame counts match the published
bank exactly. Ink-mask identity: 22/22 subjects, every frame -> the animation
is untouched (evidence-motion-untouched.md). Gate-off CRC identity: 22/22
IDENTICAL vs the pristine table incl moving-light 0x65A8D1E5
(evidence-gateoff-identity.txt). Final measurements recorded
(evidence-sunmeter-final.txt): dominant mean delta ~+20..+35 (was +60..+85),
max all-three-white fraction 0.2% of changed px (eye highlights), per-channel
new-pegging confined to the stripe under warm suns (~1% of frame, reads as a
hot stripe highlight, judged by eye). Whole-bank pair montages
(look/bank-pairs-*.png) reviewed by eye: every mood visible, distinct and
calm; moving-light carries the red-on-green additive read.
Site: creatures.json Direction 30 applied (one moving-light entry, 21 calmed
sun notes, gen19 with 23 items), CSS families + MAX_ARCHIVE_GENERATIONS ->
19 together, retired live additive files removed after byte-identity check
vs their gen19 archive copies, page reassembled: exactly one noindex meta.
scratch-reel mirrored via robocopy /MIR + first/middle/last+meta byte audit
(clean). tovideo.py encoding all 22.

## 2026-09-04 05:45 — PUBLISHED and verified; run CLOSED

Encoded all 22 clips (tovideo.py), decode-verified every webm to its exact
frame count, every poster fresh, and no live media byte-identical to its
gen19 archive copy (the stale-frame trap inverted into a gate). Remote mains
merged (both already current, no force anywhere). Final probe rerun: PASS.

Deployed ONCE: deploy.ps1 -Project upheaval -Branch main (production, 46
files uploaded). Verified live at https://upheaval.pages.dev: HTTP 200
(280640 bytes); EXACTLY ONE robots-noindex meta; archive tab carries
Generation Nineteen (46 media refs, 23 clips incl the retired additive
entry) beside intact Generation Eighteen; ONE moving-light entry, zero
live references to the retired additive webm; six served files SHA256-match
local byte-for-byte (new moving-light/idle/death webms, salto-nine poster,
the gen19 additive archive, a gen18 archive file).

Final SHAs, both pushed HEAD:main, fast-forward, no force:
  zhaozhou  ec2808e1 (+ this close commit)
  Upheaval  a49f0f4

Background tasks: all render/encode/wait tasks completed with exit 0; final
sweep verified no zhao-reel/ffmpeg/zixx-probe processes remain.

Status: COMPLETE.
