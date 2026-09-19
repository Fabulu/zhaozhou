# Manafold Pass 17 — Trick face-identity repair

**Date:** 2026-09-19
**Trigger:** repaired exact-bank batch 07
**Focused verdict:** **PASS — the complete headstand now keeps a readable face**
**Pass verdict:** exact v3 bank and fresh 400-frame Trick review PASS; see `PASS17-FINAL3-TRICK-REVIEW.md`

## Final-bank blocker

Renderer `6FD6147B43056E823F0487F9DFBB40BA` preserved Trick's contact and smooth pure-X half-turn, but exact-bank review correctly rejected the picture. From roughly f0140 through f0295 the planted phrase was a broad rear/side mass. At exact 4x, f0160/f0220/f0260 contained the eye geometry only as edge-on slivers at the far rim; the stars, facial relationship and most antenna identity did not read.

The pure-X selection was therefore a **failed art decision**, not a mechanism failure. `PASS17-FINAL2-BATCH-07.md` is the binding negative evidence.

## Why pure X was not enough

The earlier argument treated the body +X axis as the face. A 180-degree X turn does preserve that nominal axis, but the rendered eyes are not one +X-facing decal: they are two radial plates at +/-28.26 degrees, each already carrying the fixed-camera base rotation before the root transform. The inverted root conjugates that camera-relative basis. At yaw zero both plates project edge-on even though the abstract +X vector is unchanged.

The fix is deliberately separate from the contact half-turn. The root remains pure X (`X=-32768`, `Z=0`) so the loop peak and root-height choreography keep their physical meaning. A named planted **local face yaw** follows the same flip envelope continuously into and out of the headstand. It does not switch at contact, move the camera, alter timing or borrow authority from the contact gate.

## Rendered ladder and selection

One binary rendered all 400 frames for local planted yaw `-12288/-8192/-4096/0/+4096/+8192/+12288`, followed by the bounded `+16384` candidate.

- negative yaw turns the face farther away;
- `0` is the exact rejected repaired-bank output;
- `+4096` and `+8192` progressively restore one then both eyes but leave a weak side read;
- `+12288` is readable but still oblique at the plant arrival;
- **`+16384` (+90 degrees) is selected by eye**: both complete almond lenses and cyan/white stars face the fixed camera from the completed plant through the hold, while two antenna sticks remain visibly load-bearing beneath the body.

The selected complete sheet preserves anticipation, approach, plant, balance and recovery. f0115 is naturally mid-turn; f0140 already shows two complete eye forms, f0156/f0160 settle into the readable planted face, f0220/f0260 retain it through the hold, f0295 begins righting without an identity cut, and f0320 is the existing continuous recovery. No camera, crop, timing or contact value changed.

## Shipping values and controls

```cpp
kTrickFlipXA16 = -32768;
kTrickFlipZA16 = 0;
kTrickFaceYawA16 = 16384;
kTrickShowoffYawA16 = 0;
```

`ZHAO_U02_TRICK_FACE_YAW_A16` is a strict signed `-16384..16384` same-binary selector. Malformed, trailing-junk, overflow, `16385` and leading-space cases all return RC 2. `FACE_YAW=0` restores the exact rejected pure-X bank output (`CRC32C 0x58219E79`); the legacy root-Z control remains separately selectable (`0x7F5BBEDD`). The selected 400-frame output is `0x0BB73CDE`, ordered-frame SHA-256 `33d6146db8413ca467e25ac641fca7227b9afc2339679759d7a20bf10ed9ae2e`, and is 400/400 byte-identical to the reviewed `+16384` ladder generation.

No numeric likeness gate was added. The source controls continuity and regression reproduction; the final-resolution pictures choose the face.

## Build and regression receipts

Clean output: `.tmp/p17-trick-face-final`.

| Binary | MD5 | SHA-256 where recorded |
|---|---|---|
| `zhao-reel-cel.exe` | `60F8AAF3C87AF9BB2B7A6D6338DCA5C6` | `4D76843E0CCABE6BAC3FD5338783525D985517E972465331594F963208E2DE99` |
| `manafold-motiongate.exe` | `3167F068B39F776DB06414994EB83C19` | `FF77C809DC8E7E3804258AA0B6CC27F6C21C9AC46F84B8357CF0C1D73DED27AF` |
| `manafold-spangate.exe` | `7B937C9990B0E71024AACFF3884E9113` | — |
| `manafold-probe.exe` | `CCD3A84C38E45ECA5E2DE85AD7EF7D5E` | — |

- `mprobe`, `mqa`, `mspan`, `meyesize`, `msmooth`: RC 0;
- all 16 `msmooth` controls: strictly attributed RC 1;
- Trick contact remains keys 78..156, deepest `-40 mm` against the unchanged accepted `-60..-5 mm` band; global clearance contract stays green;
- new selector invalid matrix: 5/5 RC 2.

A one-process 28-subject isolation render compared every ordered frame against `pass17-final2-reel-28`: **27 subjects / 11,192 frames are byte-identical**. Only Trick changes; 92/400 Trick frames outside the authored orientation interval remain identical. This is scope evidence, not the final shipping bank receipt.

## Durable evidence

- `PASS17-TRICK-FACE-REPAIR-SELECTED-ALLFRAMES.png`
- `PASS17-TRICK-FACE-REPAIR-LADDER-2X.png`
- `PASS17-TRICK-FACE-REPAIR-SELECTED-PHASES-2X.png`
- `PASS17-TRICK-FACE-REPAIR-SELECTED-VS-YAW0-4X.png`
- `PASS17-TRICK-FACE-REPAIR-SELECTED-VS-LEGACY-NATIVE.png`

All other candidate sheets and `.tmp` frames are exploratory and remain unstaged.

## Files changed

- `tools/reel/manafold_art.h`
- `tools/reel/manafold_clips.h`
- `tools/reel/zhao_reel.cpp`
- reports/journal named in the repair handoff

No contact, camera, clip timing, root-height, antenna, eye form, effect, gate threshold or non-Trick art value changed.

## Final integration

The exact v3 renderer `67DCAFF8ABC0AA2BA830C439A4CD98C7` produced one
canonical 28-subject bank. Twenty-seven subjects / 11,192 frames are exact to v2,
and current Trick is exact to the selected candidate. Fresh isolated review of all
400 current Trick frames accepts the readable face, antenna support, contact and
continuous recovery. `PASS17-FINAL3-TRICK-REVIEW.md` promotes the exact v3 bank
to **PASS 28/28** and authorizes it for encoding.
