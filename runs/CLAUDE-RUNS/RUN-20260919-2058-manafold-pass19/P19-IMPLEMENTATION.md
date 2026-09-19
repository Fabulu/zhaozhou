# Manafold pass 19: implementation

**Date:** 2026-09-19
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-20-2026-09-19.md`
**Diagnosis:** `P19-DIAGNOSIS.md` (and its addendum)
**Worker:** Claude (sole Opus worker, no Qwen)
**Source:** Zhaozhou `manafold-pass19`
- `d172a5aa`: audit instrument and diagnosis
- `2de50e08`: the fix
- `c5a7b82c`: quaternion renormalization

**Final renderer:** `.tmp/p19-final/bin/zhao-reel-cel.exe`, MD5 `287a6b53caf4b546deceaca763d8c025`, SHA-256 `a88cd03690dac625dd0998a8fa88c0803c321f0c82331613d5ccedb340bcb482`. It is a clean direct build from `c5a7b82c` with `tools/reel` clean. All binaries are listed in `P19-RECEIPTS/binaries.txt`.

**Verdict: DONE, ready for the bank/encode packet.** All three owner items are fixed at their cause. The legacy toggles reproduce the version-18 bytes on 7/7 subjects. The gate matrix passes **122/122** (`P19-RECEIPTS/gate-matrix.txt`).

## 1. Cause

**Items 1 and 2 had one cause.** `kBRearSocket`, the End carrier and back ball, has been a Root child since pass 16, and it kept an **identity rest rotation**. Every other chain bone gets its attitude from its pose. So the End rings were laid along Root +Y, straight up, while the return arm arrives travelling down into the body:
- The tube hairpinned 150–170° at ring 55 **on every sample of every clip**.
- Rings 58–62 stood up to 207 mm out of the body as a stub beside the incoming arm. This is v18's open item "End-swell stub".
- The End authorities (a 25° press-wave knead wag, the hinge-play buzz and the swallow beat) swung that stub on their own clock, independently of the arm next to it.

That is the torn notch ("almost ripped off") and the extra spazzy part ("a bone too much"). The v18 root helpers did not create the problem. They turned the same 150° mismatch into an explicit three-ring curl.

**Item 3.** Fold-edge strand layers and bolt strands were pushed with constant screen-pixel radii (backing 14 px, core 2 px). The outline ink, meanwhile, scales with the creature's projected radius. At Drift's 128 px, the figure became a glowing blob.

## 2. Fix

### Rear: the End carrier acts in the arm's frame

`rear_socket_compose()` sets `RearSocket = qd × Authored` (renormalized), where:
- `qd` is the arm arrival frame that `finalize_rear_follow` already solves to reach the socket;
- `Authored` is the unchanged composition of every End authority.

The three staged helpers now carry only `Authored` (at most 16.5° anywhere in the bank) instead of a 150° hairpin. The End rings continue straight into the body along the same line the buried ReturnTip already used. No bone was added or removed, and RearSocket keeps its identity, its body-following centre and its public End beats.

Midpoints inherit the composition through the existing nlerp bake. The renormalization matters: an unnormalized chain product scaled the End rings by about 0.2%, and it blinded mspan G9's trace-based angular metric, which read a steady 0.5°/sample turn as 0° then 2.9°. G9 was the first gate to go red, and it is green again at exactly v18's worst values.

### Rear: a calmer ambient End

`kRearSocketAmbientGainPm` scales only the two **ambient** End oscillators: hinge-play's End station and the knead B2 wag. The authored performance beats (swallow, Lasso, nodule-solo) are untouched. They are the Direction-14 public End reads, and mjointpub's 20 mm End floor still passes (E 54–62 mm).

The first knob scaled the whole End rotation. It was replaced because at 300 it failed that floor. Upstream A/B/C motion was deliberately not damped. The "last antenna part" inherits carrier C's motion, and once the stub was gone that motion read as the antenna's own life, not a rear fault.

### Mana lines

`ManaSplat::line` (appended, default false) is set by `line_push()` on:
- the navy backing, shimmer and white core of the fold strand;
- the pass-13 outline branch;
- `bolt_stamp`.

At draw time (the pre- and post-compose passes), `mana_line_r_px()` scales a line's radius by `projected_radius / kManaLineFullRadiusPx`. The result is rounded, at least 1 px and never wider than legacy, and it uses the cel ink's own operand, `primary_radius_q8`. At or above the full radius the width is exactly legacy. Motes, bodies, glows and the (live-off) history feeds are untouched.

### Toggles

All toggles are strict and parsed before the type is built. A bad value returns RC 2.

| Knob | Values | Shipping | Exact legacy |
|---|---|---|---|
| `ZHAO_U02_REAR_SOCKET_FRAME` | `arm` / `legacy-root` | arm | legacy-root |
| `ZHAO_U02_REAR_SOCKET_FOLLOW_PM` | 0..1000 | 1000 (`kRearSocketArmFollowPm`) | n/a |
| `ZHAO_U02_REAR_AMBIENT_GAIN_PM` | 0..1000 | **400** (`kRearSocketAmbientGainPm`) | ignored under legacy-root |
| `ZHAO_U02_MANA_LINE_SCALE` | `distance` / `legacy` | distance | legacy |
| `ZHAO_U02_MANA_LINE_FULL_PX` | 40..2000 | **360** (`kManaLineFullRadiusPx`) | n/a |

The diagnosis named the last knob `..._REF_PX`. It shipped as `..._FULL_PX`.

## 3. Chosen values and the visual reason

All pictures were looked at in the production presentation. Scratch plates are in `manafold-p16/p19-look/`, with notes in `NOTES.md` written after each look (17 images read).

| Value | Ladder | What was seen |
|---|---|---|
| End frame = arm | v18 vs arm, Inspect f0–35 at 2× and f0/f20 at 3× | v18 shows the notch or bracket in every frame, and the arm frame is continuous in every frame. |
| Follow 1000 | 1000 vs 500 | 500 re-creates a 48° arm/End mismatch in Flight (R1 red). Rejected on structure, before art. |
| Ambient **400** | 1000 / 600 / 400 / 300 on Inspect f212–239 and Pirouette f36–54 (the press-snap keys 22–24), 2–3× | At 1000 a knee flicks at the body entry for 2–4 frames on every knead press. At 600 it still flicked, and at 300 the joint read nearly rigid. 400 keeps a gentle bend, "a bit wiggly", with the snap gone. |
| Line full radius **360** (the ink's own close reference) | legacy / 360 / 250 / 180 on Drift f30–290 at 4× | Legacy and 180 keep the glowing blob, and 250 is still bold. 360 turns the figure into linework of the outline ink's weight: at Drift, backing 14→5 px and core 2→1 px. |

At 360:
- Inspect (≥364 px) is byte-identical to legacy lines by construction.
- Hover, Rest, Taunt III and Trick (about 285 px) thin modestly, in step with the ink's own 3 px rung. On the 4× plate the same white core and character read, and the close-up look is preserved.

**Comparison-side numbers** (`manafold-rear-audit`, shipping vs v18):

| Measure | v18 | Shipping |
|---|---|---|
| Arm↔End rotation, worst | 175° | 16.5° (Hover 12.3°) |
| Rear centreline turn, worst | 171° | 35.2°, at the swell rim during the authored swallow beat (Hover 5.9° mean) |
| End joint step, worst | 6.9°/sample (Pirouette at gain 1000: 5.84) | 2.39°/sample |

## 4. Evidence (committed)

- `P19-REAR-CONNECTION-AB-4X.jpg`: v18 vs p19, 4× crops of Inspect f20/f218, Rest f342, Taunt III f328 and Trick f393. The last three crops landed on the fold lines, which is where the largest difference is at those distances.
- `P19-REAR-CONNECTION-AB-NATIVE.png`: Inspect f20/f218 at native scale, v18 above p19.
- `P19-SHEETS/P19-FINAL-MANAFOLD_{INSPECT,DRIFT,HOVER,REST,TAUNT3,CHANNEL,TRICK}-ALLFRAMES.jpg`: every frame, production ink, JPEG ≤1600 px.
- `P19-RECEIPTS/`:
  - `gate-matrix.txt`, `gatematrix_p19.sh`;
  - `binaries.txt`, `identity-renders.txt`;
  - `rear-gate-normal.txt`;
  - `rear-audit-v18-binary.txt` (v18 source) and `rear-audit-v18-legacy.txt` (final binary, legacy-root).

**Shipping sequence CRCs** (final binary, 7 subjects):

| Subject | CRC |
|---|---|
| Hover | `0x40E1DBF1` |
| Inspect | `0x7E4F8487` |
| Drift | `0xD474E003` |
| Channel | `0x9F8C2486` |
| Rest | `0xD04B3BC6` |
| Trick | `0x12438AAD` |
| Taunt III | `0xE87404ED` |

**Legacy identity** (`REAR_SOCKET_FRAME=legacy-root` + `MANA_LINE_SCALE=legacy`, final binary): Hover `0x24B3FE60`, Inspect `0x95A27283`, Drift `0x69158A83`, Channel `0xD49861D0`, Rest `0xD25EF330`, Trick `0xAB4D78E9` and Taunt III `0x07EACF1D`. That is **7/7 byte-identical to the v18 bank.**

## 5. Gates and control tallies

The v18 matrix is carried forward in full, plus the pass-19 legs:
- **New normal:** `manafold-rear-audit --gate`, GREEN (mask 0x0).
- **New controls, exact-mask attributed:**
  - `--fail-rear-frame`: mask **0x3**, declared. v18 genuinely had both the hairpin (R1) and the fast joint (R2).
  - `--fail-rear-joint` (ambient ×3): mask **0x2** only.
  - `--fail-line-scale` (legacy law): mask **0x4** only.
- **New selectors:** 5/5 return RC 2 (`REAR_SOCKET_FRAME=root`, `FOLLOW_PM=1001`, `AMBIENT_GAIN_PM=abc`, `MANA_LINE_SCALE=wide`, `MANA_LINE_FULL_PX=10`).
- **Tally, 122/122 PASS:**
  - normals 12/12 (the 11 v18 normals plus mrear);
  - rear controls 3/3, each with its exact mask;
  - mspan controls 35/35 (root-authority, terminal-cap, swell, front-flex and the carrier mutes included);
  - msmooth controls 16/16;
  - protected legs 27/27 (Trick support and support-depth included);
  - Wave-F mqa controls 5/5, each firing only its own category;
  - selectors 21/21 RC 2 (v18's 16 plus the 5 new);
  - live-history 3/3 (normal OK, and both the legacy and list-drift controls fire).
- mspan's first run on the unnormalized build was red at G9 (E angular jerk 8.33 vs 6). The renormalization fixed it, and G9 now reports exactly v18's worst values.

## 6. What stays protected

The following did not move, and the legacy bytes above prove it for the untouched paths:
- the Front root, signed spans and root-authority palettes (mspan G1b: 0/0 wrong);
- terminal-cap burial (worst rho 1087 pm vs 1120);
- Trick support and pin (mprobe);
- the Wave-F legs (mqa);
- every effect identity (msmooth traces fx splats, which are unchanged);
- the live-history contract and the site.

There is no RTL change and no fit.

## 7. Open issues (non-blocking)

1. **Swell rim turn during the swallow beat.** The End carrier rotates about the socket point S while the arm-side staged helpers pivot on the arm. So an authored End beat bends the tube at the swell's outer rim: 35° worst centreline turn, in Hover's swallow window. It reads as a ball joint turning. If the owner wants it softer, the lever is the authored swallow End gain (`kSwallowEndJointA16PerMm`, 20). mjointpub's 20 mm End floor then becomes the constraint.
2. **The End ball is now subtle.** With the stub gone, the End "ball" is the outer half of the 400-family swell at the body entry, so it reads as a slight thickening. That is consistent with Direction 19's smaller balls, but worth an owner glance.
3. **Mid-range line thinning.** Hover, Rest, Taunt III and Trick lines thin slightly (projected radius about 285–400 px). Setting `kManaLineFullRadiusPx` to 285 would keep them exact and still thin Drift, at the cost of diverging from the ink's reference.
4. **R2 caps the ambient gain.** Its 4°/sample ceiling means the ambient gain cannot go past about 650 without moving the ceiling consciously.
5. **Not rendered here:** the final 22-subject bank, the encode, the merge and the deploy. Those belong to later packets.
