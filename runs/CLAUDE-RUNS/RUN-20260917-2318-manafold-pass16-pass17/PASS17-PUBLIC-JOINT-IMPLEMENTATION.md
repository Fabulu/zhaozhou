# Manafold Pass 17 public five-carrier instrument

**Scope:** production public mute path, visible-skin gate, build registration and stale architecture prose only

## Implemented

- `PublicJointMute {None,F,A,B,C,E}` defaults to None.
- `swallow_nodules` copies the real public five-entry input and zeroes only the selected carrier at the production consumption point. The caller's array remains untouched, so `swallow_body`, root/deformation, lighting/effects and the other four authored inputs retain shipping values.
- `ZHAO_U02_JOINT_MUTE=none|F|A|B|C|E` selects the same-binary render control; invalid values return 2 loudly.
- New `manafold_public_jointgate.cpp` / `mjointpub` target is registered in direct and CMake builds.
- The gate builds the real Taunt and Taunt-II clips with production builders, proves the unmuted direct build equals the shipping bank, selects actual rigid visible-core skin vertices, and compares same-frame normal/muted skinned positions.
- A/B/C gate visible-core centroid plus max-vertex change. Front/End gate max-vertex orientation change because their centres intentionally remain attached.
- The input positive control proves one mute changes exactly one of five authored inputs. Root/deform/body/eye channels remain byte-identical.
- Stale rig/model/meshcheck/nodule prose now describes the actual 16-bone/five-carrier/Root-parented socket architecture.

## Gate calibration and current verdict

The fixed mechanism floor is **20 mm (~3 native pixels)**, chosen before seeing the measurements as the minimum public visible-skin change worth preserving. It is not a likeness threshold.

| Carrier | Taunt centroid/max mm | Taunt-II centroid/max mm | Current verdict |
|---|---:|---:|---|
| Front | 3.45 / 11.45 | 3.92 / 12.69 | **FAIL — public beat under-reads** |
| A | 133.66 / 141.40 | 152.12 / 161.38 | PASS |
| B | 138.87 / 172.71 | 158.99 / 206.39 | PASS |
| C | 134.54 / 149.27 | 154.31 / 188.23 | PASS |
| End/socket | 0.87 / 14.87 | 0.97 / 16.55 | **FAIL — public beat under-reads** |

Rigid visible-core samples are present for all five (Front 18, A 27, B 18, C 27, End 45). Rear socket owns 99 influenced vertices; final-source authored rho is `1.043` (superseding stale Pass-16 prose).

**The instrument correctly refuses the current public picture: normal RC 1 with exactly Front and End below the fixed floor.** This is the Direction-14 finding, not a reason to lower the floor or rebuild the bones. A/B/C are strongly live; Front/End require stronger named public rotational art and final-resolution A/B review in the authored packet.

## Fired red legs

Each independent candidate-mute invocation returns RC 0 only after its own public visible-skin check fails:

- `--fail-mute F`: RC 0, attributed Front red leg fires;
- `--fail-mute A`: RC 0, attributed A red leg fires;
- `--fail-mute B`: RC 0, attributed B red leg fires;
- `--fail-mute C`: RC 0, attributed C red leg fires;
- `--fail-mute E`: RC 0, attributed End red leg fires.

The red-leg implementation explicitly checks the named carrier's verdict; it cannot pass merely because the current Front/End baseline is red.

## Build and remaining acceptance

Both clean direct and CMake `manafold-public-jointgate` builds succeed. Invalid `ZHAO_U02_JOINT_MUTE` returns 2 before rendering. The new normal gate is intentionally red until the authored-expression wave strengthens Front/End public rotations. Existing fresh `mnodule`, `mspan` and `mprobe` baselines were green before this instrument and remain protected; final authored acceptance reruns them plus `mmeshcheck`.

Same-binary normal plus F/A/B/C/E Taunt and Taunt-II renders all returned 0. Unset/default output remains exact final4: Taunt CRC `0x86C357D2`, Taunt-II CRC `0x7431CB1B`. Committed-tool plates `PASS17-PUBLIC-JOINT-TAUNT-AB.png` and `PASS17-PUBLIC-JOINT-TAUNT2-AB.png` corroborate the gate: A/B/C removals visibly change their swell/span configurations; Front and End shipping/mute pairs are barely distinguishable at native scale.

**Wave-2 disposition:** commit/push the instrument while honestly red on Front/End. The next authored packet strengthens only named Front/End public rotations and must turn the unchanged 20 mm gate green before eye/full-bank work. The instrument found the art fault it was commissioned to see; lowering its floor would erase Direction 14.
