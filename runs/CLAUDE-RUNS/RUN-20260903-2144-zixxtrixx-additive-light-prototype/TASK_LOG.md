# Task Log: RUN-20260903-2144 - [Describe objective here]

**Created:** 2026-09-03 21:44 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-2144-zixxtrixx-additive-light-prototype/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-03 21:44 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-2144
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

## 2026-09-03 21:50 — Direction 28 read; static trace done; design fixed

Objective: prototype a per-channel ADDITIVE point-light term (gate default
OFF, byte-identical when off), render ONE new clip with good settings that
shows RED ON GREEN, publish it as an experimental entry beside the live bank.

Trace: point sources contribute gain*lambert*atten into the per-channel
shade GAIN (creature_sim.cpp PointShade3 -> creature_light -> quant_shade
clamp) which MULTIPLIES the texel in rast.cpp (direct path:
dst = sat_u8((smp.c*mod_c+32768)>>16)). So the additive term must land
AFTER the texel multiply, per channel, scaled by the SAME lambert*atten
response the multiplicative term uses.

Design (per-fragment, mirroring the multiplicative transport exactly):
* CreaturePointLight gains three new fields add_r/g/b (Q16.16 additive
  light colour; 1.0 = +255 counts at lambert=1, atten=1).
* PointShade3 accumulates the additive sum alongside the gains, from the
  SAME per-source lambert*atten response -- form survives by construction.
* Corner blend (smooth/face 819/1024) applied identically.
* ScreenV gains ar/ag/ab lanes; TriMode.add_lanes (default false, every
  existing call byte-identical); raster interpolates the lanes with the
  same one-rounding plane setup and adds (lane*255+32768)>>16 post-texel-
  multiply, pre-sat_u8. Saturation at 255 is the only clamp -- the additive
  bypasses quant_shade's 1.0 gain ceiling, which is the entire point.
* Gate: zc::g_creature_additive_light (default false) set only by the new
  subject zixxtrixx-moving-light-additive (a clone of the published
  moving-light subject differing ONLY in the gate + additive colours).
* Red on green: the orange orbit source carries a strong RED additive
  colour (the owner's earlier 'make one of the orange ones a strong red'
  ask, cancelled only because multiplicative transport could not do it);
  its marker orb retints red in the additive subject so marker and light
  agree. Blue and green sources get modest additive in their own hue;
  the warm lamp's additive stays zero (restraint = shippable settings).

Proof plan: rebuild all objects clean (struct layout change), render the
22 named bank subjects gate-off, CRC against the prior run's
expected-crc.txt (moving-light 0x65A8D1E5). Then tune the additive clip
by eye at native 384x240 with the header-verifying mlrgb.py.

## 2026-09-03 22:40 — implemented; gate-off proven byte-identical (22 subjects)

Renderer: CreaturePointLight gains add_r/g/b (Q16.16 of the 255 pixel scale);
PointShade3 accumulates the additive sum from the SAME per-source
lambert*attenuation response as the gains; corner smooth/face blend applied
identically; ScreenV gains ar/ag/ab lanes; TriMode.add_lanes (default false);
rast.cpp interpolates the lanes with the same one-rounding plane law and adds
(lane*255+32768)>>16 AFTER the texel multiply, before sat_u8. Gate:
zc::g_creature_additive_light, default OFF, set only by the new subject
zixxtrixx-moving-light-additive (scoped save/restore around compose).

BYTE-IDENTITY PROOF (evidence/gate-off-byte-identity.txt): all 21 bank
subjects + moving-light rendered with the additive build gate-off AND with a
pristine build of HEAD in a separate worktree — per-subject CRC32C equal,
frame counts equal. 22/22 IDENTICAL.

CRC bookkeeping finding: NEITHER build reproduces the previous run's logged
bank CRCs (e.g. moving-light 0x33B0876E here vs logged 0x65A8D1E5); the prior
run's build-work binary can no longer testify because the cancelled red
follow-up RELINKED it at 21:36 before the kill (the logged CRCs predate that).
The published webm itself matches this tree's render to encoder noise only
(f400: mean |d| 1.1 counts; 107/111 of the >32-count deltas on hard cel edges
= VP9 ringing), so the tree IS the published bank's source and the identity
that matters — gate-off == pristine HEAD — is proven above. No other subject
is re-encoded by this pass, so the live bank cannot be disturbed either way.

Stale-binary trap hit and dodged: while the two bank renders were running,
the v2 relink FAILED (Windows locks an executing image) and the shell
happily re-ran the v1 binary — the unchanged CRC was the tell. Versioned
binary names (zhao-reel-v2/v3/v4.exe) from then on.

## 2026-09-03 22:50 — tuned by eye at native; v4 ACCEPTED

v1 (add 0.30 red on the orange source): red folded into the multiplicative
amber — reads as WARMING, not red. v2 (0.55): stronger, still amber-gold.
v3 (0.80): GOLD, brighter but not redder — the source's own G 0.70
multiplicative gain pumps green under the pool and eats every add.
The lesson: under an additive model a red lamp must not amplify green.
v4: the third slot becomes a RED lamp in the additive subject only —
kRedGainR/G/B 1.60/0.12/0.03 (mult red still blazes the red-rich eye, crown
and pink stripe), kRedAddR/G/B 0.55/0.02/0 (the emission carries the read on
the green body). Marker orb retinted red so marker and light agree. Blue and
green sources keep published mult gains + gentle true-colour adds (0.20 B /
0.16 G); the warm lamp gets NO additive at all.

By eye at native and 3x: f115 the flank behind the head glows genuine RED
fading around the curve — light, not decal; f310 red band over the S against
the green belly pool, blue head, honest mixing; f400 the clip still breathes
calm green between events; f510 red-rose neck under the passing source.
Form survives — toon bands, dark keel and ink outline all intact, and the
red shades around the body instead of sitting on it. Seam 599->0: 3040
changed px vs the base clip's own 2962 (same family). 600 contiguous frames,
all sizes exact, no stale trailers. Ground contact: the subject shares the
published clip's held pose and animation byte-for-byte; lighting only.

## 2026-09-03 23:15 — PUBLISHED and verified; run CLOSED

Encoded via the site's own tovideo.py: 774,609 B VP9 crf16 4:4:4,
decode-verified 600 frames at 384x240, poster frame 115 (the red crossing
behind the head). creatures.json gains ONE experimental entry directly
beside the live Moving light tab; index.html regenerated by assemble.py
only. Deployed ONCE: deploy.ps1 -Project upheaval -Branch main (production).
Verified live: page 200; the new entry present; exactly one noindex META;
served additive webm SHA256-equal to the local encode; the live
moving-light webm still serves 200 untouched. No other subject re-encoded;
no archive generation touched.

Silicon cost if approved (for the CREATURE.LIGHT contract): per source per
vertex evaluation, the additive adds 3 MACs (add_c * lambert*attenuation
accumulate) beside the existing 3 multiplicative MACs — the response term
(one dot product + one attenuation) is SHARED. Application in this
prototype is per-fragment: 3 extra Gouraud interpolator lanes (the same
hardware class as the colour lanes) plus one saturating 3-channel add after
the texel multiply. A per-face landing would drop the 3 interpolator lanes
at the cost of visible triangle banding in the pools.

Final SHAs: zhaozhou 115d17f3 (pushed HEAD:main), Upheaval 709c345
(pushed HEAD:main). Pristine worktree removed; no background jobs left.
